#include "mapscriptwindow.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <utility>

#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QModelIndexList>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QScrollArea>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <QSettings>
#include <QCoreApplication>
#include <QTimer>

#include "ientity.h"
#include "map.h"
#include "preferences.h"
#include "scenelib.h"
#include "mainframe.h"
#include "generic/callback.h"
#include "signal/signal.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "debugging/debugging.h"

namespace
{
struct ScriptLink
{
	std::string sourceTrigger;
	std::string targetEntity;
	std::string targetTrigger;
};

struct ScriptData
{
	std::unordered_map<std::string, std::vector<std::string>> triggersByScript;
	std::unordered_map<std::string, std::vector<ScriptLink>> linksByScript;
};

struct EntityInfo
{
	std::string classname;
	std::string scriptname;
	std::string name;
	bool aiScript;
};

struct BlockEditorState
{
	struct ActionRow
	{
		QString action;
		QString args;
	};

	EntityInfo info;
	QString filePath;
	QString fileText;
	int blockStart = -1;
	int blockOpenBrace = -1;
	int blockCloseBrace = -1;
	QVBoxLayout* blocksLayout = nullptr;
	QHash<QString, QTableWidget*> blockTables;
	QLabel* statusLabel = nullptr;
	std::vector<QString> blockOrder;
	QHash<QString, QString> blockBodies;
	bool updatingUi = false;
};

class MapScriptsWindow
{
public:
	QDockWidget* dock = nullptr;
	QTreeWidget* entitiesTree = nullptr;
	QTabWidget* tabs = nullptr;
	QLineEdit* filterEdit = nullptr;
	QSplitter* splitter = nullptr;
	QString loadedMapName;

	std::vector<EntityInfo> entities;
	ScriptData aiData;
	ScriptData moverData;
};

MapScriptsWindow g_mapScripts;

class ScriptComboBox : public QComboBox
{
public:
	using QComboBox::QComboBox;

protected:
	void wheelEvent( QWheelEvent* event ) override {
		if( !view()->isVisible() ){
			event->ignore();
			return;
		}
		QComboBox::wheelEvent( event );
	}
};

std::string trim_copy( const std::string& value );

std::string first_non_empty_key( Entity* entity, std::initializer_list<const char*> keys ){
	for( const char* key : keys ){
		const std::string value = trim_copy( entity->getKeyValue( key ) );
		if( !value.empty() )
			return value;
	}
	return {};
}

bool string_starts_with( const std::string& value, const char* prefix ){
	if( value.size() < std::strlen( prefix ) )
		return false;
	return std::equal( prefix, prefix + std::strlen( prefix ), value.begin() );
}

std::string trim_copy( const std::string& value ){
	auto first = std::find_if_not( value.begin(), value.end(), []( unsigned char c ){ return std::isspace( c ); } );
	auto last = std::find_if_not( value.rbegin(), value.rend(), []( unsigned char c ){ return std::isspace( c ); } ).base();
	if( first >= last )
		return {};
	return std::string( first, last );
}

void append_line_without_comments( std::string& dst, const char* line ){
	const char* comment = std::strstr( line, "//" );
	if( comment != nullptr ){
		dst.append( line, static_cast<std::size_t>( comment - line ) );
	}
	else{
		dst += line;
	}
	dst += '\n';
}

std::vector<std::string> tokenise_script( const std::string& text ){
	std::vector<std::string> tokens;
	std::string token;
	for( char c : text ){
		if( std::isalnum( static_cast<unsigned char>( c ) ) || c == '_' ){
			token += c;
			continue;
		}
		if( !token.empty() ){
			tokens.push_back( token );
			token.clear();
		}
		if( c == '{' || c == '}' ){
			tokens.emplace_back( 1, c );
		}
	}
	if( !token.empty() ){
		tokens.push_back( token );
	}
	return tokens;
}

bool read_file_to_string( const char* path, std::string& out ){
	FILE* file = std::fopen( path, "rb" );
	if( file == nullptr )
		return false;

	std::fseek( file, 0, SEEK_END );
	const long size = std::ftell( file );
	std::fseek( file, 0, SEEK_SET );
	if( size <= 0 ){
		std::fclose( file );
		out.clear();
		return true;
	}

	out.resize( static_cast<std::size_t>( size ) );
	const std::size_t read = std::fread( out.data(), 1, out.size(), file );
	std::fclose( file );
	out.resize( read );
	return true;
}

ScriptData parse_map_script_file( const char* path ){
	ScriptData out;
	std::string raw;
	if( !read_file_to_string( path, raw ) )
		return out;

	std::string cleaned;
	cleaned.reserve( raw.size() );
	std::string line;
	line.reserve( 256 );
	for( char c : raw ){
		if( c == '\r' )
			continue;
		if( c == '\n' ){
			append_line_without_comments( cleaned, line.c_str() );
			line.clear();
		}
		else{
			line += c;
		}
	}
	if( !line.empty() ){
		append_line_without_comments( cleaned, line.c_str() );
	}

	const std::vector<std::string> tokens = tokenise_script( cleaned );

	std::size_t i = 0;
	while( i + 1 < tokens.size() ){
		const std::string& scriptName = tokens[i];
		if( tokens[i + 1] != "{" ){
			++i;
			continue;
		}
		i += 2;

		int scriptDepth = 1;
		while( i < tokens.size() && scriptDepth > 0 ){
			if( tokens[i] == "}" ){
				--scriptDepth;
				++i;
				continue;
			}
			if( tokens[i] == "{" ){
				++scriptDepth;
				++i;
				continue;
			}

			if( scriptDepth == 1 && tokens[i] == "trigger" && i + 2 < tokens.size() && tokens[i + 2] == "{" ){
				const std::string sourceTrigger = tokens[i + 1];
				auto& sourceTriggers = out.triggersByScript[scriptName];
				if( std::find( sourceTriggers.begin(), sourceTriggers.end(), sourceTrigger ) == sourceTriggers.end() ){
					sourceTriggers.push_back( sourceTrigger );
				}

				i += 3;
				int triggerDepth = 1;
				while( i < tokens.size() && triggerDepth > 0 ){
					if( tokens[i] == "}" ){
						--triggerDepth;
						++i;
						continue;
					}
					if( tokens[i] == "{" ){
						++triggerDepth;
						++i;
						continue;
					}

					if( tokens[i] == "trigger" && i + 2 < tokens.size() && tokens[i + 1] != "{" && tokens[i + 2] != "{" ){
						out.linksByScript[scriptName].push_back( { sourceTrigger, tokens[i + 1], tokens[i + 2] } );
						i += 3;
						continue;
					}
					++i;
				}
				continue;
			}
			++i;
		}
	}

	return out;
}

class ScriptEntityCollector : public scene::Graph::Walker
{
	std::vector<EntityInfo>& m_out;
	mutable std::unordered_set<const Entity*> m_seen;
public:
	explicit ScriptEntityCollector( std::vector<EntityInfo>& out ) : m_out( out ){
	}

	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		( void )instance;
		Entity* entity = Node_getEntity( path.top() );
		if( entity == nullptr || !m_seen.insert( entity ).second )
			return true;

		const std::string classname = entity->getClassName();
		const bool aiScript = string_starts_with( classname, "ai_" );
		const bool moverScript = classname == "script_mover";
		if( !aiScript && !moverScript )
			return true;
		if( classname == "ai_marker" || classname == "ai_trigger" )
			return true;

		const std::string scriptname = aiScript
			? first_non_empty_key( entity, { "ainame", "aiName", "scriptname", "scriptName", "name", "targetname" } )
			: first_non_empty_key( entity, { "scriptname", "scriptName", "name", "targetname" } );
		if( scriptname.empty() )
			return true;

		EntityInfo info;
		info.classname = classname;
		info.scriptname = scriptname;
		info.name = trim_copy( entity->getKeyValue( "name" ) );
		info.aiScript = aiScript;
		m_out.push_back( std::move( info ) );
		return true;
	}
};

std::string map_script_path_for_extension( const char* extension ){
	if( Map_Unnamed( g_map ) )
		return {};

	const char* mapName = Map_Name( g_map );

	{
		const auto path = StringStream( PathExtensionless( mapName ), extension );
		if( file_exists( path.c_str() ) )
			return path.c_str();
	}

	if( !path_is_absolute( mapName ) ){
		const auto pathFromEngine = StringStream( DirectoryCleaned( EnginePath_get() ), PathExtensionless( mapName ), extension );
		if( file_exists( pathFromEngine.c_str() ) )
			return pathFromEngine.c_str();

		const auto pathFromMaps = StringStream( DirectoryCleaned( getMapsPath() ), PathFilename( mapName ), extension );
		if( file_exists( pathFromMaps.c_str() ) )
			return pathFromMaps.c_str();
	}

	return {};
}

QString script_file_path_for_entity( const EntityInfo& info ){
	const char* ext = info.aiScript ? ".ai" : ".script";
	if( const std::string existing = map_script_path_for_extension( ext ); !existing.empty() )
		return QString::fromLatin1( existing.c_str() );

	const char* mapName = Map_Name( g_map );
	if( path_is_absolute( mapName ) ){
		const auto local = StringStream( PathExtensionless( mapName ), ext );
		return QString::fromLatin1( local.c_str() );
	}

	const auto inMaps = StringStream( DirectoryCleaned( getMapsPath() ), PathFilename( mapName ), ext );
	return QString::fromLatin1( inMaps.c_str() );
}

bool find_script_entity_block( const QString& text, const QString& scriptname, int& blockStart, int& openBrace, int& closeBrace ){
	const int nameLen = scriptname.size();
	for( int pos = 0;; ){
		pos = text.indexOf( scriptname, pos );
		if( pos < 0 )
			return false;

		const bool leftOk = pos == 0 || !( text[pos - 1].isLetterOrNumber() || text[pos - 1] == '_' );
		const int after = pos + nameLen;
		const bool rightOk = after >= text.size() || !( text[after].isLetterOrNumber() || text[after] == '_' );
		if( !leftOk || !rightOk ){
			pos = after;
			continue;
		}

		const int lineStartRaw = text.lastIndexOf( '\n', pos );
		const int lineStart = lineStartRaw < 0 ? 0 : lineStartRaw + 1;
		if( !text.mid( lineStart, pos - lineStart ).trimmed().isEmpty() ){
			pos = after;
			continue;
		}

		const int brace = text.indexOf( '{', after );
		if( brace < 0 )
			return false;

		int depth = 0;
		for( int i = brace; i < text.size(); ++i ){
			if( text[i] == '{' )
				++depth;
			else if( text[i] == '}' ){
				--depth;
				if( depth == 0 ){
					blockStart = lineStart;
					openBrace = brace;
					closeBrace = i;
					return true;
				}
			}
		}
		return false;
	}
}

void parse_entity_blocks( BlockEditorState& state ){
	state.blockOrder.clear();
	state.blockBodies.clear();

	if( state.blockOpenBrace < 0 || state.blockCloseBrace < 0 )
		return;

	const QString& text = state.fileText;
	const int to = state.blockCloseBrace;
	int pos = state.blockOpenBrace + 1;

	while( pos < to ){
		while( pos < to && text[pos].isSpace() )
			++pos;
		if( pos >= to )
			break;

		const int open = text.indexOf( '{', pos );
		if( open < 0 || open >= to )
			break;

		const QString header = text.mid( pos, open - pos ).trimmed();
		if( header.isEmpty() ){
			pos = open + 1;
			continue;
		}

		int depth = 1;
		int close = open + 1;
		for( ; close < to; ++close ){
			if( text[close] == '{' )
				++depth;
			else if( text[close] == '}' ){
				--depth;
				if( depth == 0 )
					break;
			}
		}
		if( close >= to )
			break;

		QString body = text.mid( open + 1, close - open - 1 );
		if( body.startsWith( '\n' ) )
			body.remove( 0, 1 );
		if( body.endsWith( '\n' ) )
			body.chop( 1 );

		state.blockOrder.push_back( header );
		state.blockBodies.insert( header, body );

		pos = close + 1;
	}
}

std::vector<BlockEditorState::ActionRow> parse_actions_from_body( const QString& body ){
	std::vector<BlockEditorState::ActionRow> out;
	const QStringList lines = body.split( '\n', Qt::KeepEmptyParts );
	for( QString line : lines ){
		const int commentPos = line.indexOf( "//" );
		if( commentPos >= 0 )
			line = line.left( commentPos );
		line = line.trimmed();
		if( line.isEmpty() )
			continue;

		int split = 0;
		while( split < line.size() && !line[split].isSpace() )
			++split;

		BlockEditorState::ActionRow row;
		row.action = line.left( split ).trimmed();
		row.args = line.mid( split ).trimmed();
		out.push_back( std::move( row ) );
	}
	return out;
}

struct ActionSpec
{
	QString name;
	QString argLabels[4];
	int argCount = 0;
};

std::vector<ActionSpec> action_specs( bool aiScript ){
	std::vector<ActionSpec> specs = {
		{ "wait", { "duration", "moverange", "facetarget", "" }, 3 },
		{ "trigger", { "target", "event", "", "" }, 2 },
		{ "alertentity", { "targetname", "", "", "" }, 1 },
		{ "accum", { "buffer", "command", "parameter", "" }, 3 },
		{ "playsound", { "sound/script", "option", "", "" }, 2 },
		{ "playanim", { "arg1", "arg2", "arg3", "arg4" }, 4 },
		{ "gotomarker", { "target", "firetarget", "noattack", "nostop" }, 4 },
		{ "runtomarker", { "target", "firetarget", "noattack", "nostop" }, 4 },
		{ "walktomarker", { "target", "firetarget", "noattack", "nostop" }, 4 },
		{ "crouchtomarker", { "target", "firetarget", "noattack", "nostop" }, 4 },
		{ "gotocast", { "ainame", "firetarget", "noattack", "" }, 3 },
		{ "runtocast", { "ainame", "firetarget", "noattack", "" }, 3 },
		{ "walktocast", { "ainame", "firetarget", "noattack", "" }, 3 },
		{ "crouchtocast", { "ainame", "firetarget", "noattack", "" }, 3 },
		{ "movetype", { "walk/run/crouch/default", "", "", "" }, 1 },
		{ "fireattarget", { "target", "duration", "", "" }, 2 },
		{ "attrib", { "attribute", "value", "", "" }, 2 },
		{ "startcam", { "camera_file", "", "", "" }, 1 },
		{ "startcamblack", { "camera_file", "", "", "" }, 1 },
		{ "mu_start", { "wav", "fade_time", "", "" }, 2 },
		{ "mu_fade", { "wav", "fade_time", "", "" }, 2 },
		{ "mu_queue", { "wav", "", "", "" }, 1 },
		{ "changelevel", { "mapname", "nostats", "persistant", "" }, 3 },
		{ "objectivemet", { "objective_index", "", "", "" }, 1 },
		{ "objectiveneeded", { "objective_count", "", "", "" }, 1 },
		{ "teleport", { "targetname", "", "", "" }, 1 },
		{ "setammo", { "pickup", "count", "", "" }, 2 },
		{ "selectweapon", { "pickup", "", "", "" }, 1 },
		{ "giveweapon", { "pickup", "", "", "" }, 1 },
		{ "takeweapon", { "pickup", "", "", "" }, 1 },
		{ "print", { "text", "", "", "" }, 1 },
	};

	if( !aiScript ){
		specs.push_back( { "gotomarker", { "targetname", "speed", "accel/deccel", "turntotarget/wait" }, 4 } );
	}
	return specs;
}

const ActionSpec* find_action_spec( const std::vector<ActionSpec>& specs, const QString& name ){
	for( const auto& spec : specs ){
		if( spec.name.compare( name, Qt::CaseInsensitive ) == 0 )
			return &spec;
	}
	return nullptr;
}

QStringList split_args_tokens( const QString& args ){
	return args.split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
}

QStringList collect_all_script_entities(){
	QStringList names;
	for( const EntityInfo& info : g_mapScripts.entities ){
		names.push_back( QString::fromLatin1( info.scriptname.c_str() ) );
	}
	names.removeDuplicates();
	names.sort( Qt::CaseInsensitive );
	return names;
}

QStringList triggers_for_entity_name( const QString& entity ){
	QStringList out;
	const auto addFrom = [&]( const ScriptData& data ){
		const auto it = data.triggersByScript.find( entity.toLatin1().constData() );
		if( it == data.triggersByScript.end() )
			return;
		for( const auto& t : it->second ){
			out.push_back( QString::fromLatin1( t.c_str() ) );
		}
	};
	addFrom( g_mapScripts.aiData );
	addFrom( g_mapScripts.moverData );
	out.removeDuplicates();
	out.sort( Qt::CaseInsensitive );
	return out;
}

void set_combo_items_keep_value( QComboBox* combo, const QStringList& items ){
	const QString current = combo->currentText();
	const QSignalBlocker blocker( combo );
	combo->clear();
	combo->addItems( items );
	combo->setCurrentText( current );
}

void configure_action_arg_options( const QString& actionRaw, QComboBox* const args[4] ){
	const QString action = actionRaw.trimmed().toLower();
	if( action == "trigger" ){
		set_combo_items_keep_value( args[0], collect_all_script_entities() );
		set_combo_items_keep_value( args[1], triggers_for_entity_name( args[0]->currentText() ) );
	}
	else if( action == "accum" ){
		set_combo_items_keep_value( args[1], {
			"set", "inc", "random",
			"bitset", "bitreset",
			"abort_if_less_than", "abort_if_greater_than",
			"abort_if_equal", "abort_if_not_equal",
			"abort_if_bitset", "abort_if_not_bitset"
		} );
	}
	else if( action == "movetype" ){
		set_combo_items_keep_value( args[0], { "walk", "run", "crouch", "default" } );
	}
	else if( action == "headlook" || action == "godmode" || action == "noaidamage" || action == "notarget"
	      || action == "zoom" || action == "parachute" || action == "cigarette" || action == "lockplayer" ){
		set_combo_items_keep_value( args[0], { "on", "off" } );
	}
}

QString render_actions_to_body( const std::vector<BlockEditorState::ActionRow>& actions ){
	QStringList lines;
	for( const auto& row : actions ){
		if( row.action.trimmed().isEmpty() )
			continue;
		lines.push_back( row.args.trimmed().isEmpty()
			? row.action.trimmed()
			: row.action.trimmed() + " " + row.args.trimmed() );
	}
	return lines.join( '\n' );
}

std::vector<BlockEditorState::ActionRow> read_actions_from_table( QTableWidget* table ){
	std::vector<BlockEditorState::ActionRow> out;
	for( int row = 0; row < table->rowCount(); ++row ){
		auto* actionCombo = qobject_cast<QComboBox*>( table->cellWidget( row, 0 ) );
		BlockEditorState::ActionRow a;
		a.action = actionCombo ? actionCombo->currentText().trimmed() : QString();
		QStringList args;
		for( int col = 1; col <= 4; ++col ){
			auto* argCombo = qobject_cast<QComboBox*>( table->cellWidget( row, col ) );
			if( argCombo == nullptr || !argCombo->isEnabled() )
				continue;
			const QString v = argCombo->currentText().trimmed();
			if( !v.isEmpty() )
				args.push_back( v );
		}
		a.args = args.join( ' ' );
		if( a.action.isEmpty() && a.args.isEmpty() )
			continue;
		out.push_back( std::move( a ) );
	}
	return out;
}

void ensure_default_blocks( BlockEditorState& state );
void fit_table_height_to_rows( QTableWidget* table );
void ensure_trailing_blank_action_row( QTableWidget* table, bool aiScript );

void load_block_editor_state( BlockEditorState& state ){
	state.filePath = script_file_path_for_entity( state.info );
	state.fileText.clear();
	state.blockStart = state.blockOpenBrace = state.blockCloseBrace = -1;

	std::string raw;
	if( !state.filePath.isEmpty() && read_file_to_string( state.filePath.toLatin1().constData(), raw ) ){
		state.fileText = QString::fromUtf8( raw.c_str(), static_cast<int>( raw.size() ) );
	}

	find_script_entity_block(
		state.fileText,
		QString::fromLatin1( state.info.scriptname.c_str() ),
		state.blockStart, state.blockOpenBrace, state.blockCloseBrace );
	parse_entity_blocks( state );
	ensure_default_blocks( state );
}

QString render_entity_block_text( const BlockEditorState& state ){
	QString out = QString::fromLatin1( state.info.scriptname.c_str() ) + "\n{\n";
	for( const QString& header : state.blockOrder ){
		out += "\t" + header + "\n";
		out += "\t{\n";
		const QString body = state.blockBodies.value( header );
		const QStringList lines = body.split( '\n', Qt::KeepEmptyParts );
		for( const QString& line : lines ){
			if( lines.size() == 1 && line.isEmpty() )
				break;
			out += "\t\t" + line + "\n";
		}
		out += "\t}\n\n";
	}
	out += "}\n";
	return out;
}

bool save_block_editor_state( BlockEditorState& state ){
	QString newFileText = state.fileText;
	const QString rendered = render_entity_block_text( state );

	if( state.blockStart >= 0 && state.blockCloseBrace >= state.blockStart ){
		newFileText.replace( state.blockStart, state.blockCloseBrace - state.blockStart + 1, rendered );
	}
	else{
		if( !newFileText.isEmpty() && !newFileText.endsWith( '\n' ) )
			newFileText += '\n';
		newFileText += '\n' + rendered;
	}

	QFile file( state.filePath );
	if( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return false;
	const QByteArray bytes = newFileText.toUtf8();
	if( file.write( bytes ) != bytes.size() )
		return false;
	file.close();

	state.fileText = newFileText;
	find_script_entity_block(
		state.fileText,
		QString::fromLatin1( state.info.scriptname.c_str() ),
		state.blockStart, state.blockOpenBrace, state.blockCloseBrace );
	return true;
}

void gather_entities(){
	g_mapScripts.entities.clear();
	if( !Map_Valid( g_map ) )
		return;

	GlobalSceneGraph().traverse( ScriptEntityCollector( g_mapScripts.entities ) );

	const bool hasPlayer = std::any_of( g_mapScripts.entities.begin(), g_mapScripts.entities.end(), []( const EntityInfo& info ){
		return info.scriptname == "player";
	} );
	if( !hasPlayer ){
		g_mapScripts.entities.push_back( { "player", "player", "player", true } );
	}

	std::sort( g_mapScripts.entities.begin(), g_mapScripts.entities.end(), []( const EntityInfo& a, const EntityInfo& b ){
		const bool aPlayer = a.scriptname == "player";
		const bool bPlayer = b.scriptname == "player";
		if( aPlayer != bPlayer )
			return aPlayer;
		if( a.classname != b.classname )
			return a.classname < b.classname;
		if( a.name != b.name )
			return a.name < b.name;
		return a.scriptname < b.scriptname;
	} );
}

QString entity_display_name( const EntityInfo& info ){
	const QString title = info.name.empty()
		? QString::fromLatin1( info.scriptname.c_str() )
		: QString::fromLatin1( info.name.c_str() );
	return QStringLiteral( "%1 (%2) [%3]" )
		.arg( title, QString::fromLatin1( info.classname.c_str() ), QString::fromLatin1( info.scriptname.c_str() ) );
}

void refresh_scripts_data();

QString current_map_name(){
	if( !Map_Valid( g_map ) )
		return {};
	const char* name = Map_Name( g_map );
	return name != nullptr ? QString::fromLatin1( name ) : QString();
}

void close_all_editor_tabs(){
	if( g_mapScripts.tabs == nullptr )
		return;
	while( g_mapScripts.tabs->count() > 0 ){
		delete g_mapScripts.tabs->widget( 0 );
	}
}

QSettings mapscripts_settings(){
	return QSettings( QCoreApplication::organizationName(), QCoreApplication::applicationName() );
}

void save_mapscripts_layout( QMainWindow* main_window ){
	if( g_mapScripts.dock == nullptr )
		return;

	QSettings settings = mapscripts_settings();
	settings.setValue( "MapScripts/geometry", g_mapScripts.dock->saveGeometry() );
	settings.setValue( "MapScripts/floating", g_mapScripts.dock->isFloating() );
	settings.setValue( "MapScripts/visible", g_mapScripts.dock->isVisible() );
	settings.setValue( "MapScripts/dockSize", g_mapScripts.dock->size() );
	if( main_window != nullptr ){
		settings.setValue( "MapScripts/area", static_cast<int>( main_window->dockWidgetArea( g_mapScripts.dock ) ) );
	}
	if( g_mapScripts.splitter != nullptr ){
		settings.setValue( "MapScripts/splitterState", g_mapScripts.splitter->saveState() );
	}
}

void apply_mapscripts_dock_size( QMainWindow* main_window ){
	if( main_window == nullptr || g_mapScripts.dock == nullptr || g_mapScripts.dock->isFloating() )
		return;

	QSettings settings = mapscripts_settings();
	const QSize saved = settings.value( "MapScripts/dockSize", QSize() ).toSize();
	if( !saved.isValid() )
		return;

	const Qt::DockWidgetArea area = main_window->dockWidgetArea( g_mapScripts.dock );
	if( area == Qt::DockWidgetArea::LeftDockWidgetArea || area == Qt::DockWidgetArea::RightDockWidgetArea ){
		main_window->resizeDocks( { g_mapScripts.dock }, { saved.width() }, Qt::Horizontal );
	}
	else if( area == Qt::DockWidgetArea::TopDockWidgetArea || area == Qt::DockWidgetArea::BottomDockWidgetArea ){
		main_window->resizeDocks( { g_mapScripts.dock }, { saved.height() }, Qt::Vertical );
	}
}

void restore_mapscripts_layout( QMainWindow* main_window ){
	if( g_mapScripts.dock == nullptr )
		return;

	QSettings settings = mapscripts_settings();
	if( const QByteArray splitterState = settings.value( "MapScripts/splitterState", QByteArray() ).toByteArray(); !splitterState.isEmpty() ){
		if( g_mapScripts.splitter != nullptr ){
			g_mapScripts.splitter->restoreState( splitterState );
		}
	}

	const Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(
		settings.value( "MapScripts/area", static_cast<int>( Qt::DockWidgetArea::RightDockWidgetArea ) ).toInt() );
	if( main_window != nullptr ){
		main_window->addDockWidget( area, g_mapScripts.dock );
	}

	if( const QByteArray geometry = settings.value( "MapScripts/geometry", QByteArray() ).toByteArray(); !geometry.isEmpty() ){
		g_mapScripts.dock->restoreGeometry( geometry );
	}

	g_mapScripts.dock->setFloating( settings.value( "MapScripts/floating", false ).toBool() );
	g_mapScripts.dock->setVisible( settings.value( "MapScripts/visible", false ).toBool() );
	QTimer::singleShot( 0, [main_window](){
		apply_mapscripts_dock_size( main_window );
	} );
}

QStringList action_names_for_script( bool aiScript ){
	QStringList out;
	for( const auto& spec : action_specs( aiScript ) ){
		out.push_back( spec.name );
	}
	out.removeDuplicates();
	out.sort( Qt::CaseInsensitive );
	return out;
}

bool is_attributes_block_header( const QString& header ){
	const QString first = header.trimmed().section( QRegularExpression( "\\s+" ), 0, 0 );
	return first.compare( "attributes", Qt::CaseInsensitive ) == 0;
}

bool is_spawn_block_header( const QString& header ){
	const QString first = header.trimmed().section( QRegularExpression( "\\s+" ), 0, 0 );
	return first.compare( "spawn", Qt::CaseInsensitive ) == 0;
}

bool has_block_keyword( const BlockEditorState& state, const QString& keyword ){
	for( const QString& header : state.blockOrder ){
		const QString first = header.trimmed().section( QRegularExpression( "\\s+" ), 0, 0 );
		if( first.compare( keyword, Qt::CaseInsensitive ) == 0 ){
			return true;
		}
	}
	return false;
}

void ensure_default_blocks( BlockEditorState& state ){
	if( state.info.aiScript && !has_block_keyword( state, "attributes" ) ){
		state.blockOrder.insert( state.blockOrder.begin(), "attributes" );
		state.blockBodies.insert( "attributes", "" );
	}

	if( !has_block_keyword( state, "spawn" ) ){
		auto insertPos = state.blockOrder.begin();
		if( state.info.aiScript && !state.blockOrder.empty()
			&& state.blockOrder.front().trimmed().compare( "attributes", Qt::CaseInsensitive ) == 0 ){
			++insertPos;
		}
		state.blockOrder.insert( insertPos, "spawn" );
		state.blockBodies.insert( "spawn", "" );
	}
}

std::vector<std::pair<QString, QString>> ai_attribute_defaults(){
	QStringList out = {
		"running_speed 220",
		"walking_speed 90",
		"crouching_speed 80",
		"fov 240",
		"yaw_speed 300",
		"leader 0",
		"aggression 0.5",
		"starting_health 100",
		"hearing_scale 1.0",
		"inner_detection_radius 512",
		"alertness 16000",
		"reaction_time 0.5",
		"attack_skill 0.75",
		"aim_skill 0.5",
		"aim_accuracy 0.5",
		"attack_crouch 0.4",
		"idle_crouch 0.0"
	};
	std::vector<std::pair<QString, QString>> defaults;
	defaults.reserve( out.size() );
	for( const QString& row : out ){
		const int split = row.indexOf( ' ' );
		if( split <= 0 )
			continue;
		defaults.emplace_back( row.left( split ), row.mid( split + 1 ).trimmed() );
	}
	return defaults;
}

void configure_inline_row_for_action( bool aiScript, QComboBox* actionCombo, QComboBox* const args[4] ){
	const QString actionName = actionCombo->currentText().trimmed();
	const auto specs = action_specs( aiScript );
	const ActionSpec* spec = find_action_spec( specs, actionName );
	const int argCount = spec != nullptr ? spec->argCount : 4;

	for( int i = 0; i < 4; ++i ){
		const bool enabled = i < argCount;
		args[i]->setEnabled( enabled );
		if( !enabled ){
			args[i]->setCurrentText( "" );
		}
	}

	configure_action_arg_options( actionName, args );
}

void add_action_row_inline( QTableWidget* table, bool aiScript, const BlockEditorState::ActionRow* initial ){
	const int row = table->rowCount();
	table->insertRow( row );

	auto* actionCombo = new ScriptComboBox;
	actionCombo->setEditable( true );
	actionCombo->addItems( action_names_for_script( aiScript ) );
	table->setCellWidget( row, 0, actionCombo );

	QComboBox* argCombos[4] = {};
	for( int i = 0; i < 4; ++i ){
		auto* combo = new ScriptComboBox;
		combo->setEditable( true );
		table->setCellWidget( row, i + 1, combo );
		argCombos[i] = combo;
	}
	QComboBox* arg0 = argCombos[0];
	QComboBox* arg1 = argCombos[1];
	QComboBox* arg2 = argCombos[2];
	QComboBox* arg3 = argCombos[3];

	if( initial != nullptr ){
		actionCombo->setCurrentText( initial->action );
		const QStringList tokens = split_args_tokens( initial->args );
		for( int i = 0; i < 4 && i < tokens.size(); ++i ){
			argCombos[i]->setCurrentText( tokens[i] );
		}
	}
	else{
		actionCombo->setCurrentText( "wait" );
		argCombos[0]->setCurrentText( "1000" );
	}

	configure_inline_row_for_action( aiScript, actionCombo, argCombos );

	QObject::connect( actionCombo, &QComboBox::currentTextChanged, [table, aiScript, actionCombo, arg0, arg1, arg2, arg3]( const QString& ){
		QComboBox* args[4] = { arg0, arg1, arg2, arg3 };
		configure_inline_row_for_action( aiScript, actionCombo, args );
		ensure_trailing_blank_action_row( table, aiScript );
	} );
	QObject::connect( arg0, &QComboBox::currentTextChanged, [table, aiScript, actionCombo, arg0, arg1, arg2, arg3]( const QString& ){
		if( actionCombo->currentText().trimmed().compare( "trigger", Qt::CaseInsensitive ) == 0 ){
			QComboBox* args[4] = { arg0, arg1, arg2, arg3 };
			configure_action_arg_options( actionCombo->currentText(), args );
		}
		ensure_trailing_blank_action_row( table, aiScript );
	} );

	auto* removeButton = new QPushButton( "X" );
	removeButton->setToolTip( "Remove this action" );
	removeButton->setMaximumWidth( 28 );
	table->setCellWidget( row, 5, removeButton );
	QObject::connect( removeButton, &QPushButton::clicked, [table, aiScript, removeButton](){
		for( int r = 0; r < table->rowCount(); ++r ){
			if( table->cellWidget( r, 5 ) == removeButton ){
				table->removeRow( r );
				ensure_trailing_blank_action_row( table, aiScript );
				fit_table_height_to_rows( table );
				break;
			}
		}
	} );
}

bool action_row_is_empty( QTableWidget* table, int row ){
	auto* actionCombo = qobject_cast<QComboBox*>( table->cellWidget( row, 0 ) );
	if( actionCombo != nullptr && !actionCombo->currentText().trimmed().isEmpty() )
		return false;
	for( int col = 1; col <= 4; ++col ){
		auto* argCombo = qobject_cast<QComboBox*>( table->cellWidget( row, col ) );
		if( argCombo != nullptr && argCombo->isEnabled() && !argCombo->currentText().trimmed().isEmpty() )
			return false;
	}
	return true;
}

void ensure_trailing_blank_action_row( QTableWidget* table, bool aiScript ){
	if( table == nullptr || table->columnCount() < 6 )
		return;
	if( table->rowCount() == 0 ){
		BlockEditorState::ActionRow blank;
		add_action_row_inline( table, aiScript, &blank );
		return;
	}
	const int last = table->rowCount() - 1;
	if( !action_row_is_empty( table, last ) ){
		BlockEditorState::ActionRow blank;
		add_action_row_inline( table, aiScript, &blank );
	}
}

void add_attribute_row_inline( QTableWidget* table, const BlockEditorState::ActionRow* initial ){
	const int row = table->rowCount();
	table->insertRow( row );

	auto* attrEdit = new QLineEdit;
	attrEdit->setReadOnly( true );
	table->setCellWidget( row, 0, attrEdit );

	auto* valueCombo = new ScriptComboBox;
	valueCombo->setEditable( true );
	table->setCellWidget( row, 1, valueCombo );

	if( initial != nullptr ){
		attrEdit->setText( initial->action );
		valueCombo->setCurrentText( initial->args );
	}
	else{
		attrEdit->setText( "running_speed" );
		valueCombo->setCurrentText( "220" );
	}
}

std::vector<BlockEditorState::ActionRow> read_attributes_from_table( QTableWidget* table ){
	QHash<QString, QString> defaults;
	for( const auto& it : ai_attribute_defaults() ){
		defaults.insert( it.first.toLower(), it.second );
	}

	std::vector<BlockEditorState::ActionRow> out;
	for( int row = 0; row < table->rowCount(); ++row ){
		auto* attrEdit = qobject_cast<QLineEdit*>( table->cellWidget( row, 0 ) );
		auto* valueCombo = qobject_cast<QComboBox*>( table->cellWidget( row, 1 ) );
		BlockEditorState::ActionRow a;
		a.action = attrEdit ? attrEdit->text().trimmed() : QString();
		a.args = valueCombo ? valueCombo->currentText().trimmed() : QString();
		if( a.action.isEmpty() )
			continue;
		const QString def = defaults.value( a.action.toLower() );
		if( a.args.isEmpty() ){
			a.args = def;
		}
		if( !def.isEmpty() && a.args.compare( def, Qt::CaseSensitive ) == 0 )
			continue;
		out.push_back( std::move( a ) );
	}
	return out;
}

void fit_table_height_to_rows( QTableWidget* table ){
	int h = table->frameWidth() * 2;
	if( table->horizontalHeader() != nullptr && !table->horizontalHeader()->isHidden() ){
		h += table->horizontalHeader()->height();
	}
	for( int row = 0; row < table->rowCount(); ++row ){
		h += table->rowHeight( row );
	}
	if( table->horizontalScrollBar()->isVisible() ){
		h += table->horizontalScrollBar()->height();
	}
	table->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	table->setMinimumHeight( h );
	table->setMaximumHeight( h );
}

void sync_tables_to_blocks( BlockEditorState& state ){
	for( const QString& header : state.blockOrder ){
		QTableWidget* table = state.blockTables.value( header, nullptr );
		if( table == nullptr )
			continue;
		if( is_attributes_block_header( header ) ){
			state.blockBodies[header] = render_actions_to_body( read_attributes_from_table( table ) );
		}
		else{
			state.blockBodies[header] = render_actions_to_body( read_actions_from_table( table ) );
		}
	}
}

void clear_layout_widgets( QLayout* layout ){
	while( layout->count() > 0 ){
		QLayoutItem* item = layout->takeAt( 0 );
		if( item->widget() != nullptr ){
			delete item->widget();
		}
		delete item;
	}
}

void rebuild_blocks_ui( const std::shared_ptr<BlockEditorState>& state, QWidget* owner ){
	if( state->blocksLayout == nullptr )
		return;

	sync_tables_to_blocks( *state );
	state->blockTables.clear();
	clear_layout_widgets( state->blocksLayout );

	for( const QString& header : state->blockOrder ){
		const bool attributesBlock = is_attributes_block_header( header );
		auto* group = new QGroupBox( header );
		auto* groupLayout = new QVBoxLayout( group );
		QPushButton* addAction = nullptr;
		QPushButton* removeBlock = nullptr;

		if (!attributesBlock) {
			auto* rowButtons = new QHBoxLayout;
			addAction = new QPushButton( "Add trigger" );
			removeBlock = new QPushButton( "Remove trigger" );
			rowButtons->addWidget( addAction );
			rowButtons->addWidget( removeBlock );
			rowButtons->addStretch( 1 );
			groupLayout->addLayout( rowButtons );
		}
		QTableWidget* table = nullptr;
		if( attributesBlock ){
			table = new QTableWidget( 0, 2 );
			table->setHorizontalHeaderLabels( { "Attribute", "Value" } );
			table->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeMode::ResizeToContents );
			table->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeMode::Stretch );
		}
		else{
			table = new QTableWidget( 0, 6 );
			table->setHorizontalHeaderLabels( { "Action", "Arg1", "Arg2", "Arg3", "Arg4", "" } );
			table->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeMode::ResizeToContents );
			for( int col = 1; col <= 4; ++col ){
				table->horizontalHeader()->setSectionResizeMode( col, QHeaderView::ResizeMode::Stretch );
			}
			table->horizontalHeader()->setSectionResizeMode( 5, QHeaderView::ResizeMode::ResizeToContents );
		}
		table->setSelectionBehavior( QAbstractItemView::SelectionBehavior::SelectRows );
		table->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
		groupLayout->addWidget( table );
		state->blockTables.insert( header, table );

		const auto actions = parse_actions_from_body( state->blockBodies.value( header ) );
		if( attributesBlock ){
			QHash<QString, QString> overrides;
			for( const auto& action : actions ){
				overrides.insert( action.action.trimmed().toLower(), action.args.trimmed() );
			}
			for( const auto& def : ai_attribute_defaults() ){
				BlockEditorState::ActionRow row;
				row.action = def.first;
				row.args = overrides.value( def.first.toLower(), def.second );
				add_attribute_row_inline( table, &row );
			}
		}
		else{
			for( const auto& action : actions ){
				add_action_row_inline( table, state->info.aiScript, &action );
			}
			ensure_trailing_blank_action_row( table, state->info.aiScript );
		}
		fit_table_height_to_rows( table );

		if( !attributesBlock ){
			QObject::connect( addAction, &QPushButton::clicked, [state, header, attributesBlock](){
				QTableWidget* blockTable = state->blockTables.value( header, nullptr );
				if( blockTable == nullptr )
					return;
				if( attributesBlock ){
					return;
				}
				else{
					add_action_row_inline( blockTable, state->info.aiScript, nullptr );
					ensure_trailing_blank_action_row( blockTable, state->info.aiScript );
				}
				fit_table_height_to_rows( blockTable );
			} );
			QObject::connect( removeBlock, &QPushButton::clicked, [state, owner, header](){
				state->blockBodies.remove( header );
				state->blockOrder.erase(
					std::remove( state->blockOrder.begin(), state->blockOrder.end(), header ),
					state->blockOrder.end() );
				rebuild_blocks_ui( state, owner );
			} );
		}

		state->blocksLayout->addWidget( group );
	}

	state->blocksLayout->addStretch( 1 );
}

QWidget* create_editor_tab( const EntityInfo& info ){
	auto state = std::make_shared<BlockEditorState>();
	state->info = info;

	auto* widget = new QWidget;
	auto* vbox = new QVBoxLayout( widget );
	vbox->setContentsMargins( 4, 4, 4, 4 );

	{
		auto* label = new QLabel(
			QStringLiteral( "Entity: %1\nScript: %2 (%3)" )
				.arg( entity_display_name( info ),
				      QString::fromLatin1( info.scriptname.c_str() ),
				      info.aiScript ? QStringLiteral( ".ai" ) : QStringLiteral( ".script" ) ) );
		vbox->addWidget( label );
	}

	{
		auto* blockCaption = new QLabel( "Entity Blocks" );
		vbox->addWidget( blockCaption );

		auto* controls = new QHBoxLayout;
		vbox->addLayout( controls );

		auto* addBlock = new QPushButton( "Add Trigger" );
		auto* saveBlocks = new QPushButton( "Save Blocks" );
		controls->addStretch( 1 );
		controls->addWidget( addBlock );
		controls->addWidget( saveBlocks );

		auto* scroll = new QScrollArea;
		scroll->setWidgetResizable( true );
		auto* blocksHost = new QWidget;
		state->blocksLayout = new QVBoxLayout( blocksHost );
		state->blocksLayout->setContentsMargins( 0, 0, 0, 0 );
		state->blocksLayout->setSpacing( 6 );
		scroll->setWidget( blocksHost );
		vbox->addWidget( scroll, 1 );

		state->statusLabel = new QLabel;
		vbox->addWidget( state->statusLabel );

		load_block_editor_state( *state );
		rebuild_blocks_ui( state, widget );
		QObject::connect( addBlock, &QPushButton::clicked, [state, widget](){
			bool ok = false;
			const QString name = QInputDialog::getText(
				widget, "Add Trigger", "Trigger header (example: trigger myevent):",
				QLineEdit::Normal, "", &ok ).trimmed();
			if( !ok || name.isEmpty() )
				return;
			if( state->blockBodies.contains( name ) ){
				QMessageBox::warning( widget, "Map Scripts", "Trigger already exists." );
				return;
			}
			state->blockOrder.push_back( name );
			state->blockBodies.insert( name, "" );
			rebuild_blocks_ui( state, widget );
		} );
		QObject::connect( saveBlocks, &QPushButton::clicked, [state, widget](){
			sync_tables_to_blocks( *state );
			if( state->filePath.isEmpty() ){
				QMessageBox::warning( widget, "Map Scripts", "Cannot resolve .ai/.script file path." );
				return;
			}
			if( save_block_editor_state( *state ) ){
				state->statusLabel->setText( "Saved blocks to: " + state->filePath );
				refresh_scripts_data();
			}
			else{
				state->statusLabel->setText( "Failed to save: " + state->filePath );
				QMessageBox::warning( widget, "Map Scripts", "Failed to save blocks to script file." );
			}
		} );
	}

	return widget;
}

QString make_tab_key( const EntityInfo& info ){
	return QStringLiteral( "%1|%2|%3" )
		.arg( QString::fromLatin1( info.classname.c_str() ),
		      QString::fromLatin1( info.scriptname.c_str() ),
		      QString::fromLatin1( info.name.c_str() ) );
}

void open_entity_editor( const EntityInfo& info ){
	const QString key = make_tab_key( info );
	for( int i = 0; i < g_mapScripts.tabs->count(); ++i ){
		if( g_mapScripts.tabs->widget( i )->property( "mapscripts_key" ).toString() == key ){
			g_mapScripts.tabs->setCurrentIndex( i );
			return;
		}
	}

	QWidget* tab = create_editor_tab( info );
	tab->setProperty( "mapscripts_key", key );
	const QString tabLabel = info.name.empty()
		? QString::fromLatin1( info.scriptname.c_str() )
		: QString::fromLatin1( info.name.c_str() );
	const int idx = g_mapScripts.tabs->addTab( tab, tabLabel );
	g_mapScripts.tabs->setCurrentIndex( idx );
}

void refresh_entities_tree(){
	if( g_mapScripts.entitiesTree == nullptr )
		return;

	g_mapScripts.entitiesTree->clear();
	gather_entities();

	const QString filter = g_mapScripts.filterEdit != nullptr ? g_mapScripts.filterEdit->text().trimmed() : QString();
	for( const EntityInfo& info : g_mapScripts.entities ){
		const QString label = entity_display_name( info );
		if( !filter.isEmpty() && !label.contains( filter, Qt::CaseInsensitive ) )
			continue;

		auto* item = new QTreeWidgetItem( g_mapScripts.entitiesTree );
		item->setText( 0, label );
		item->setData( 0, Qt::ItemDataRole::UserRole, QString::fromLatin1( info.classname.c_str() ) );
		item->setData( 0, Qt::ItemDataRole::UserRole + 1, QString::fromLatin1( info.scriptname.c_str() ) );
		item->setData( 0, Qt::ItemDataRole::UserRole + 2, QString::fromLatin1( info.name.c_str() ) );
		item->setData( 0, Qt::ItemDataRole::UserRole + 3, info.aiScript );
	}
}

void refresh_scripts_data(){
	const std::string aiPath = map_script_path_for_extension( ".ai" );
	const std::string scriptPath = map_script_path_for_extension( ".script" );
	g_mapScripts.aiData = parse_map_script_file( aiPath.c_str() );
	g_mapScripts.moverData = parse_map_script_file( scriptPath.c_str() );
}

void refresh_all_data(){
	const QString mapNameNow = current_map_name();
	if( g_mapScripts.loadedMapName != mapNameNow ){
		close_all_editor_tabs();
		g_mapScripts.loadedMapName = mapNameNow;
	}
	refresh_scripts_data();
	refresh_entities_tree();
}

void mapscripts_refresh_on_map_valid_changed(){
	if( g_mapScripts.dock == nullptr )
		return;
	refresh_all_data();
}

void mapscripts_on_map_valid_changed( QDockWidget* dock ){
	( void )dock;
	mapscripts_refresh_on_map_valid_changed();
}

EntityInfo item_to_entity( QTreeWidgetItem* item ){
	EntityInfo info;
	info.classname = item->data( 0, Qt::ItemDataRole::UserRole ).toString().toLatin1().constData();
	info.scriptname = item->data( 0, Qt::ItemDataRole::UserRole + 1 ).toString().toLatin1().constData();
	info.name = item->data( 0, Qt::ItemDataRole::UserRole + 2 ).toString().toLatin1().constData();
	info.aiScript = item->data( 0, Qt::ItemDataRole::UserRole + 3 ).toBool();
	return info;
}
}

bool MapScripts_isSupportedGame(){
	if( g_pGameDescription == nullptr )
		return false;
	return g_pGameDescription->mGameType == "wolf"
		|| g_pGameDescription->mGameType == "et";
}

void MapScripts_constructWindow( QMainWindow* main_window ){
	if( !MapScripts_isSupportedGame() )
		return;

	ASSERT_MESSAGE( g_mapScripts.dock == nullptr, "Map Scripts dock already created" );

	g_mapScripts.dock = new QDockWidget( "Map Scripts", main_window );
	g_mapScripts.dock->setObjectName( "MapScripts_Dock" );
	g_mapScripts.dock->setAllowedAreas(
		Qt::DockWidgetArea::LeftDockWidgetArea
		| Qt::DockWidgetArea::RightDockWidgetArea
		| Qt::DockWidgetArea::TopDockWidgetArea
		| Qt::DockWidgetArea::BottomDockWidgetArea
	);

	auto* root = new QWidget( g_mapScripts.dock );
	auto* rootLayout = new QVBoxLayout( root );
	rootLayout->setContentsMargins( 0, 0, 0, 0 );
	auto* splitter = new QSplitter;
	g_mapScripts.splitter = splitter;
	rootLayout->addWidget( splitter );

	{
		auto* leftWidget = new QWidget;
		auto* leftLayout = new QVBoxLayout( leftWidget );
		leftLayout->setContentsMargins( 4, 4, 4, 4 );

		auto* controlsLayout = new QHBoxLayout;
		leftLayout->addLayout( controlsLayout );

		g_mapScripts.filterEdit = new QLineEdit;
		g_mapScripts.filterEdit->setPlaceholderText( "Filter entities..." );
		controlsLayout->addWidget( g_mapScripts.filterEdit, 1 );

		auto* refreshButton = new QToolButton;
		refreshButton->setText( "Refresh" );
		controlsLayout->addWidget( refreshButton );

		g_mapScripts.entitiesTree = new QTreeWidget;
		g_mapScripts.entitiesTree->setHeaderHidden( true );
		g_mapScripts.entitiesTree->setRootIsDecorated( false );
		g_mapScripts.entitiesTree->setUniformRowHeights( true );
		leftLayout->addWidget( g_mapScripts.entitiesTree, 1 );

		splitter->addWidget( leftWidget );

		QObject::connect( g_mapScripts.filterEdit, &QLineEdit::textChanged, []( const QString& ){
			refresh_entities_tree();
		} );
		QObject::connect( refreshButton, &QToolButton::clicked, [](){
			refresh_all_data();
		} );
		QObject::connect( g_mapScripts.entitiesTree, &QTreeWidget::itemDoubleClicked, []( QTreeWidgetItem* item, int ){
			if( item == nullptr )
				return;
			open_entity_editor( item_to_entity( item ) );
		} );
	}

	{
		g_mapScripts.tabs = new QTabWidget;
		g_mapScripts.tabs->setTabsClosable( true );
		g_mapScripts.tabs->setMovable( true );
		splitter->addWidget( g_mapScripts.tabs );
		QObject::connect( g_mapScripts.tabs, &QTabWidget::tabCloseRequested, []( int index ){
			delete g_mapScripts.tabs->widget( index );
		} );
	}

	splitter->setStretchFactor( 0, 0 );
	splitter->setStretchFactor( 1, 1 );
	splitter->setSizes( { 360, 700 } );

	g_mapScripts.dock->setWidget( root );
	restore_mapscripts_layout( main_window );

	QObject::connect( g_mapScripts.dock, &QDockWidget::topLevelChanged, [main_window]( bool ){
		save_mapscripts_layout( main_window );
		QTimer::singleShot( 0, [main_window](){
			apply_mapscripts_dock_size( main_window );
		} );
	} );
	QObject::connect( g_mapScripts.dock, &QDockWidget::dockLocationChanged, [main_window]( Qt::DockWidgetArea ){
		save_mapscripts_layout( main_window );
		QTimer::singleShot( 0, [main_window](){
			apply_mapscripts_dock_size( main_window );
		} );
	} );
	QObject::connect( g_mapScripts.dock, &QDockWidget::visibilityChanged, [main_window]( bool visible ){
		save_mapscripts_layout( main_window );
		if( visible ){
			QTimer::singleShot( 0, [main_window](){
				apply_mapscripts_dock_size( main_window );
			} );
		}
	} );
	QObject::connect( splitter, &QSplitter::splitterMoved, [main_window]( int, int ){
		save_mapscripts_layout( main_window );
	} );

	Map_addValidCallback( g_map, PointerCaller<QDockWidget, void(), mapscripts_on_map_valid_changed>( g_mapScripts.dock ) );
	refresh_all_data();
}

void MapScripts_destroyWindow(){
	if( g_mapScripts.dock == nullptr )
		return;
	save_mapscripts_layout( nullptr );
	delete g_mapScripts.dock;
	g_mapScripts = {};
}

void MapScripts_toggleShown(){
	if( !MapScripts_isSupportedGame() )
		return;

	if( g_mapScripts.dock == nullptr )
		return;

	const bool show = !g_mapScripts.dock->isVisible();
	g_mapScripts.dock->setVisible( show );
	if( show ){
		g_mapScripts.dock->raise();
		refresh_all_data();
	}
}
