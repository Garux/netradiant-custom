#include "filterbar.h"
#include <gtk/gtk.h>
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

#include "entity.h"


CopiedString GetCommonShader( const char* name ){
	StringOutputStream sstream( 128 );
	sstream << "shader_" << name;
	const char* gotShader = g_pGameDescription->getKeyValue( sstream.c_str() );
	if( !string_empty( gotShader ) ){
		return gotShader;
	}
	else{
		sstream.clear();
		if( string_empty( g_pGameDescription->getKeyValue( "show_wads" ) ) ){
			sstream << "textures/common/" << name;
		}
		else{
			sstream << "textures/" << name;
		}
		return sstream.c_str();
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
	const std::vector<const char*> m_funcStrings;
public:
	CommonFunc( GtkToggleToolButton* button, const std::vector<const char*>&& funcStrings ) : m_funcStrings( funcStrings ){
		g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( button ) ) ), "button_press_event", G_CALLBACK( texFunc ), this );
	}
	CommonFunc( GtkToggleToolButton* button, const char* funcStrings ) : m_funcStrings( 1, funcStrings ){
		g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( button ) ) ), "button_press_event", G_CALLBACK( commandFunc ), this );
	}
	static std::size_t m_toggleFuncNum;
	static CommonFunc* m_recentFunc;
	static gboolean resetToggleFuncNum( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
		m_toggleFuncNum = 0;
	//	globalOutputStream() << "resetToggleFuncNum\n";
		return FALSE;
	}
	static gboolean texFunc( GtkWidget *widget, GdkEventButton *event, gpointer data ){
		CommonFunc* self = reinterpret_cast<CommonFunc*>( data );
		if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
			if ( m_recentFunc != self ){
				m_toggleFuncNum = 0;
				m_recentFunc = self;
			}
			else{
				m_toggleFuncNum %= self->m_funcStrings.size();
			}

			SetCommonShader( self->m_funcStrings[m_toggleFuncNum] );
			++m_toggleFuncNum;
			return TRUE;
		}
		return FALSE;
	}
	static gboolean commandFunc( GtkWidget *widget, GdkEventButton *event, gpointer data ){
		CommonFunc* self = reinterpret_cast<CommonFunc*>( data );
		if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
			GlobalCommands_find( self->m_funcStrings[0] ).m_callback();
			m_toggleFuncNum = 0;
			m_recentFunc = self;
			return TRUE;
		}
		return FALSE;
	}
};

std::size_t CommonFunc::m_toggleFuncNum = 0;
CommonFunc* CommonFunc::m_recentFunc = nullptr;

static std::list<CommonFunc> g_commonFuncs;


gboolean Func_Groups_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		Entity_createFromSelection( "func_group", g_vector3_identity );
		CommonFunc::m_toggleFuncNum = 0;
		return TRUE;
	}
	return FALSE;
}


GtkToolbar* create_filter_toolbar(){
	GtkToolbar* toolbar = toolbar_new();
	g_signal_connect( G_OBJECT( toolbar ), "enter_notify_event", G_CALLBACK( CommonFunc::resetToggleFuncNum ), 0 );

	GtkToggleToolButton* button;

	toolbar_append_toggle_button( toolbar, "World (ALT + 1)", "f-world.png", "FilterWorldBrushes" );

	button = toolbar_append_toggle_button( toolbar, "Structural (CTRL + SHIFT + D)\nRightClick: MakeStructural", "f-structural.png", "FilterStructural" );
	g_commonFuncs.emplace_back( button, "MakeStructural" );

	button = toolbar_append_toggle_button( toolbar, "Details (CTRL + D)\nRightClick: MakeDetail", "f-details.png", "FilterDetails" );
	g_commonFuncs.emplace_back( button, "MakeDetail" );

	button = toolbar_append_toggle_button( toolbar, "Func_Groups\nRightClick: create func_group", "f-funcgroups.png", "FilterFuncGroups" );
	g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( button ) ) ), "button_press_event", G_CALLBACK( Func_Groups_button_press ), 0 );

	toolbar_append_toggle_button( toolbar, "Patches (CTRL + P)", "patch_wireframe.png", "FilterPatches" );
	toolbar_append_space( toolbar );

//	if ( g_pGameDescription->mGameType == "doom3" ) {
//		button = toolbar_append_toggle_button( toolbar, "Visportals (ALT + 3)", "f-areaportal.png", "FilterVisportals" );
//	}
//	else{
//		button = toolbar_append_toggle_button( toolbar, "Areaportals (ALT + 3)", "f-areaportal.png", "FilterAreaportals" );
//	}

	button = toolbar_append_toggle_button( toolbar, "Translucent (ALT + 4)\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-translucent.png", "FilterTranslucent" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "nodraw", "nodrawnonsolid" } );

	button = toolbar_append_toggle_button( toolbar, "Liquids (ALT + 5)\nRightClick: toggle tex\n\twaterCaulk\n\tlavaCaulk\n\tslimeCaulk", "f-liquids.png", "FilterLiquids" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "watercaulk", "lavacaulk", "slimecaulk" } );

	button = toolbar_append_toggle_button( toolbar, "Caulk (ALT + 6)\nRightClick: tex Caulk", "f-caulk.png", "FilterCaulk" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "caulk" } );

	button = toolbar_append_toggle_button( toolbar, "Clips (ALT + 7)\nRightClick: toggle tex\n\tplayerClip\n\tweapClip", "f-clip.png", "FilterClips" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "clip", "weapclip" } );

	button = toolbar_append_toggle_button( toolbar, "HintsSkips (CTRL + H)\nRightClick: toggle tex\n\thint\n\thintLocal\n\thintSkip", "f-hint.png", "FilterHintsSkips" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "hint", "hintlocal", "hintskip" } );

	//toolbar_append_toggle_button( toolbar, "Paths (ALT + 8)", "texture_lock.png", "FilterPaths" );
	toolbar_append_space( toolbar );
	toolbar_append_toggle_button( toolbar, "Entities (ALT + 2)", "f-entities.png", "FilterEntities" );
	toolbar_append_toggle_button( toolbar, "Point Entities", "status_entiy.png", "FilterPointEntities" );
	toolbar_append_toggle_button( toolbar, "Lights (ALT + 0)", "f-lights.png", "FilterLights" );
	toolbar_append_toggle_button( toolbar, "Models (SHIFT + M)", "f-models.png", "FilterModels" );

	button = toolbar_append_toggle_button( toolbar, "Triggers (CTRL + SHIFT + T)\nRightClick: tex Trigger", "f-triggers.png", "FilterTriggers" );
	g_commonFuncs.emplace_back( button, std::vector<const char*>{ "trigger" } );

	//toolbar_append_toggle_button( toolbar, "Decals (SHIFT + D)", "f-decals.png", "FilterDecals" );
	toolbar_append_space( toolbar );
	//toolbar_append_button( toolbar, "InvertFilters", "f-invert.png", "InvertFilters" );

	toolbar_append_button( toolbar, "ResetFilters", "f-reset.png", "ResetFilters" );

	toolbar_append_space( toolbar );
	button = toolbar_append_toggle_button( toolbar, "Region Set Selection (CTRL + SHIFT + R)\nRightClick: Region Off", "f-region.png", "RegionSetSelection" );
	g_commonFuncs.emplace_back( button, "RegionOff" );

	button = toolbar_append_toggle_button( toolbar, "Hide Selected (H)\nRightClick: Show Hidden (SHIFT + H)", "f-hide.png", "HideSelected" );
	g_commonFuncs.emplace_back( button, "ShowHidden" );

	return toolbar;
}
