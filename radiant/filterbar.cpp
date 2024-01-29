#include "filterbar.h"
#include "gtkmisc.h"
#include "gtkutil/widget.h"
#include "gtkutil/toolbar.h"
#include "stream/stringstream.h"
#include "select.h"
#include "iundo.h"
#include "preferences.h"

#include "commands.h"
#include "gtkutil/accelerator.h"
#include "generic/callback.h"

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
	CommonFunc_tex( const std::vector<const char*>&& texNames ) : m_texNames( std::move( texNames ) ){}
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
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick ){
			QMouseEvent *mouseEvent = static_cast<QMouseEvent *>( event );
			if( mouseEvent->button() == Qt::MouseButton::RightButton ){
				QAction *action = m_toolbar->actionAt( mouseEvent->pos() );
				if( action != nullptr ){
					auto it = m_actions.find( action );
					if( it != m_actions.end() ){
						it->second->exec();
						return true;
					}
				}
			}
		}
		else if( event->type() == QEvent::ContextMenu ){ // suppress context menu
			return true;
		}
		else if( event->type() == QEvent::Enter ){
			CommonFunc::m_recentFunc = nullptr;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
public:
	std::map<QAction*, std::unique_ptr<CommonFunc>> m_actions;
	static inline const QToolBar *m_toolbar{};
}
g_filter_toolbar_handler;


void create_filter_toolbar( QToolBar *toolbar ){
	g_filter_toolbar_handler.m_toolbar = toolbar;
	toolbar->installEventFilter( &g_filter_toolbar_handler );

	QAction* button;

	toolbar_append_toggle_button( toolbar, "World", "f-world.png", "FilterWorldBrushes" );

	button = toolbar_append_toggle_button( toolbar, "Structural\nRightClick: MakeStructural", "f-structural.png", "FilterStructural" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_command( "MakeStructural" ) );

	button = toolbar_append_toggle_button( toolbar, "Details\nRightClick: MakeDetail", "f-details.png", "FilterDetails" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_command( "MakeDetail" ) );

	button = toolbar_append_toggle_button( toolbar, "Func_Groups\nRightClick: create func_group", "f-funcgroups.png", "FilterFuncGroups" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_group );

	toolbar_append_toggle_button( toolbar, "Patches", "patch_wireframe.png", "FilterPatches" );
	toolbar->addSeparator();

//	if ( g_pGameDescription->mGameType == "doom3" ) {
//		button = toolbar_append_toggle_button( toolbar, "Visportals", "f-areaportal.png", "FilterVisportals" );
//	}
//	else{
//		button = toolbar_append_toggle_button( toolbar, "Areaportals", "f-areaportal.png", "FilterAreaportals" );
//	}

	button = toolbar_append_toggle_button( toolbar, "Translucent\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-translucent.png", "FilterTranslucent" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "nodraw", "nodrawnonsolid" } ) );

	button = toolbar_append_toggle_button( toolbar, "Liquids\nRightClick: toggle tex\n\twaterCaulk\n\tlavaCaulk\n\tslimeCaulk", "f-liquids.png", "FilterLiquids" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "watercaulk", "lavacaulk", "slimecaulk" } ) );

	button = toolbar_append_toggle_button( toolbar, "Caulk\nRightClick: tex Caulk", "f-caulk.png", "FilterCaulk" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "caulk" } ) );

	button = toolbar_append_toggle_button( toolbar, "Clips\nRightClick: toggle tex\n\tplayerClip\n\tweapClip", "f-clip.png", "FilterClips" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "clip", "weapclip" } ) );

	button = toolbar_append_toggle_button( toolbar, "HintsSkips\nRightClick: toggle tex\n\thint\n\thintLocal\n\thintSkip", "f-hint.png", "FilterHintsSkips" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "hint", "hintlocal", "hintskip" } ) );

	button = toolbar_append_toggle_button( toolbar, "Sky", "f-sky.png", "FilterSky" );

	//toolbar_append_toggle_button( toolbar, "Paths", "texture_lock.png", "FilterPaths" );
	toolbar->addSeparator();
	toolbar_append_toggle_button( toolbar, "Entities", "f-entities.png", "FilterEntities" );
	toolbar_append_toggle_button( toolbar, "Point Entities", "status_entity.png", "FilterPointEntities" );
	toolbar_append_toggle_button( toolbar, "Lights", "f-lights.png", "FilterLights" );
	toolbar_append_toggle_button( toolbar, "Models", "f-models.png", "FilterModels" );

	button = toolbar_append_toggle_button( toolbar, "Triggers\nRightClick: tex Trigger", "f-triggers.png", "FilterTriggers" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_tex( std::vector<const char*>{ "trigger" } ) );

	//toolbar_append_toggle_button( toolbar, "Decals", "f-decals.png", "FilterDecals" );
	toolbar->addSeparator();
	//toolbar_append_button( toolbar, "InvertFilters", "f-invert.png", "InvertFilters" );

	toolbar_append_button( toolbar, "ResetFilters", "f-reset.png", "ResetFilters" );

	toolbar->addSeparator();
	button = toolbar_append_toggle_button( toolbar, "Region Set Selection\nRightClick: Region Off", "f-region.png", "RegionSetSelection" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_command( "RegionOff" ) );

	button = toolbar_append_toggle_button( toolbar, "Hide Selected\nRightClick: Show Hidden", "f-hide.png", "HideSelected" );
	g_filter_toolbar_handler.m_actions.emplace( button, new CommonFunc_command( "ShowHidden" ) );
}
