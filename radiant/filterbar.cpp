#include "filterbar.h"
#include <gtk/gtktoolbar.h>
#include "gtkmisc.h"
#include "gtkutil/widget.h"
#include "stream/stringstream.h"
#include "select.h"
#include "iundo.h"
#include "preferences.h"

#include "commands.h"
#include "gtkutil/accelerator.h"
#include "generic/callback.h"

#include "entity.h"


int ToggleActions = 0;
int ButtonNum = 0;

gboolean ToggleActions0( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	ToggleActions = 0;
	return FALSE;
	//globalOutputStream() << "ToggleActions\n";
}

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

const char* g_caulk_shader = 0;

const char* GetCaulkShader(){
	if( !g_caulk_shader ){
		g_caulk_shader = strdup( GetCommonShader( "caulk" ).c_str() );
	}
	return g_caulk_shader;
}

gboolean Translucent_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		if ( ButtonNum == 1 ){
			ToggleActions %= 2;
		}
		else{
			ToggleActions = 0;
			ButtonNum = 1;
		}
		if( ToggleActions == 0 ){
			SetCommonShader( "nodraw" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "nodrawnonsolid" );
		}
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}


gboolean Caulk_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		SetCommonShader( "caulk" );
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Clip_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		if ( ButtonNum == 3 ){
			ToggleActions %= 2;
		}
		else{
			ToggleActions = 0;
			ButtonNum = 3;
		}
		if( ToggleActions == 0 ){
			SetCommonShader( "clip" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "weapclip" );
		}
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}

gboolean Liquids_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		if ( ButtonNum == 4 ){
			ToggleActions %= 3;
		}
		else{
			ToggleActions = 0;
			ButtonNum = 4;
		}
		if( ToggleActions == 0 ){
			SetCommonShader( "watercaulk" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "lavacaulk" );
		}
		else if( ToggleActions == 2 ){
			SetCommonShader( "slimecaulk" );
		}
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}


gboolean Hint_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		if ( ButtonNum == 5 ){
			ToggleActions %= 3;
		}
		else{
			ToggleActions = 0;
			ButtonNum = 5;
		}
		if( ToggleActions == 0 ){
			SetCommonShader( "hint" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "hintlocal" );
		}
		else if( ToggleActions == 2 ){
			SetCommonShader( "hintskip" );
		}
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}

gboolean Trigger_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		SetCommonShader( "trigger" );
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Func_Groups_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		Entity_createFromSelection( "func_group", g_vector3_identity );
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Detail_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		GlobalCommands_find( "MakeDetail" ).m_callback();
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Structural_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		GlobalCommands_find( "MakeStructural" ).m_callback();
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Region_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		GlobalCommands_find( "RegionOff" ).m_callback();
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Hide_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		GlobalCommands_find( "ShowHidden" ).m_callback();
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

GtkToolbar* create_filter_toolbar(){
			GtkToolbar* toolbar = GTK_TOOLBAR( gtk_toolbar_new() );
			gtk_toolbar_set_style( toolbar, GTK_TOOLBAR_ICONS );
//			gtk_toolbar_set_show_arrow( toolbar, TRUE );
			gtk_widget_show( GTK_WIDGET( toolbar ) );
			g_signal_connect( G_OBJECT( toolbar ), "enter_notify_event", G_CALLBACK( ToggleActions0 ), 0 );

			GtkToggleButton* button;

			toolbar_append_toggle_button( toolbar, "World (ALT + 1)", "f-world.png", "FilterWorldBrushes" );

			button = toolbar_append_toggle_button( toolbar, "Structural (CTRL + SHIFT + D)\nRightClick: MakeStructural", "f-structural.png", "FilterStructural" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Structural_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Details (CTRL + D)\nRightClick: MakeDetail", "f-details.png", "FilterDetails" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Detail_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Func_Groups\nRightClick: create func_group", "f-funcgroups.png", "FilterFuncGroups" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Func_Groups_button_press ), 0 );

			toolbar_append_toggle_button( toolbar, "Patches (CTRL + P)", "patch_wireframe.png", "FilterPatches" );
			gtk_toolbar_append_space( GTK_TOOLBAR( toolbar ) );

//			if ( g_pGameDescription->mGameType == "doom3" ) {
//				button = toolbar_append_toggle_button( toolbar, "Visportals (ALT + 3)\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-areaportal.png", "FilterVisportals" );
//			}
//			else{
//				button = toolbar_append_toggle_button( toolbar, "Areaportals (ALT + 3)\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-areaportal.png", "FilterAreaportals" );
//			}
//			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Translucent_button_press ), 0 );


			button = toolbar_append_toggle_button( toolbar, "Translucent (ALT + 4)\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-translucent.png", "FilterTranslucent" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Translucent_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Liquids (ALT + 5)\nRightClick: toggle tex\n\twaterCaulk\n\tlavaCaulk\n\tslimeCaulk", "f-liquids.png", "FilterLiquids" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Liquids_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Caulk (ALT + 6)\nRightClick: tex Caulk", "f-caulk.png", "FilterCaulk" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Caulk_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Clips (ALT + 7)\nRightClick: toggle tex\n\tplayerClip\n\tweapClip", "f-clip.png", "FilterClips" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Clip_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "HintsSkips (CTRL + H)\nRightClick: toggle tex\n\thint\n\thintLocal\n\thintSkip", "f-hint.png", "FilterHintsSkips" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Hint_button_press ), 0 );

			//toolbar_append_toggle_button( toolbar, "Paths (ALT + 8)", "texture_lock.png", "FilterPaths" );
			gtk_toolbar_append_space( GTK_TOOLBAR( toolbar ) );
			toolbar_append_toggle_button( toolbar, "Entities (ALT + 2)", "f-entities.png", "FilterEntities" );
			toolbar_append_toggle_button( toolbar, "Lights (ALT + 0)", "f-lights.png", "FilterLights" );
			toolbar_append_toggle_button( toolbar, "Models (SHIFT + M)", "f-models.png", "FilterModels" );

			button = toolbar_append_toggle_button( toolbar, "Triggers (CTRL + SHIFT + T)\nRightClick: tex Trigger", "f-triggers.png", "FilterTriggers" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Trigger_button_press ), 0 );

			//toolbar_append_toggle_button( toolbar, "Decals (SHIFT + D)", "f-decals.png", "FilterDecals" );
			gtk_toolbar_append_space( GTK_TOOLBAR( toolbar ) );
			//toolbar_append_button( toolbar, "InvertFilters", "f-invert.png", "InvertFilters" );

			toolbar_append_button( toolbar, "ResetFilters", "f-reset.png", "ResetFilters" );

			gtk_toolbar_append_space( GTK_TOOLBAR( toolbar ) );
			button = toolbar_append_toggle_button( toolbar, "Region Set Selection (CTRL + SHIFT + R)\nRightClick: Region Off", "f-region.png", "RegionSetSelection" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Region_button_press ), 0 );

			button = toolbar_append_toggle_button( toolbar, "Hide Selected (H)\nRightClick: Show Hidden (SHIFT + H)", "f-hide.png", "HideSelected" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Hide_button_press ), 0 );

			return toolbar;
}
