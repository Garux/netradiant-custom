/**
 * @file GeneralFunctionDialog.cpp
 * Implements the GeneralFunctionDialog class.
 * @ingroup meshtex-ui
 */

/*
 * Copyright 2012 Joel Baxter
 *
 * This file is part of MeshTex.
 *
 * MeshTex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * MeshTex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshTex.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#include "GenericPluginUI.h"
#include "GeneralFunctionDialog.h"
#include "PluginUIMessages.h"

#include "iundo.h"


/**
 * Constructor. See MeshEntity::GeneralFunction for details of how these
 * arguments are interpreted.
 *
 * @param sFactors      Factors to determine the S texture coords; NULL if S
 *                      axis unaffected.
 * @param tFactors      Factors to determine the T texture coords; NULL if T
 *                      axis unaffected.
 * @param alignRow      Pointer to zero-point row; if NULL, row 0 is assumed.
 * @param alignCol      Pointer to zero-point column; if NULL, column 0 is
 *                      assumed.
 * @param refRow        Pointer to reference row description, including how
 *                      to use the reference; NULL if no reference.
 * @param refCol        Pointer to reference column description, including
 *                      how to use the reference; NULL if no reference.
 * @param surfaceValues true if calculations are for S/T values on the mesh
 *                      surface; false if calculations are for S/T values at
 *                      the control points.
 */
GeneralFunctionDialog::GeneralFunctionVisitor::GeneralFunctionVisitor(
   const MeshEntity::GeneralFunctionFactors *sFactors,
   const MeshEntity::GeneralFunctionFactors *tFactors,
   const MeshEntity::SliceDesignation *alignRow,
   const MeshEntity::SliceDesignation *alignCol,
   const MeshEntity::RefSliceDescriptor *refRow,
   const MeshEntity::RefSliceDescriptor *refCol,
   bool surfaceValues) :
   _sFactors(sFactors),
   _tFactors(tFactors),
   _alignRow(alignRow),
   _alignCol(alignCol),
   _refRow(refRow),
   _refCol(refCol),
   _surfaceValues(surfaceValues)
{
}

/**
 * Visitor action; invoke MeshEntity::GeneralFunction on a mesh.
 *
 * @param [in,out] meshEntity The mesh.
 *
 * @return true.
 */
bool
GeneralFunctionDialog::GeneralFunctionVisitor::Execute(MeshEntity& meshEntity) const
{
   meshEntity.GeneralFunction(_sFactors, _tFactors,
                              _alignRow, _alignCol, _refRow, _refCol,
                              _surfaceValues);
   return true;
}

/**
 * Constructor. Configure the dialog window and create all the contained
 * widgets. Connect widgets to callbacks as necessary.
 *
 * @param key The unique key identifying this dialog.
 */
GeneralFunctionDialog::GeneralFunctionDialog(const std::string& key) :
   GenericDialog(key),
   _nullVisitor(new MeshVisitor())
{
   // Enable the usual handling of the close event.
   CreateWindowCloseCallback();

   // Configure the dialog window.
   gtk_window_set_resizable(GTK_WINDOW(_dialog), FALSE);
   gtk_window_set_title(GTK_WINDOW(_dialog), DIALOG_GEN_FUNC_TITLE);
   gtk_container_set_border_width(GTK_CONTAINER(_dialog), 10);

   // Create the contained widgets.

   GtkWidget *table;
   GtkWidget *entry;
   GtkWidget *applybutton, *refbutton, *button;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *mainvbox, *vbox, *hbox, *mainhbox;
   GtkWidget *frame;

   table = gtk_table_new(6, 13, FALSE);
   gtk_table_set_row_spacing(GTK_TABLE(table), 0, 5);
   gtk_table_set_row_spacing(GTK_TABLE(table), 1, 10);
   gtk_table_set_row_spacing(GTK_TABLE(table), 3, 15);
   gtk_table_set_row_spacing(GTK_TABLE(table), 4, 15);
   gtk_container_add(GTK_CONTAINER(_dialog), table);
   gtk_widget_show(table);

   hbox = gtk_hbox_new(TRUE, 10);
   gtk_table_attach(GTK_TABLE(table), hbox, 0, 13, 0, 1, GTK_SHRINK, GTK_EXPAND, 0, 0);
   gtk_widget_show(hbox);

   // Mutually exclusive "Surface values" and "Control values" radio buttons.

   button = gtk_radio_button_new_with_label(NULL,
                                            DIALOG_GEN_FUNC_SURFACE_VALUES);
   gtk_object_set_data(GTK_OBJECT(_dialog), "surface", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_GEN_FUNC_CONTROL_VALUES);
   gtk_object_set_data(GTK_OBJECT(_dialog), "control", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   separator = gtk_hseparator_new();
   gtk_table_attach_defaults(GTK_TABLE(table), separator, 0, 13, 1, 2);
   gtk_widget_show(separator);

   // Checkbox for the "S" row of factors. All the other widgets on this row
   // will have a dependence registered on this checkbox; i.e. they will only
   // be active when it is checked.

   applybutton = gtk_check_button_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_apply", applybutton);
   gtk_table_attach_defaults(GTK_TABLE(table), applybutton, 0, 1, 2, 3);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(applybutton), TRUE);
   gtk_widget_show(applybutton);

   label = gtk_label_new(DIALOG_GEN_FUNC_S_FUNC_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_oldval", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_OLD_S_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 3, 4, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_rowdist", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 4, 5, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_ROW_DIST_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 5, 6, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_coldist", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 6, 7, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_COL_DIST_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 7, 8, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_rownum", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 8, 9, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_ROW_NUM_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 9, 10, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_colnum", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 10, 11, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_COL_NUM_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 11, 12, 2, 3);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_constant", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 12, 13, 2, 3);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   // Checkbox for the "T" row of factors. All the other widgets on this row
   // will have a dependence registered on this checkbox; i.e. they will only
   // be active when it is checked.

   applybutton = gtk_check_button_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_apply", applybutton);
   gtk_table_attach_defaults(GTK_TABLE(table), applybutton, 0, 1, 3, 4);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(applybutton), TRUE);
   gtk_widget_show(applybutton);

   label = gtk_label_new(DIALOG_GEN_FUNC_T_FUNC_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_oldval", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_OLD_T_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 3, 4, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_rowdist", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 4, 5, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_ROW_DIST_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 5, 6, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_coldist", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 6, 7, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_COL_DIST_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 7, 8, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_rownum", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 8, 9, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_ROW_NUM_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 9, 10, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_colnum", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 10, 11, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   label = gtk_label_new(DIALOG_GEN_FUNC_COL_NUM_LABEL);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 11, 12, 3, 4);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_constant", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0.0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 12, 13, 3, 4);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);

   mainhbox = gtk_hbox_new(TRUE, 0);
   gtk_table_attach(GTK_TABLE(table), mainhbox, 0, 13, 4, 5, GTK_SHRINK, GTK_EXPAND, 0, 0);
   gtk_widget_show(mainhbox);

   mainvbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainhbox), mainvbox, FALSE, FALSE, 0);
   gtk_widget_show(mainvbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   frame = gtk_frame_new(DIALOG_GEN_FUNC_COL_ALIGN_FRAME_LABEL);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the alignment column.

   button = gtk_radio_button_new(NULL);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_num_align", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_GEN_FUNC_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_max_align", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the reference row & usage.

   refbutton = gtk_check_button_new_with_label(DIALOG_GEN_FUNC_REF_ROW_FRAME_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_ref", refbutton);
   gtk_widget_show(refbutton);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), refbutton);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new(NULL);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_num_ref", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_set_sensitive(entry, FALSE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(refbutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_GEN_FUNC_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_max_ref", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_check_button_new_with_label(DIALOG_GEN_FUNC_REF_TOTAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_ref_total", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   mainvbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(mainhbox), mainvbox, FALSE, FALSE, 0);
   gtk_widget_show(mainvbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   frame = gtk_frame_new(DIALOG_GEN_FUNC_ROW_ALIGN_FRAME_LABEL);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the alignment row.

   button = gtk_radio_button_new(NULL);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_num_align", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_GEN_FUNC_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_max_align", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the reference column & usage.

   refbutton = gtk_check_button_new_with_label(DIALOG_GEN_FUNC_REF_COL_FRAME_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_ref", refbutton);
   gtk_widget_show(refbutton);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), refbutton);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new(NULL);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_num_ref", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_set_sensitive(entry, FALSE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(refbutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_GEN_FUNC_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_max_ref", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_check_button_new_with_label(DIALOG_GEN_FUNC_REF_TOTAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_ref_total", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_set_sensitive(button, FALSE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 13, 5, 6);
   gtk_widget_show(hbox);

   // Create Cancel button and hook it to callback.

   button = gtk_button_new_with_label(DIALOG_CANCEL_BUTTON);
   gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   gtk_widget_set_usize(button, 60, -2);
   gtk_widget_show(button);

   CreateCancelButtonCallback(button);

   // Create Apply button and hook it to callback.

   button = gtk_button_new_with_label(DIALOG_APPLY_BUTTON);
   gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 10);
   gtk_widget_set_usize (button, 60, -2);
   gtk_widget_show(button);

   CreateApplyButtonCallback(button);

   // Create OK button and hook it to callback.

   button = gtk_button_new_with_label(DIALOG_OK_BUTTON);
   gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   gtk_widget_set_usize (button, 60, -2);
   gtk_widget_show(button);

   CreateOkButtonCallback(button);
}

/**
 * Destructor.
 */
GeneralFunctionDialog::~GeneralFunctionDialog()
{
}

/**
 * Handler for the Apply logic for this dialog. Apply the specified equations
 * to the selected mesh entities.
 *
 * @return true if any meshes are selected, false otherwise.
 */
bool
GeneralFunctionDialog::Apply()
{
   // Before doing anything, check to see if there are some meshes selected.
   _nullVisitor->ResetVisitedCount();
   GlobalSelectionSystem().foreachSelected(*_nullVisitor);
   if (_nullVisitor->GetVisitedCount() == 0)
   {
      // Nope. Warn and bail out.
      GenericPluginUI::WarningReportDialog(DIALOG_WARNING_TITLE,
                                           DIALOG_NOMESHES_MSG);
      return false;
   }

   // See if we're going to be affecting the S and/or T texture axis.
   bool sApply = NamedToggleWidgetActive("s_apply");
   bool tApply = NamedToggleWidgetActive("t_apply");

   if (!sApply && !tApply)
   {
      // Not affecting either, so bail out.
      return true;
   }

   // OK read the remaining info from the widgets.

   MeshEntity::GeneralFunctionFactors s, t;
   MeshEntity::GeneralFunctionFactors *sFactors = NULL;
   MeshEntity::GeneralFunctionFactors *tFactors = NULL;
   if (sApply)
   {
      // S axis is affected, so read the S factors.
      s.oldValue = (float)atof(NamedEntryWidgetText("s_oldval"));
      s.rowDistance = (float)atof(NamedEntryWidgetText("s_rowdist"));
      s.colDistance = (float)atof(NamedEntryWidgetText("s_coldist"));
      s.rowNumber = (float)atof(NamedEntryWidgetText("s_rownum"));
      s.colNumber = (float)atof(NamedEntryWidgetText("s_colnum"));
      s.constant = (float)atof(NamedEntryWidgetText("s_constant"));
      sFactors = &s;
   }
   if (tApply)
   {
      // T axis is affected, so read the T factors.
      t.oldValue = (float)atof(NamedEntryWidgetText("t_oldval"));
      t.rowDistance = (float)atof(NamedEntryWidgetText("t_rowdist"));
      t.colDistance = (float)atof(NamedEntryWidgetText("t_coldist"));
      t.rowNumber = (float)atof(NamedEntryWidgetText("t_rownum"));
      t.colNumber = (float)atof(NamedEntryWidgetText("t_colnum"));
      t.constant = (float)atof(NamedEntryWidgetText("t_constant"));
      tFactors = &t;
   }
   MeshEntity::SliceDesignation alignRow, alignCol;
   alignRow.maxSlice = NamedToggleWidgetActive("row_max_align");
   alignRow.index = atoi(NamedEntryWidgetText("row_num_align"));
   alignCol.maxSlice = NamedToggleWidgetActive("col_max_align");
   alignCol.index = atoi(NamedEntryWidgetText("col_num_align"));
   MeshEntity::RefSliceDescriptor row, col;
   MeshEntity::RefSliceDescriptor *refRow = NULL;
   MeshEntity::RefSliceDescriptor *refCol = NULL;
   if (NamedToggleWidgetActive("row_ref"))
   {
      // Reference row is specified, so get that info.
      row.designation.maxSlice = NamedToggleWidgetActive("row_max_ref");
      row.designation.index = atoi(NamedEntryWidgetText("row_num_ref"));
      row.totalLengthOnly = NamedToggleWidgetActive("row_ref_total");
      refRow = &row;
   }
   if (NamedToggleWidgetActive("col_ref"))
   {
      // Reference column is specified, so get that info.
      col.designation.maxSlice = NamedToggleWidgetActive("col_max_ref");
      col.designation.index = atoi(NamedEntryWidgetText("col_num_ref"));
      col.totalLengthOnly = NamedToggleWidgetActive("col_ref_total");
      refCol = &col;
   }
   bool surfaceValues = NamedToggleWidgetActive("surface");

   // Let Radiant know the name of the operation responsible for the changes
   // that are about to happen.
   UndoableCommand undo(_triggerCommand.c_str());

   // Apply the specified equation to every selected mesh.
   SmartPointer<GeneralFunctionVisitor> funcVisitor(
      new GeneralFunctionVisitor(sFactors, tFactors,
                                 &alignRow, &alignCol, refRow, refCol,
                                 surfaceValues));
   GlobalSelectionSystem().foreachSelected(*funcVisitor);

   // Done!
   return true;
}
