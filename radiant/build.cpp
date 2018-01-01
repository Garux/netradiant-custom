/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "build.h"
#include "debugging/debugging.h"

#include <map>
#include <list>
#include "stream/stringstream.h"
#include "versionlib.h"

#include "mainframe.h"

typedef std::map<CopiedString, CopiedString> Variables;
Variables g_build_variables;

void build_clear_variables(){
	g_build_variables.clear();
}

void build_set_variable( const char* name, const char* value ){
	g_build_variables[name] = value;
}

const char* build_get_variable( const char* name ){
	Variables::iterator i = g_build_variables.find( name );
	if ( i != g_build_variables.end() ) {
		return ( *i ).second.c_str();
	}
	globalErrorStream() << "undefined build variable: " << makeQuoted( name ) << '\n';
	return "";
}

#include "xml/ixml.h"
#include "xml/xmlelement.h"

class Evaluatable
{
public:
	virtual ~Evaluatable(){}
	virtual void evaluate( StringBuffer& output ) const = 0;
	virtual void exportXML( XMLImporter& importer ) const = 0;
};

class VariableString : public Evaluatable
{
	CopiedString m_string;
public:
	VariableString() : m_string(){
	}
	VariableString( const char* string ) : m_string( string ){
	}
	const char* c_str() const {
		return m_string.c_str();
	}
	void setString( const char* string ){
		m_string = string;
	}
	void evaluate( StringBuffer& output ) const override {
		StringBuffer variable;
		bool in_variable = false;
		for ( const char* i = m_string.c_str(); *i != '\0'; ++i )
		{
			if ( !in_variable ) {
				switch ( *i )
				{
				case '[':
					in_variable = true;
					break;
				default:
					output.push_back( *i );
					break;
				}
			}
			else
			{
				switch ( *i )
				{
				case ']':
					in_variable = false;
					output.push_string( build_get_variable( variable.c_str() ) );
					variable.clear();
					break;
				default:
					variable.push_back( *i );
					break;
				}
			}
		}
	}
	void exportXML( XMLImporter& importer ) const override {
		importer << c_str();
	}
};

class Conditional : public Evaluatable
{
	VariableString* m_test;
public:
	Evaluatable* m_result;
	Conditional( VariableString* test ) : m_test( test ){
	}
	~Conditional(){
		delete m_test;
		delete m_result;
	}
	void evaluate( StringBuffer& output ) const override {
		StringBuffer buffer;
		m_test->evaluate( buffer );
		if ( !buffer.empty() ) {
			m_result->evaluate( output );
		}
	}
	void exportXML( XMLImporter& importer ) const override {
		StaticElement conditionElement( "cond" );
		conditionElement.insertAttribute( "value", m_test->c_str() );
		importer.pushElement( conditionElement );
		m_result->exportXML( importer );
		importer.popElement( conditionElement.name() );
	}
};

typedef std::vector<Evaluatable*> Evaluatables;

class Tool : public Evaluatable
{
	Evaluatables m_evaluatables;
public:
	~Tool(){
		for ( auto* e : m_evaluatables )
		{
			delete ( e );
		}
	}
	void push_back( Evaluatable* evaluatable ){
		m_evaluatables.push_back( evaluatable );
	}
	void evaluate( StringBuffer& output ) const override {
		for ( const auto* e : m_evaluatables )
		{
			e->evaluate( output );
		}
	}
	void exportXML( XMLImporter& importer ) const override {
		for ( const auto* e : m_evaluatables )
		{
			e->exportXML( importer );
		}
	}
};

#include "xml/ixml.h"

class XMLElementParser : public TextOutputStream
{
public:
	virtual XMLElementParser& pushElement( const XMLElement& element ) = 0;
	virtual void popElement( const char* name ) = 0;
};

class VariableStringXMLConstructor final : public XMLElementParser
{
	StringBuffer m_buffer;
	VariableString& m_variableString;
public:
	VariableStringXMLConstructor( VariableString& variableString ) : m_variableString( variableString ){
	}
	~VariableStringXMLConstructor(){
		m_variableString.setString( m_buffer.c_str() );
	}
	std::size_t write( const char* buffer, std::size_t length ){
		m_buffer.push_range( buffer, buffer + length );
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		ERROR_MESSAGE( "parse error: invalid element " << makeQuoted( element.name() ) );
		return *this;
	}
	void popElement( const char* name ){
	}
};

class ConditionalXMLConstructor final : public XMLElementParser
{
	StringBuffer m_buffer;
	Conditional& m_conditional;
public:
	ConditionalXMLConstructor( Conditional& conditional ) : m_conditional( conditional ){
	}
	~ConditionalXMLConstructor(){
		m_conditional.m_result = new VariableString( m_buffer.c_str() );
	}
	std::size_t write( const char* buffer, std::size_t length ){
		m_buffer.push_range( buffer, buffer + length );
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		ERROR_MESSAGE( "parse error: invalid element " << makeQuoted( element.name() ) );
		return *this;
	}
	void popElement( const char* name ){
	}
};

class ToolXMLConstructor final : public XMLElementParser
{
	StringBuffer m_buffer;
	Tool& m_tool;
	ConditionalXMLConstructor* m_conditional;
public:
	ToolXMLConstructor( Tool& tool ) : m_tool( tool ){
	}
	~ToolXMLConstructor(){
		flush();
	}
	std::size_t write( const char* buffer, std::size_t length ){
		m_buffer.push_range( buffer, buffer + length );
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		if ( string_equal( element.name(), "cond" ) ) {
			flush();
			Conditional* conditional = new Conditional( new VariableString( element.attribute( "value" ) ) );
			m_tool.push_back( conditional );
			m_conditional = new ConditionalXMLConstructor( *conditional );
			return *m_conditional;
		}
		else
		{
			ERROR_MESSAGE( "parse error: invalid element " << makeQuoted( element.name() ) );
			return *this;
		}
	}
	void popElement( const char* name ){
		if ( string_equal( name, "cond" ) ) {
			delete m_conditional;
		}
	}

	void flush(){
		if ( !m_buffer.empty() ) {
			m_tool.push_back( new VariableString( m_buffer.c_str() ) );
			// q3map2 ExtraResourcePaths hack
			if( strstr( m_buffer.c_str(), "[RadiantPath]q3map2.[ExecutableType]" ) != nullptr // is q3map2
			 && strstr( m_buffer.c_str(), "[ExtraResourcePaths]" ) == nullptr ){ // has no extra path right away (could have been added by this before)
				m_tool.push_back( new VariableString( "[ExtraResourcePaths]" ) );
			}
			m_buffer.clear();
		}
	}
};

typedef VariableString BuildCommand;
typedef std::list<BuildCommand> Build;

class BuildXMLConstructor final : public XMLElementParser
{
	VariableStringXMLConstructor* m_variableString;
	Build& m_build;
public:
	BuildXMLConstructor( Build& build ) : m_build( build ){
	}
	std::size_t write( const char* buffer, std::size_t length ){
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		if ( string_equal( element.name(), "command" ) ) {
			m_build.push_back( BuildCommand() );
			m_variableString = new VariableStringXMLConstructor( m_build.back() );
			return *m_variableString;
		}
		else
		{
			ERROR_MESSAGE( "parse error: invalid element" );
			return *this;
		}
	}
	void popElement( const char* name ){
		delete m_variableString;
	}
};

typedef std::pair<CopiedString, Build> BuildPair;
#define SEPARATOR_STRING "-"
inline bool is_separator( const CopiedString& name, const Build& commands ){
	if ( !string_equal( name.c_str(), SEPARATOR_STRING ) ) {
		return false;
	}
	for ( const BuildCommand& cmd : commands )
	{
		if ( !string_empty( cmd.c_str() ) ) {
			return false;
		}
	}
	return true;
}


typedef std::list<BuildPair> Project;

Project::iterator Project_find( Project& project, std::size_t index ){
	return index < project.size()
	       ? std::next( project.begin(), index )
	       : project.end();
}
// returns either found index or fallback to 0
size_t Project_find( const Project& project, const Project::const_iterator iterator ){
	size_t idx = 0;
	for( auto i = project.cbegin(); i != project.cend(); ++i, ++idx )
		if( i == iterator )
			return idx;
	return 0;
}

Build::iterator Build_find( Build& build, std::size_t index ){
	Build::iterator i = build.begin();
	while ( index-- != 0 && i != build.end() )
	{
		++i;
	}
	return i;
}

typedef std::map<CopiedString, Tool> Tools;

class ProjectXMLConstructor : public XMLElementParser
{
	ToolXMLConstructor* m_tool;
	BuildXMLConstructor* m_build;
	Project& m_project;
	Tools& m_tools;
public:
	ProjectXMLConstructor( Project& project, Tools& tools ) : m_project( project ), m_tools( tools ){
	}
	std::size_t write( const char* buffer, std::size_t length ){
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		if ( string_equal( element.name(), "var" ) ) {
			Tools::iterator i = m_tools.insert( Tools::value_type( element.attribute( "name" ), Tool() ) ).first;
			m_tool = new ToolXMLConstructor( ( *i ).second );
			return *m_tool;
		}
		else if ( string_equal( element.name(), "build" ) ) {
			m_project.push_back( Project::value_type( element.attribute( "name" ), Build() ) );
			m_build = new BuildXMLConstructor( m_project.back().second );
			return *m_build;
		}
		else if ( string_equal( element.name(), "separator" ) ) {
			m_project.push_back( Project::value_type( SEPARATOR_STRING, Build() ) );
			return *this;
		}
		else
		{
			ERROR_MESSAGE( "parse error: invalid element" );
			return *this;
		}
	}
	void popElement( const char* name ){
		if ( string_equal( name, "var" ) ) {
			delete m_tool;
		}
		else if ( string_equal( name, "build" ) ) {
			delete m_build;
		}
	}
};

class SkipAllParser : public XMLElementParser
{
public:
	std::size_t write( const char* buffer, std::size_t length ){
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		return *this;
	}
	void popElement( const char* name ){
	}
};

class RootXMLConstructor : public XMLElementParser
{
	CopiedString m_elementName;
	XMLElementParser& m_parser;
	SkipAllParser m_skip;
	Version m_version;
	bool m_compatible;
public:
	RootXMLConstructor( const char* elementName, XMLElementParser& parser, const char* version ) :
		m_elementName( elementName ),
		m_parser( parser ),
		m_version( version_parse( version ) ),
		m_compatible( false ){
	}
	std::size_t write( const char* buffer, std::size_t length ){
		return length;
	}
	XMLElementParser& pushElement( const XMLElement& element ){
		if ( string_equal( element.name(), m_elementName.c_str() ) ) {
			Version dataVersion( version_parse( element.attribute( "version" ) ) );
			if ( version_compatible( m_version, dataVersion ) ) {
				m_compatible = true;
				return m_parser;
			}
			else
			{
				return m_skip;
			}
		}
		else
		{
			//ERROR_MESSAGE( "parse error: invalid element " << makeQuoted( element.name() ) );
			return *this;
		}
	}
	void popElement( const char* name ){
	}

	bool versionCompatible() const {
		return m_compatible;
	}
};

namespace
{
Project g_build_project;
Project::const_iterator g_lastExecutedBuild;
Tools g_build_tools;
bool g_build_changed = false;
}

void build_error_undefined_tool( const char* build, const char* tool ){
	globalErrorStream() << "build " << makeQuoted( build ) << " refers to undefined tool " << makeQuoted( tool ) << '\n';
}

void project_verify( Project& project, Tools& tools ){
#if 0
	for ( Project::iterator i = project.begin(); i != project.end(); ++i )
	{
		Build& build = ( *i ).second;
		for ( Build::iterator j = build.begin(); j != build.end(); ++j )
		{
			Tools::iterator k = tools.find( ( *j ).first );
			if ( k == g_build_tools.end() ) {
				build_error_undefined_tool( ( *i ).first.c_str(), ( *j ).first.c_str() );
			}
		}
	}
#endif
}

std::vector<CopiedString> build_construct_commands( size_t buildIdx ){
	for ( const auto& [ name, tool ] : g_build_tools )
	{
		StringBuffer output;
		tool.evaluate( output );
		build_set_variable( name.c_str(), output.c_str() );
	}

	std::vector<CopiedString> commands;

	if ( const auto buildIt = Project_find( g_build_project, buildIdx ); buildIt != g_build_project.end() )
	{
		g_lastExecutedBuild = buildIt;
		for ( const auto& command : buildIt->second )
		{
			StringBuffer output;
			command.evaluate( output );
			if ( !output.empty() )
				commands.emplace_back( output.c_str() );
		}
	}
	else
	{
		globalErrorStream() << "build #" << buildIdx << " not defined";
	}

	return commands;
}


typedef std::vector<XMLElementParser*> XMLElementStack;

class XMLParser : public XMLImporter
{
	XMLElementStack m_stack;
public:
	XMLParser( XMLElementParser& parser ){
		m_stack.push_back( &parser );
	}
	std::size_t write( const char* buffer, std::size_t length ){
		return m_stack.back()->write( buffer, length );
	}
	void pushElement( const XMLElement& element ){
		m_stack.push_back( &m_stack.back()->pushElement( element ) );
	}
	void popElement( const char* name ){
		m_stack.pop_back();
		m_stack.back()->popElement( name );
	}
};

#include "stream/textfilestream.h"
#include "xml/xmlparser.h"

const char* const BUILDMENU_VERSION = "2.0";

bool build_commands_parse( const char* filename ){
	TextFileInputStream projectFile( filename );
	if ( !projectFile.failed() ) {
		ProjectXMLConstructor projectConstructor( g_build_project, g_build_tools );
		RootXMLConstructor rootConstructor( "project", projectConstructor, BUILDMENU_VERSION );
		XMLParser importer( rootConstructor );
		XMLStreamParser parser( projectFile );
		parser.exportXML( importer );

		if ( rootConstructor.versionCompatible() ) {
			project_verify( g_build_project, g_build_tools );

			return true;
		}
		globalErrorStream() << "failed to parse build menu: " << makeQuoted( filename ) << '\n';
	}
	return false;
}

void build_commands_clear(){
	g_build_project.clear();
	g_build_tools.clear();
}

class BuildXMLExporter
{
	const Build& m_build;
public:
	BuildXMLExporter( const Build& build ) : m_build( build ){
	}
	void exportXML( XMLImporter& importer ) const {
		importer << '\n';
		for ( const auto& command : m_build )
		{
			StaticElement commandElement( "command" );
			importer.pushElement( commandElement );
			command.exportXML( importer );
			importer.popElement( commandElement.name() );
			importer << '\n';
		}
	}
};

class ProjectXMLExporter
{
	const Project& m_project;
	const Tools& m_tools;
public:
	ProjectXMLExporter( const Project& project, const Tools& tools ) : m_project( project ), m_tools( tools ){
	}
	void exportXML( XMLImporter& importer ) const {
		StaticElement projectElement( "project" );
		projectElement.insertAttribute( "version", BUILDMENU_VERSION );
		importer.pushElement( projectElement );
		importer << '\n';

		for ( const auto& [ name, tool ] : m_tools )
		{
			StaticElement toolElement( "var" );
			toolElement.insertAttribute( "name", name.c_str() );
			importer.pushElement( toolElement );
			tool.exportXML( importer );
			importer.popElement( toolElement.name() );
			importer << '\n';
		}
		for ( const auto& [ name, build ] : m_project )
		{
			if ( is_separator( name, build ) ) {
				StaticElement buildElement( "separator" );
				importer.pushElement( buildElement );
				importer.popElement( buildElement.name() );
				importer << '\n';
			}
			else
			{
				StaticElement buildElement( "build" );
				buildElement.insertAttribute( "name", name.c_str() );
				importer.pushElement( buildElement );
				BuildXMLExporter buildExporter( build );
				buildExporter.exportXML( importer );
				importer.popElement( buildElement.name() );
				importer << '\n';
			}
		}
		importer.popElement( projectElement.name() );
	}
};

#include "xml/xmlwriter.h"

void build_commands_write( const char* filename ){
	TextFileOutputStream projectFile( filename );
	if ( !projectFile.failed() ) {
		XMLStreamWriter writer( projectFile );
		ProjectXMLExporter projectExporter( g_build_project, g_build_tools );
		writer << '\n';
		projectExporter.exportXML( writer );
		writer << '\n';
	}
}


#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QScrollBar>
#include <QHeaderView>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>


void Build_refreshMenu( QMenu* menu );

inline QTreeWidgetItem* new_item( const char *text ){
	auto item = new QTreeWidgetItem;
	item->setText( 0, text );
	item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren );
	return item;
}

#define LAST_ITER_STRING "..."
inline void last_iter_append( QTreeWidget* tree ){
	tree->addTopLevelItem( new_item( LAST_ITER_STRING ) );
}


void BSPCommandList_Construct( QTreeWidget* tree, Project& project ){
	tree->clear();

	for ( const auto& [ name, commands ] : project )
	{
		tree->addTopLevelItem( new_item( name.c_str() ) );
	}

	last_iter_append( tree );
}

class ProjectList
{
public:
	Project& m_project;
	QTreeWidget* m_buildView;
	bool m_changed;
	ProjectList( Project& project ) : m_project( project ), m_changed( false ){
	}
};

void project_cell_edited( QTreeWidgetItem *item, ProjectList& projectList ){
	Project& project = projectList.m_project;
	const auto new_text = item->text( 0 ).toLatin1();

	Project::iterator i = Project_find( project, item->treeWidget()->indexOfTopLevelItem( item ) );
	if ( i != project.end() ) { // edit
		projectList.m_changed = true;
		if ( new_text.isEmpty() ) { // empty = delete
			project.erase( i );
			delete item;
		}
		else
		{
			( *i ).first = new_text.constData();
		}
	}
	else if ( !new_text.isEmpty() && !string_equal( new_text, LAST_ITER_STRING ) ) { // add new
		projectList.m_changed = true;
		project.push_back( Project::value_type( new_text.constData(), Build() ) );

		last_iter_append( projectList.m_buildView );
		//refresh command field
		item->treeWidget()->currentItemChanged( item, nullptr );
	}

	Build_refreshMenu( g_bsp_menu );
}


BuildPair g_buildpair_copied;
BuildCommand g_buildcommand_copied;

class Project_key_press : public QObject
{
	ProjectList& m_projectList;
public:
	Project_key_press( ProjectList& projectList ) : QObject( projectList.m_buildView ), m_projectList( projectList ){}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::KeyPress ) {
			Project& project = m_projectList.m_project;
			if( QTreeWidgetItem *item = m_projectList.m_buildView->currentItem() ){
				Project::iterator x = Project_find( project, item->treeWidget()->indexOfTopLevelItem( item ) );
				QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
				if ( keyEvent->matches( QKeySequence::StandardKey::Delete ) && x != project.end() ) {
					m_projectList.m_changed = true;
					project.erase( x );
					Build_refreshMenu( g_bsp_menu );

					const int id = item->treeWidget()->indexOfTopLevelItem( item );
					delete item;
					m_projectList.m_buildView->currentItemChanged( m_projectList.m_buildView->topLevelItem( id ), nullptr ); //refresh command field
				}
				else if ( keyEvent->matches( QKeySequence::StandardKey::Copy ) && x != project.end() ) {
					g_buildpair_copied = ( *x );
				}
				else if ( keyEvent->matches( QKeySequence::StandardKey::Paste ) && !g_buildpair_copied.first.empty() ) {
					m_projectList.m_changed = true;
					project.insert( x, g_buildpair_copied );
					Build_refreshMenu( g_bsp_menu );

					item->treeWidget()->insertTopLevelItem( item->treeWidget()->indexOfTopLevelItem( item ), new_item( g_buildpair_copied.first.c_str() ) );
				}

				event->accept();
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

Build* g_current_build = 0;

void project_selection_changed( QTreeWidgetItem* buildItem, QTreeWidget* cmdTree ){
	Project& project = g_build_project;

	cmdTree->clear();

	if ( buildItem != nullptr ) {
		Project::iterator x = Project_find( project, buildItem->treeWidget()->indexOfTopLevelItem( buildItem ) );

		if ( x != project.end() ) {
			Build& build = ( *x ).second;
			g_current_build = &build;

			for ( const BuildCommand& cmd : build )
			{
				cmdTree->addTopLevelItem( new_item( cmd.c_str() ) );
			}
			last_iter_append( cmdTree );
		}
		else
		{
			g_current_build = 0;
		}
	}
	else
	{
		g_current_build = 0;
	}
}

void commands_cell_edited( QTreeWidgetItem *item ){
	if ( g_current_build == 0 ) {
		return;
	}
	Build& build = *g_current_build;
	const auto new_text = item->text( 0 ).toLatin1();

	Build::iterator i = Build_find( build, item->treeWidget()->indexOfTopLevelItem( item ) );
	if ( i != build.end() ) { // edit
		g_build_changed = true;
		( *i ).setString( new_text );
	}
	else if ( !new_text.isEmpty() && !string_equal( new_text, LAST_ITER_STRING ) ) { // add new
		g_build_changed = true;
		build.push_back( Build::value_type( VariableString( new_text ) ) );

		last_iter_append( item->treeWidget() );
	}

	Build_refreshMenu( g_bsp_menu );
}

class Commands_key_press : public QObject
{
	QTreeWidget* m_tree;
public:
	Commands_key_press( QTreeWidget* tree ) : QObject( tree ), m_tree( tree ){}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::KeyPress && g_current_build != nullptr ) {
			Build& build = *g_current_build;
			if( QTreeWidgetItem *item = m_tree->currentItem() ){
				Build::iterator x = Build_find( build, item->treeWidget()->indexOfTopLevelItem( item ) );
				QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
				if ( keyEvent->matches( QKeySequence::StandardKey::Delete ) && x != build.end() ) {
					g_build_changed = true;
					build.erase( x );

					delete item;
				}
				else if ( keyEvent->matches( QKeySequence::StandardKey::Copy ) && x != build.end() ) {
					g_buildcommand_copied = ( *x );
				}
				else if ( keyEvent->matches( QKeySequence::StandardKey::Paste ) && !string_empty( g_buildcommand_copied.c_str() ) ) {
					g_build_changed = true;
					build.insert( x, g_buildcommand_copied );

					item->treeWidget()->insertTopLevelItem( item->treeWidget()->indexOfTopLevelItem( item ), new_item( g_buildcommand_copied.c_str() ) );
				}

				event->accept();
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

#include "qe3.h"
#include "gtkutil/guisettings.h"

EMessageBoxReturn BuildMenuDialog_construct( ProjectList& projectList ){
	static auto [dialog, rootLayout] = [](){
		QDialog *dialog = new QDialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
		dialog->setWindowTitle( "Build Menu" );
		g_guiSettings.addWindow( dialog, "BuildMenu/geometry", 700, 500 );
		auto *rootLayout = new QHBoxLayout( dialog );
		rootLayout->setContentsMargins( 0, 0, 0, 0 );
		return std::pair( dialog, rootLayout );
	}();

	auto *container = new QWidget; // use container widget to easily remove window content
	rootLayout->addWidget( container );

	{
		auto grid = new QGridLayout( container );
		{
			auto buttons = new QDialogButtonBox;
			buttons->setOrientation( Qt::Orientation::Vertical );
			// rejection via dialog means will return DialogCode::Rejected (0), eID* > 0
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Ok ),
								&QAbstractButton::clicked, [dialog = dialog](){ dialog->done( eIDOK ); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ),
								&QAbstractButton::clicked, dialog, &QDialog::reject );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Reset ),
								&QAbstractButton::clicked, [dialog = dialog](){ dialog->done( eIDNO ); } );
			buttons->button( QDialogButtonBox::StandardButton::Reset )->setToolTip( "Reset to editor start state" );
			grid->addWidget( buttons, 0, 1 );
		}
		QTreeWidget* buildView = nullptr;
		{
			auto frame = new QGroupBox( "Build menu" );
			grid->addWidget( frame, 0, 0 );
			grid->setRowStretch( 0, 1 );
			{
				auto tree = projectList.m_buildView = buildView = new QTreeWidget;
				tree->setColumnCount( 1 );
				tree->setUniformRowHeights( true ); // optimization
				tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
				tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
				tree->header()->setStretchLastSection( false ); // non greedy column sizing
				tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
				tree->setHeaderHidden( true );
				tree->setRootIsDecorated( false );
				( new QHBoxLayout( frame ) )->addWidget( tree );
				{
					QObject::connect( tree, &QTreeWidget::itemChanged, [&projectList]( QTreeWidgetItem *item, int column ){
						project_cell_edited( item, projectList );
					} );

					tree->installEventFilter( new Project_key_press( projectList ) );
				}
			}
		}
		{
			auto frame = new QGroupBox( "Commandline" );
			grid->addWidget( frame, 1, 0 );
			{
				auto tree = new QTreeWidget;
				tree->setColumnCount( 1 );
				tree->setUniformRowHeights( true ); // optimization
				tree->setVerticalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
				tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
				tree->header()->setStretchLastSection( false ); // non greedy column sizing
				tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
				tree->setHeaderHidden( true );
				tree->setRootIsDecorated( false );
				( new QHBoxLayout( frame ) )->addWidget( tree );
				{
					QObject::connect( tree, &QTreeWidget::itemChanged, []( QTreeWidgetItem *item, int column ){
						commands_cell_edited( item );
					} );

					QObject::connect( buildView, &QTreeWidget::currentItemChanged, [tree]( QTreeWidgetItem *current, QTreeWidgetItem *previous ){
						project_selection_changed( current, tree );
					} );

					tree->installEventFilter( new Commands_key_press( tree ) );

					QObject::connect( tree->model(), &QAbstractItemModel::rowsInserted, [tree]( const QModelIndex &parent, int first, int last ){
						tree->setFixedHeight( tree->sizeHintForRow( 0 ) * std::max( tree->model()->rowCount(), 4 )
							+ tree->horizontalScrollBar()->sizeHint().height() + tree->frameWidth() * 2 );
					} );
				}
			}
		}
		{
			auto expander = new QGroupBox( "build variables" );
			expander->setFlat( true );
			expander->setCheckable( true );
			expander->setChecked( false );
			grid->addWidget( expander, 2, 0 );

			bsp_init();
			for ( auto& [ name, tool ] : g_build_tools ){
				StringBuffer output;
				tool.evaluate( output );
				build_set_variable( name.c_str(), output.c_str() );
			}
			StringOutputStream stream( 256 );
			for( const auto& [ name, var ] : g_build_variables ){
				stream << '[' << name << "] = " << var << '\n';
			}
			build_clear_variables();

			auto label = new QLabel( stream.c_str() );
			label->hide();
			( new QHBoxLayout( expander ) )->addWidget( label );
			QObject::connect( expander, &QGroupBox::clicked, label, &QWidget::setVisible );
		}
	}

	BSPCommandList_Construct( projectList.m_buildView, g_build_project );

	const auto ret = static_cast<EMessageBoxReturn>( dialog->exec() );
	delete container;
	return ret;
}

void LoadBuildMenu();

void DoBuildMenu(){
	ProjectList projectList( g_build_project );
	const Project bakproj = g_build_project;
	const size_t baklast = Project_find( g_build_project, g_lastExecutedBuild );

	const EMessageBoxReturn ret = BuildMenuDialog_construct( projectList );

	if ( ret == eIDCANCEL || ret == 0 ) {
		if ( projectList.m_changed || g_build_changed ){
			g_build_project = bakproj;
			g_lastExecutedBuild = std::next( g_build_project.cbegin(), baklast );
			Build_refreshMenu( g_bsp_menu );
		}
	}
	else if( ret == eIDNO ){//RESET
		build_commands_clear();
		LoadBuildMenu();

		Build_refreshMenu( g_bsp_menu );
	}
	else if ( projectList.m_changed ) {
		g_build_changed = true;
	}
}



#include "gtkutil/menu.h"
#include "mainframe.h"
#include "preferences.h"

class BuildMenuItem
{
	/* using build index to pass build reference: it is okay for now, as we rebuild g_BuildMenuItems entirely on any modification
	   using build name before was faulty design, as builds may have equal names */
	const size_t m_buildIdx;
public:
	QAction* m_item;
	BuildMenuItem( size_t buildIdx )
		: m_buildIdx( buildIdx ), m_item( nullptr ){
	}
	void run() const {
		RunBSP( m_buildIdx );
	}
	typedef ConstMemberCaller<BuildMenuItem, void(), &BuildMenuItem::run> RunCaller;
};

typedef std::list<BuildMenuItem> BuildMenuItems;
BuildMenuItems g_BuildMenuItems;


QMenu* g_bsp_menu;

void Build_constructMenu( QMenu* menu ){
	size_t buildIdx{};
	for ( const auto& [ name, commands ] : g_build_project )
	{
		g_BuildMenuItems.push_back( BuildMenuItem( buildIdx++ ) );
		if ( is_separator( name, commands ) ) {
			g_BuildMenuItems.back().m_item = menu->addSeparator();
		}
		else
		{
			g_BuildMenuItems.back().m_item = create_menu_item_with_mnemonic( menu, name.c_str(), BuildMenuItem::RunCaller( g_BuildMenuItems.back() ) );
			{
				QString str;
				for( const BuildCommand& cmd : commands ){
					str += cmd.c_str();
					str += '\n';
				}
				str.truncate( str.size() - 1 );
				g_BuildMenuItems.back().m_item->setToolTip( str );
			}
		}
	}
}


void Build_refreshMenu( QMenu* menu ){
	for ( const BuildMenuItem& item : g_BuildMenuItems )
	{
		menu->removeAction( item.m_item );
	}

	g_BuildMenuItems.clear();

	Build_constructMenu( menu );
}


namespace
{
#include "os/path.h"

CopiedString g_buildMenu;

const char* g_buildMenuFullPah(){
	if( path_is_absolute( g_buildMenu.c_str() ) )
		return g_buildMenu.c_str();

	static StringOutputStream buffer( 256 );
	return buffer( SettingsPath_get(), g_pGameDescription->mGameFile, '/', g_buildMenu );
}
}

void LoadBuildMenu(){
	if ( g_buildMenu.empty() || !build_commands_parse( g_buildMenuFullPah() ) ) {
		if( !string_equal_nocase( g_buildMenu.c_str(), "build_menu.xml" ) ){
			g_buildMenu = "build_menu.xml";
			if( build_commands_parse( g_buildMenuFullPah() ) )
				return;
		}
		{
			const auto buffer = StringStream( GameToolsPath_get(), "default_build_menu.xml" );

			const bool success = build_commands_parse( buffer );
			ASSERT_MESSAGE( success, "failed to parse default build commands: " << buffer );
		}
	}
}

void SaveBuildMenu(){
	if ( g_build_changed ) {
		g_build_changed = false;
		build_commands_write( g_buildMenuFullPah() );
	}
}

#include "preferencesystem.h"
#include "stringio.h"

void BuildMenu_Construct(){
	GlobalPreferenceSystem().registerPreference( "BuildMenu", CopiedStringImportStringCaller( g_buildMenu ), CopiedStringExportStringCaller( g_buildMenu ) );
	LoadBuildMenu();
}
void BuildMenu_Destroy(){
	SaveBuildMenu();
}


void Build_runRecentExecutedBuild(){
	if( !g_build_project.empty() )
		RunBSP( Project_find( g_build_project, g_lastExecutedBuild ) );
}
