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

void SetCommonShader( const char* key, const char* shader ){
	const char* gotShader = g_pGameDescription->getKeyValue( key );
	UndoableCommand undo( "textureNameSetSelected" );
	if ( gotShader && *gotShader ){
		Select_SetShader( gotShader );
	}
	else{
		Select_SetShader( shader );
	}
}


gboolean Areaportals_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		if ( ButtonNum == 1 ){
			ToggleActions %= 2;
		}
		else{
			ToggleActions = 0;
			ButtonNum = 1;
		}
		if( ToggleActions == 0 ){
			SetCommonShader( "shader_nodraw", "textures/common/nodraw" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "shader_nodrawnonsolid", "textures/common/nodrawnonsolid" );
		}
		//SetCommonShader( "shader_caulk", "textures/common/caulk" );
		//globalOutputStream() << "Found '" << "fullname" << "'\n";
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}


gboolean Caulk_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		SetCommonShader( "shader_caulk", "textures/common/caulk" );
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
			SetCommonShader( "shader_clip", "textures/common/clip" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "shader_weapclip", "textures/common/weapclip" );
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
			SetCommonShader( "shader_watercaulk", "textures/common/watercaulk" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "shader_lavacaulk", "textures/common/lavacaulk" );
		}
		else if( ToggleActions == 2 ){
			SetCommonShader( "shader_slimecaulk", "textures/common/slimecaulk" );
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
			SetCommonShader( "shader_hint", "textures/common/hint" );
		}
		else if( ToggleActions == 1 ){
			SetCommonShader( "shader_hintlocal", "textures/common/hintlocal" );
		}
		else if( ToggleActions == 2 ){
			SetCommonShader( "shader_hintskip", "textures/common/hintskip" );
		}
		ToggleActions++;
		return TRUE;
	}
	return FALSE;
}

gboolean Trigger_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		SetCommonShader( "shader_trigger", "textures/common/trigger" );
		ToggleActions = 0;
		return TRUE;
	}
	return FALSE;
}

gboolean Func_Groups_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->button == 3 && event->type == GDK_BUTTON_PRESS ) {
		UndoableCommand undo( "create func_group" );
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


GtkToolbar* create_filter_toolbar(){
			GtkToolbar* filter_toolbar = GTK_TOOLBAR( gtk_toolbar_new() );
			gtk_widget_show( GTK_WIDGET( filter_toolbar ) );
			g_signal_connect( G_OBJECT( filter_toolbar ), "enter_notify_event", G_CALLBACK( ToggleActions0 ), 0 );

			GtkToggleButton* button;

			toolbar_append_toggle_button( filter_toolbar, "World (ALT + 1)", "f-world.png", "FilterWorldBrushes" );

			button = toolbar_append_toggle_button( filter_toolbar, "Structural (CTRL + SHIFT + D)\nRightClick: MakeStructural", "f-structural.png", "FilterStructural" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Structural_button_press ), 0 );

			button = toolbar_append_toggle_button( filter_toolbar, "Details (CTRL + D)\nRightClick: MakeDetail", "f-details.png", "FilterDetails" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Detail_button_press ), 0 );

			button = toolbar_append_toggle_button( filter_toolbar, "Func_Groups\nRightClick: create func_group", "f-funcgroups.png", "FilterFuncGroups" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Func_Groups_button_press ), 0 );

			toolbar_append_toggle_button( filter_toolbar, "Patches (CTRL + P)", "patch_wireframe.png", "FilterPatches" );
			gtk_toolbar_append_space( GTK_TOOLBAR( filter_toolbar ) );

			button = toolbar_append_toggle_button( filter_toolbar, "Areaportals (ALT + 3)\nRightClick: toggle tex\n\tnoDraw\n\tnoDrawNonSolid", "f-areaportal.png", "FilterAreaportals" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Areaportals_button_press ), 0 );



			toolbar_append_toggle_button( filter_toolbar, "Translucent (ALT + 4)", "f-translucent.png", "FilterTranslucent" );

			button = toolbar_append_toggle_button( filter_toolbar, "Liquids (ALT + 5)\nRightClick: toggle tex\n\twaterCaulk\n\tlavaCaulk\n\tslimeCaulk", "f-liquids.png", "FilterLiquids" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Liquids_button_press ), 0 );

			button = toolbar_append_toggle_button( filter_toolbar, "Caulk (ALT + 6)\nRightClick: tex Caulk", "f-caulk.png", "FilterCaulk" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Caulk_button_press ), 0 );

			button = toolbar_append_toggle_button( filter_toolbar, "Clips (ALT + 7)\nRightClick: toggle tex\n\tplayerClip\n\tweapClip", "f-clip.png", "FilterClips" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Clip_button_press ), 0 );

			button = toolbar_append_toggle_button( filter_toolbar, "HintsSkips (CTRL + H)\nRightClick: toggle tex\n\thint\n\thintLocal\n\thintSkip", "f-hint.png", "FilterHintsSkips" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Hint_button_press ), 0 );

			//toolbar_append_toggle_button( filter_toolbar, "Paths (ALT + 8)", "texture_lock.png", "FilterPaths" );
			gtk_toolbar_append_space( GTK_TOOLBAR( filter_toolbar ) );
			toolbar_append_toggle_button( filter_toolbar, "Entities (ALT + 2)", "f-entities.png", "FilterEntities" );
			toolbar_append_toggle_button( filter_toolbar, "Lights (ALT + 0)", "f-lights.png", "FilterLights" );
			toolbar_append_toggle_button( filter_toolbar, "Models (SHIFT + M)", "f-models.png", "FilterModels" );

			button = toolbar_append_toggle_button( filter_toolbar, "Triggers (CTRL + SHIFT + T)\nRightClick: tex Trigger", "f-triggers.png", "FilterTriggers" );
			g_signal_connect( G_OBJECT( button ), "button_press_event", G_CALLBACK( Trigger_button_press ), 0 );

			//toolbar_append_toggle_button( filter_toolbar, "Decals (SHIFT + D)", "f-decals.png", "FilterDecals" );
			gtk_toolbar_append_space( GTK_TOOLBAR( filter_toolbar ) );
			toolbar_append_button( filter_toolbar, "InvertFilters", "f-invert.png", "InvertFilters" );

			toolbar_append_button( filter_toolbar, "ResetFilters", "f-reset.png", "ResetFilters" );
			return filter_toolbar;
}
