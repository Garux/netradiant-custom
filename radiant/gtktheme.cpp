/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Wed Jan  1 19:06:46 GMT+4 2003
    copyright            : (C) 2003 - 2005 by Alex Shaduri
    email                : ashaduri '@' gmail.com
 ***************************************************************************/


#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sstream>
#include <gtk/gtk.h>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#endif

#include <unistd.h>


#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <gdk/gdkkeysyms.h>


#include "gtktheme.h"
#include "mainframe.h"
//#include "gtkutil/window.h"

// ------------------------------------------------------


std::string& get_orig_theme();
std::string& get_orig_font();

std::string get_current_theme();
std::string get_current_font();

std::string get_selected_theme();
std::string get_selected_font();

void set_theme(const std::string& theme_name, const std::string& font);
void apply_theme(const std::string& theme_name, const std::string& font);



GtkWidget* g_main_rc_window = NULL;


static std::string s_rc_file;



// ------------------------------------------------------



GtkWidget* lookup_widget (GtkWidget *widget, const gchar *widget_name){
    GtkWidget *parent, *found_widget;

    for (;;)
    {
        if (GTK_IS_MENU (widget))
            parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
        else
            parent = widget->parent;
        if (!parent)
            parent = (GtkWidget*)g_object_get_data (G_OBJECT (widget), "GladeParentKey");
        if (parent == NULL)
            break;
        widget = parent;
    }

    found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget),
                   widget_name);
    if (!found_widget)
        g_warning ("Widget not found: %s", widget_name);
    return found_widget;
}






void on_main_cancel_button_clicked( GtkButton *button, gpointer user_data ){
	set_theme( get_orig_theme(), get_orig_font() );
	gtk_widget_destroy( g_main_rc_window );
	g_main_rc_window = NULL;
}


void on_main_reset_button_clicked( GtkButton *button, gpointer user_data ){
	set_theme( get_orig_theme(), get_orig_font() );
}


gboolean on_main_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	set_theme( get_orig_theme(), get_orig_font() );
	gtk_widget_destroy( g_main_rc_window );
	g_main_rc_window = NULL;
	return TRUE;
}


void on_main_use_default_font_radio_toggled ( GtkToggleButton *togglebutton, gpointer user_data ){
	bool default_font = gtk_toggle_button_get_active( togglebutton );

	gtk_widget_set_sensitive( lookup_widget( g_main_rc_window, "main_font_selector_button" ), !default_font );

	apply_theme( get_selected_theme(), get_selected_font() );
}


void on_main_font_selector_button_font_set( GtkFontButton *fontbutton, gpointer user_data ){
	apply_theme( get_selected_theme(), get_selected_font() );
}

void on_main_ok_button_clicked( GtkButton *button, gpointer user_data ){
	gtk_widget_destroy( g_main_rc_window );
	g_main_rc_window = NULL;
}



#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget*
create_rc_window (void)
{
  GtkWidget *main_window;
  GtkWidget *main_hbox;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *frame2;
  GtkWidget *alignment3;
  GtkWidget *scrolledwindow3;
  GtkWidget *main_themelist;
  GtkWidget *label1234;
  GtkWidget *frame3;
  GtkWidget *alignment4;
  GtkWidget *vbox7;
  GtkWidget *hbox8;
  GtkWidget *vbox9;
  GtkWidget *main_use_default_font_radio;
  GSList *main_use_default_font_radio_group = NULL;
  GtkWidget *main_use_custom_font_radio;
  GtkWidget *alignment5;
  GtkWidget *vbox10;
  GtkWidget *hbox9;
  GtkWidget *vbox11;
  GtkWidget *main_font_selector_button;
  GtkWidget *label669;
  GtkWidget *vbox13;
  GtkWidget *hbuttonbox1;
  GtkWidget *hbox7;
  GtkWidget *vbox6;
  GtkWidget *hbox5;
  GtkWidget *main_ok_button;
  GtkWidget *main_cancel_button;
  GtkWidget *main_reset_button;
  GtkWidget *alignment2;
  GtkWidget *hbox6;
  GtkWidget *image1;
  GtkWidget *label667;
  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  accel_group = gtk_accel_group_new ();

  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (main_window, "main_window");
  gtk_window_set_title (GTK_WINDOW (main_window), "Gtk2 Theme Selector");
  gtk_window_set_transient_for( GTK_WINDOW (main_window), MainFrame_getWindow() );
  gtk_window_set_destroy_with_parent( GTK_WINDOW (main_window), TRUE );

  //gtk_window_set_keep_above ( GTK_WINDOW( main_window ), TRUE );

  main_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (main_hbox, "main_hbox");
  gtk_widget_show (main_hbox);
  gtk_container_add (GTK_CONTAINER (main_window), main_hbox);
  gtk_widget_set_size_request (main_hbox, 310, 320);
  gtk_window_resize(GTK_WINDOW(main_window), 310, 640);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox1, "vbox1");
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (main_hbox), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox1, "hbox1");
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, TRUE, TRUE, 0);

  frame2 = gtk_frame_new (NULL);
  gtk_widget_set_name (frame2, "frame2");
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (hbox1), frame2, TRUE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_NONE);

  alignment3 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_set_name (alignment3, "alignment3");
  gtk_widget_show (alignment3);
  gtk_container_add (GTK_CONTAINER (frame2), alignment3);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment3), 0, 0, 12, 0);

  scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow3, "scrolledwindow3");
  gtk_widget_show (scrolledwindow3);
  gtk_container_add (GTK_CONTAINER (alignment3), scrolledwindow3);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow3), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow3), GTK_SHADOW_IN);

  main_themelist = gtk_tree_view_new ();
  gtk_widget_set_name (main_themelist, "main_themelist");
  gtk_widget_show (main_themelist);
  gtk_container_add (GTK_CONTAINER (scrolledwindow3), main_themelist);
  GTK_WIDGET_SET_FLAGS (main_themelist, GTK_CAN_DEFAULT);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (main_themelist), FALSE);

  label1234 = gtk_label_new ("<b>Theme</b>");
  gtk_widget_set_name (label1234, "label1234");
  gtk_widget_show (label1234);
  gtk_frame_set_label_widget (GTK_FRAME (frame2), label1234);
  gtk_label_set_use_markup (GTK_LABEL (label1234), TRUE);

  frame3 = gtk_frame_new (NULL);
  gtk_widget_set_name (frame3, "frame3");
  gtk_widget_show (frame3);
  gtk_box_pack_start (GTK_BOX (vbox1), frame3, FALSE, FALSE, 9);
  gtk_frame_set_shadow_type (GTK_FRAME (frame3), GTK_SHADOW_NONE);

  alignment4 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_set_name (alignment4, "alignment4");
  gtk_widget_show (alignment4);
  gtk_container_add (GTK_CONTAINER (frame3), alignment4);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment4), 0, 0, 12, 0);

  vbox7 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox7, "vbox7");
  gtk_widget_show (vbox7);
  gtk_container_add (GTK_CONTAINER (alignment4), vbox7);

  hbox8 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox8, "hbox8");
  gtk_widget_show (hbox8);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox8, FALSE, FALSE, 0);

  vbox9 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox9, "vbox9");
  gtk_widget_show (vbox9);
  gtk_box_pack_start (GTK_BOX (hbox8), vbox9, TRUE, TRUE, 0);

  main_use_default_font_radio = gtk_radio_button_new_with_mnemonic (NULL, "Use theme default font");
  gtk_widget_set_name (main_use_default_font_radio, "main_use_default_font_radio");
  gtk_widget_show (main_use_default_font_radio);
  gtk_box_pack_start (GTK_BOX (vbox9), main_use_default_font_radio, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (main_use_default_font_radio), main_use_default_font_radio_group);
  main_use_default_font_radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (main_use_default_font_radio));

  main_use_custom_font_radio = gtk_radio_button_new_with_mnemonic (NULL, "Use custom font:");
  gtk_widget_set_name (main_use_custom_font_radio, "main_use_custom_font_radio");
  gtk_widget_show (main_use_custom_font_radio);
  gtk_box_pack_start (GTK_BOX (vbox9), main_use_custom_font_radio, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (main_use_custom_font_radio), main_use_default_font_radio_group);
  main_use_default_font_radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (main_use_custom_font_radio));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (main_use_custom_font_radio), TRUE);
  //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (main_use_custom_font_radio), FALSE);

  alignment5 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_set_name (alignment5, "alignment5");
  gtk_widget_show (alignment5);
  gtk_box_pack_start (GTK_BOX (vbox9), alignment5, TRUE, TRUE, 0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment5), 0, 0, 12, 0);

  vbox10 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox10, "vbox10");
  gtk_widget_show (vbox10);
  gtk_container_add (GTK_CONTAINER (alignment5), vbox10);

  hbox9 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox9, "hbox9");
  gtk_widget_show (hbox9);
  gtk_box_pack_start (GTK_BOX (vbox10), hbox9, FALSE, FALSE, 0);

  vbox11 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox11, "vbox11");
  gtk_widget_show (vbox11);
  gtk_box_pack_start (GTK_BOX (hbox9), vbox11, TRUE, TRUE, 0);

  main_font_selector_button = gtk_font_button_new ();
  gtk_widget_set_name (main_font_selector_button, "main_font_selector_button");
  gtk_widget_show (main_font_selector_button);
  gtk_box_pack_start (GTK_BOX (vbox11), main_font_selector_button, FALSE, FALSE, 0);

  label669 = gtk_label_new ("<b>Font</b>");
  gtk_widget_set_name (label669, "label669");
  gtk_widget_show (label669);
  gtk_frame_set_label_widget (GTK_FRAME (frame3), label669);
  gtk_label_set_use_markup (GTK_LABEL (label669), TRUE);

  vbox13 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox13, "vbox13");
  gtk_widget_show (vbox13);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox13, FALSE, FALSE, 0);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_set_name (hbuttonbox1, "hbuttonbox1");
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, FALSE, FALSE, 0);

  hbox7 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox7, "hbox7");
  gtk_widget_show (hbox7);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox7, FALSE, TRUE, 6);

  vbox6 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox6, "vbox6");
  gtk_widget_show (vbox6);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox6, FALSE, FALSE, 0);

  hbox5 = gtk_hbox_new (TRUE, 0);
  gtk_widget_set_name (hbox5, "hbox5");
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (vbox6), hbox5, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox5), 4);

  main_ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_set_name (main_ok_button, "main_ok_button");
  gtk_widget_show (main_ok_button);
  gtk_box_pack_end (GTK_BOX (hbox5), main_ok_button, TRUE, TRUE, 4);
  GTK_WIDGET_SET_FLAGS (main_ok_button, GTK_CAN_DEFAULT);

  main_cancel_button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_set_name (main_cancel_button, "main_cancel_button");
  gtk_widget_show (main_cancel_button);
  gtk_box_pack_end (GTK_BOX (hbox5), main_cancel_button, TRUE, TRUE, 4);
  GTK_WIDGET_SET_FLAGS (main_cancel_button, GTK_CAN_DEFAULT);

  main_reset_button = gtk_button_new ();
  gtk_widget_set_name (main_reset_button, "main_reset_button");
  gtk_widget_show (main_reset_button);
  gtk_box_pack_end (GTK_BOX (hbox5), main_reset_button, TRUE, TRUE, 4);

  alignment2 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_set_name (alignment2, "alignment2");
  gtk_widget_show (alignment2);
  gtk_container_add (GTK_CONTAINER (main_reset_button), alignment2);

  hbox6 = gtk_hbox_new (FALSE, 2);
  gtk_widget_set_name (hbox6, "hbox6");
  gtk_widget_show (hbox6);
  gtk_container_add (GTK_CONTAINER (alignment2), hbox6);

  image1 = gtk_image_new_from_stock ("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_name (image1, "image1");
  gtk_widget_show (image1);
  gtk_box_pack_start (GTK_BOX (hbox6), image1, FALSE, FALSE, 0);

  label667 = gtk_label_new_with_mnemonic ("_Reset");
  gtk_widget_set_name (label667, "label667");
  gtk_widget_show (label667);
  gtk_box_pack_start (GTK_BOX (hbox6), label667, FALSE, FALSE, 0);


  g_signal_connect ((gpointer) main_window, "delete_event",
                    G_CALLBACK (on_main_window_delete_event),
                    NULL);
  g_signal_connect ((gpointer) main_use_default_font_radio, "toggled",
                    G_CALLBACK (on_main_use_default_font_radio_toggled),
                    NULL);
  g_signal_connect ((gpointer) main_font_selector_button, "font_set",
                    G_CALLBACK (on_main_font_selector_button_font_set),
                    NULL);
  g_signal_connect ((gpointer) main_cancel_button, "clicked",
                    G_CALLBACK (on_main_cancel_button_clicked),
                    NULL);
  g_signal_connect ((gpointer) main_reset_button, "clicked",
                    G_CALLBACK (on_main_reset_button_clicked),
                    NULL);
  g_signal_connect ((gpointer) main_ok_button, "clicked",
                    G_CALLBACK (on_main_ok_button_clicked),
                    NULL);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (main_window, main_window, "main_window");
  GLADE_HOOKUP_OBJECT (main_window, main_hbox, "main_hbox");
  GLADE_HOOKUP_OBJECT (main_window, vbox1, "vbox1");
  GLADE_HOOKUP_OBJECT (main_window, hbox1, "hbox1");
  GLADE_HOOKUP_OBJECT (main_window, frame2, "frame2");
  GLADE_HOOKUP_OBJECT (main_window, alignment3, "alignment3");
  GLADE_HOOKUP_OBJECT (main_window, scrolledwindow3, "scrolledwindow3");
  GLADE_HOOKUP_OBJECT (main_window, main_themelist, "main_themelist");
  GLADE_HOOKUP_OBJECT (main_window, label1234, "label1234");
  GLADE_HOOKUP_OBJECT (main_window, frame3, "frame3");
  GLADE_HOOKUP_OBJECT (main_window, alignment4, "alignment4");
  GLADE_HOOKUP_OBJECT (main_window, vbox7, "vbox7");
  GLADE_HOOKUP_OBJECT (main_window, hbox8, "hbox8");
  GLADE_HOOKUP_OBJECT (main_window, vbox9, "vbox9");
  GLADE_HOOKUP_OBJECT (main_window, main_use_default_font_radio, "main_use_default_font_radio");
  GLADE_HOOKUP_OBJECT (main_window, main_use_custom_font_radio, "main_use_custom_font_radio");
  GLADE_HOOKUP_OBJECT (main_window, alignment5, "alignment5");
  GLADE_HOOKUP_OBJECT (main_window, vbox10, "vbox10");
  GLADE_HOOKUP_OBJECT (main_window, hbox9, "hbox9");
  GLADE_HOOKUP_OBJECT (main_window, vbox11, "vbox11");
  GLADE_HOOKUP_OBJECT (main_window, main_font_selector_button, "main_font_selector_button");
  GLADE_HOOKUP_OBJECT (main_window, label669, "label669");
  GLADE_HOOKUP_OBJECT (main_window, vbox13, "vbox13");
  GLADE_HOOKUP_OBJECT (main_window, hbuttonbox1, "hbuttonbox1");
  GLADE_HOOKUP_OBJECT (main_window, hbox7, "hbox7");
  GLADE_HOOKUP_OBJECT (main_window, vbox6, "vbox6");
  GLADE_HOOKUP_OBJECT (main_window, hbox5, "hbox5");
  GLADE_HOOKUP_OBJECT (main_window, main_ok_button, "main_ok_button");
  GLADE_HOOKUP_OBJECT (main_window, main_cancel_button, "main_cancel_button");
  GLADE_HOOKUP_OBJECT (main_window, main_reset_button, "main_reset_button");
  GLADE_HOOKUP_OBJECT (main_window, alignment2, "alignment2");
  GLADE_HOOKUP_OBJECT (main_window, hbox6, "hbox6");
  GLADE_HOOKUP_OBJECT (main_window, image1, "image1");
  GLADE_HOOKUP_OBJECT (main_window, label667, "label667");

  GLADE_HOOKUP_OBJECT_NO_REF (main_window, tooltips, "tooltips");

  gtk_widget_grab_default (main_themelist);
  gtk_window_add_accel_group (GTK_WINDOW (main_window), accel_group);

  return main_window;
}




static std::string gchar_to_string(gchar* gstr)
{
	std::string str = (gstr ? gstr : "");
	g_free(gstr);
	return str;
}



// ------------------------------------------------------


static std::string s_orig_theme;
static std::string s_orig_font;

std::string& get_orig_theme()
{
	return s_orig_theme;
}


std::string& get_orig_font()
{
	return s_orig_font;
}


// ------------------------------------------------------


std::string get_current_theme()
{

	GtkSettings* settings = gtk_settings_get_default();
	gchar* theme;
	g_object_get(settings, "gtk-theme-name", &theme, NULL);

	/* dummy check */
	if( !g_ascii_isalnum( theme[0] ) ){
		g_free( theme );
		return "";
	}
	return gchar_to_string(theme);
}




std::string get_current_font()
{
	return gchar_to_string( pango_font_description_to_string( gtk_rc_get_style( g_main_rc_window )->font_desc ) );
}



// ------------------------------------------------------



std::string get_selected_theme()
{
	GtkTreeView* treeview = GTK_TREE_VIEW(lookup_widget(g_main_rc_window, "main_themelist"));
	GtkTreeModel* model = gtk_tree_view_get_model(treeview);
	GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

	GtkTreeIter iter;
	gtk_tree_selection_get_selected(selection, 0, &iter);

	gchar* theme_name;
	gtk_tree_model_get(model, &iter, 0, &theme_name, -1);
// 	std::cout << theme_name << "\n";
	return gchar_to_string(theme_name);
}



std::string get_selected_font()
{
// 	GtkWidget* fontentry = lookup_widget(g_main_rc_window, "main_fontentry");
// 	return gtk_entry_get_text(GTK_ENTRY(fontentry));

	bool default_font = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(g_main_rc_window, "main_use_default_font_radio")));

	if (default_font)
		return "";

	GtkWidget* fontbutton = lookup_widget(g_main_rc_window, "main_font_selector_button");
	return gtk_font_button_get_font_name(GTK_FONT_BUTTON(fontbutton));
}


// ------------------------------------------------------



static void themelist_selection_changed_cb(GtkTreeSelection* selection, gpointer data)
{
	if (gtk_tree_selection_get_selected (selection, 0, 0))
		apply_theme(get_selected_theme(), get_current_font());
}



// ------------------------------------------------------



static void populate_with_themes(GtkWidget* w)
{

	std::string search_path = gchar_to_string(gtk_rc_get_theme_dir());

	if (search_path.size() && search_path[search_path.size() -1] != G_DIR_SEPARATOR)
		search_path += G_DIR_SEPARATOR_S;

	GDir* gdir = g_dir_open(search_path.c_str(), 0, NULL);
	if (gdir == NULL)
		return;


	char* name;
	GList* glist = 0;

	while ( (name = const_cast<char*>(g_dir_read_name(gdir))) != NULL ) {
		std::string filename = name;

//		if (g_ascii_strup(fname.c_str(), -1) == "Default")
//			continue;

		std::string fullname = search_path + filename;
		std::string rc = fullname; rc += G_DIR_SEPARATOR_S; rc += "gtk-2.0"; rc += G_DIR_SEPARATOR_S; rc += "gtkrc";

		bool is_dir = 0;
		if (g_file_test(fullname.c_str(), G_FILE_TEST_IS_DIR))
			is_dir = 1;

		if (is_dir && g_file_test(rc.c_str(), G_FILE_TEST_IS_REGULAR)) {
			glist = g_list_insert_sorted(glist, g_strdup(filename.c_str()), (GCompareFunc)strcmp);
		}
	}

	g_dir_close(gdir);




	// ---------------- tree


	GtkTreeView* treeview = GTK_TREE_VIEW(w);
	GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));

	GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes (
												"Theme", gtk_cell_renderer_text_new(),
												"text", 0,
												NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);


	GtkTreeIter   iter;

	int i =0, curr=0;
	while (char* theme = (char*)g_list_nth_data(glist, i)) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, theme, -1);

		if (strcmp(theme, get_current_theme().c_str()) == 0) {
			curr = i;
		}

		++i;
	}


	GtkTreeSelection* selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	// set the default theme

	// THIS IS IMPORTANT!!!
	gtk_widget_grab_focus(w);

	std::stringstream str;
	str << curr;
	GtkTreePath* selpath = gtk_tree_path_new_from_string (str.str().c_str());
	if (selpath) {
		gtk_tree_selection_select_path(selection, selpath);
		gtk_tree_view_scroll_to_cell(treeview, selpath, NULL, true, 0.5, 0.0);
		gtk_tree_path_free(selpath);
	}

	g_signal_connect (G_OBJECT (selection), "changed",
                  G_CALLBACK (themelist_selection_changed_cb), NULL);

	g_object_unref (G_OBJECT (store));


	// ---------------- font


	GtkWidget* fontbutton = lookup_widget(g_main_rc_window, "main_font_selector_button");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(fontbutton), get_current_font().c_str());


}




// ------------------------------------------------------




void gtkThemeDlg(){
	if( g_main_rc_window ) return;

	s_rc_file = std::string( AppPath_get() ) + G_DIR_SEPARATOR_S + ".gtkrc-2.0.radiant";

	g_main_rc_window = create_rc_window();

    populate_with_themes( lookup_widget( g_main_rc_window, "main_themelist" ) );

	get_orig_theme() = get_current_theme();
	get_orig_font() = get_current_font();

	gtk_widget_show ( g_main_rc_window );
}



// -------------------------------



void set_theme(const std::string& theme_name, const std::string& font)
{
	if( theme_name.empty() ) return;

	// tree
	GtkTreeView* treeview = GTK_TREE_VIEW(lookup_widget(g_main_rc_window, "main_themelist"));
	GtkTreeModel* model = gtk_tree_view_get_model(treeview);
	GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

	GtkTreeIter iter;
	gtk_tree_model_get_iter_first(model, &iter);

	while(gtk_tree_model_iter_next(model, &iter)) {

		gchar* text;
		gtk_tree_model_get (model, &iter, 0, &text, -1);
		std::string theme = gchar_to_string(text);

		if (theme_name == theme) {
			gtk_tree_selection_select_iter(selection, &iter);
			break;
		}

	}


	// font
	if (font != "") {
		GtkWidget* fontbutton = lookup_widget(g_main_rc_window, "main_font_selector_button");
		//gtk_font_button_set_font_name(GTK_FONT_BUTTON(fontbutton), get_current_font().c_str());
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(fontbutton), font.c_str());
	}


	apply_theme(get_selected_theme(), get_selected_font());

}




void apply_theme(const std::string& theme_name, const std::string& font)
{

	std::stringstream strstr;
	strstr << "gtk-theme-name = \"" << theme_name << "\"\n";

	if (font != "")
		strstr << "style \"user-font\"\n{\nfont_name=\"" << font << "\"\n}\nwidget_class \"*\" style \"user-font\"";

	//strstr << "\ngtk-menu-popup-delay = 10";

// 	std::cout << strstr.str() << "\n\n\n";
	std::fstream f;
	f.open(s_rc_file.c_str(), std::ios::out);
		f << strstr.str();
	f.close();


	GtkSettings* settings = gtk_settings_get_default();

  	gtk_rc_reparse_all_for_settings (settings, true);
// 	gtk_rc_parse_string(strstr.str().c_str());
//  	gtk_rc_parse("/root/.gtk-tmp");
//  	gtk_rc_reset_styles(settings);

	//unlink(s_rc_file.c_str());

	while (gtk_events_pending())
		gtk_main_iteration();


}
