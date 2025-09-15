#include "filterbar.h"
#include "gtkmisc.h"
#include "stream/stringstream.h"
#include "select.h"
#include "iundo.h"
#include "preferences.h"

#include "commands.h"
#include "gtkutil/accelerator.h"
#include "generic/callback.h"
#include "math/vector.h"

#include <QEvent>
#include <QMouseEvent>

#include "entity.h"


CopiedString GetCommonShader( const char* name ){
	const char* gotShader = g_pGameDescription->getKeyValue( StringStream<32>( "shader_", name ) );
	if( !string_empty( gotShader ) ){
		return gotShader;
	}
	else{
		if( string_empty( g_pGameDescription->getKeyValue( "show_wads" ) ) ){
			const char* commonDir = g_pGameDescription->getKeyValue( "common_shaders_dir" );
			if( string_empty( commonDir ) )
				commonDir = "common/";
			return StringStream<64>( "textures/", commonDir, name ).c_str();
		}
		else{
			return StringStream<64>( "textures/", name ).c_str();
		}
	}
}

void SetCommonShader( const char* name ){
	UndoableCommand undo( "textureNameSetSelected" );
	Select_SetShader( GetCommonShader( name ).c_str() );
}


const char* GetCaulkShader(){
	static const char* caulk_shader = string_clone( GetCommonShader( "caulk" ).c_str() );
	return caulk_shader;
}

class CommonFunc
{
public:
	static inline CommonFunc* m_recentFunc{};
	virtual void exec() = 0;
	virtual ~CommonFunc() = default;
};

class CommonFunc_tex : public CommonFunc
{
	const std::vector<const char*> m_texNames;
	std::size_t m_toggleTexNum{};
public:
	CommonFunc_tex( std::vector<const char*>&& texNames ) : m_texNames( std::move( texNames ) ){}
	void exec() override {
		if ( m_recentFunc != this ){
			m_toggleTexNum = 0;
			m_recentFunc = this;
		}
		else{
			m_toggleTexNum %= m_texNames.size();
		}

		SetCommonShader( m_texNames[m_toggleTexNum] );
		++m_toggleTexNum;
	}
};

class CommonFunc_command : public CommonFunc
{
	const char * const m_commandName;
public:
	CommonFunc_command( const char* commandName ) : m_commandName( commandName ){}
	void exec() override {
		GlobalCommands_find( m_commandName ).m_callback();
		m_recentFunc = this;
	}
};

class CommonFunc_group : public CommonFunc
{
public:
	void exec() override {
		Entity_createFromSelection( "func_group", g_vector3_identity );
		m_recentFunc = this;
	}
};


class FilterToolbarHandler : public QObject
{
	std::optional<CommonFunc*> findFunc( const QPoint pos ){
		if( QAction *action = m_toolbar->actionAt( pos ) )
			if( auto it = m_actions.find( action ); it != m_actions.end() )
				return it->second.get();
		return {};
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick ){
			auto *mouseEvent = static_cast<QMouseEvent *>( event );
			if( mouseEvent->button() == Qt::MouseButton::RightButton ){
				if( auto func = findFunc( mouseEvent->pos() ) ){
					func.value()->exec();
					return true;
				}
			}
		}
		else if( event->type() == QEvent::ContextMenu ){ // suppress context menu on special buttons
			auto *contextEvent = static_cast<QContextMenuEvent *>( event );
			if( contextEvent->reason() == QContextMenuEvent::Reason::Mouse && findFunc( contextEvent->pos() ) )
				return true;
		}
		else if( event->type() == QEvent::Enter ){
			CommonFunc::m_recentFunc = nullptr;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
	const QToolBar *m_toolbar;
public:
	FilterToolbarHandler( QToolBar *toolbar ) : QObject( toolbar ), m_toolbar( toolbar ){
	}
	std::map<QAction*, std::unique_ptr<CommonFunc>> m_actions;
};


void create_filter_toolbar( QToolBar *toolbar ){
	auto *handler = new FilterToolbarHandler( toolbar );
	toolbar->installEventFilter( handler );

	QAction* button;

	toolbar_append_toggle_button( toolbar, "World", "f-world.png", "FilterWorldBrushes" );

	button = toolbar_append_toggle_button( toolbar, "Structural\nRightClick: MakeStructural", "f-structural.png", "FilterStructural" );
	handler->m_actions.emplace( button, new CommonFunc_command( "MakeStructural" ) );

	button = toolbar_append_toggle_button( toolbar, "Details\nRightClick: MakeDetail", "f-details.png", "FilterDetails" );
	handler->m_actions.emplace( button, new CommonFunc_command( "MakeDetail" ) );

	button = toolbar_append_toggle_button( toolbar, "Func_Groups\nRightClick: create func_group", "f-funcgroups.png", "FilterFuncGroups" );
	handler->m_actions.emplace( button, new CommonFunc_group );

	toolbar_append_toggle_button( toolbar, "Patches", "f-patches.png", "FilterPatches" );
	toolbar_append_separator( toolbar );

//	if ( g_pGameDescription->mGameType == "doom3" ) {
//		button = toolbar_append_toggle_button( toolbar, "Visportals", "f-areaportal.png", "FilterVisportals" );
//	}
//	else{
//		button = toolbar_append_toggle_button( toolbar, "Areaportals", "f-areaportal.png", "FilterAreaportals" );
//	}

	button = toolbar_append_toggle_button( toolbar, "Translucent\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-translucent.png", "FilterTranslucent" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "nodraw", "nodrawnonsolid" } ) );

	button = toolbar_append_toggle_button( toolbar, "Liquids\nRightClick: toggle tex\n\twaterCaulk\n\tlavaCaulk\n\tslimeCaulk", "f-liquids.png", "FilterLiquids" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "watercaulk", "lavacaulk", "slimecaulk" } ) );

	button = toolbar_append_toggle_button( toolbar, "Caulk\nRightClick: tex Caulk", "f-caulk.png", "FilterCaulk" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "caulk" } ) );

	button = toolbar_append_toggle_button( toolbar, "Clips\nRightClick: toggle tex\n\tplayerClip\n\tweapClip", "f-clip.png", "FilterClips" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "clip", "weapclip" } ) );

	button = toolbar_append_toggle_button( toolbar, "HintsSkips\nRightClick: toggle tex\n\thint\n\thintLocal\n\thintSkip", "f-hint.png", "FilterHintsSkips" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "hint", "hintlocal", "hintskip" } ) );

	button = toolbar_append_toggle_button( toolbar, "Sky", "f-sky.png", "FilterSky" );

	//toolbar_append_toggle_button( toolbar, "Paths", "texture_lock.png", "FilterPaths" );
	toolbar_append_separator( toolbar );
	toolbar_append_toggle_button( toolbar, "Entities", "f-entities.png", "FilterEntities" );
	toolbar_append_toggle_button( toolbar, "Point Entities", "f-pointentities.png", "FilterPointEntities" );
	toolbar_append_toggle_button( toolbar, "Lights", "f-lights.png", "FilterLights" );
	toolbar_append_toggle_button( toolbar, "Models", "f-models.png", "FilterModels" );

	button = toolbar_append_toggle_button( toolbar, "Triggers\nRightClick: tex Trigger", "f-triggers.png", "FilterTriggers" );
	handler->m_actions.emplace( button, new CommonFunc_tex( { "trigger" } ) );

	//toolbar_append_toggle_button( toolbar, "Decals", "f-decals.png", "FilterDecals" );
	toolbar_append_separator( toolbar );
	//toolbar_append_button( toolbar, "InvertFilters", "f-invert.png", "InvertFilters" );

	toolbar_append_button( toolbar, "ResetFilters", "f-reset.png", "ResetFilters" );

	toolbar_append_separator( toolbar );
	button = toolbar_append_toggle_button( toolbar, "Region Set Selection\nRightClick: Region Off", "f-region.png", "RegionSetSelection" );
	handler->m_actions.emplace( button, new CommonFunc_command( "RegionOff" ) );

	button = toolbar_append_toggle_button( toolbar, "Hide Selected\nRightClick: Show Hidden", "f-hide.png", "HideSelected" );
	handler->m_actions.emplace( button, new CommonFunc_command( "ShowHidden" ) );
}
