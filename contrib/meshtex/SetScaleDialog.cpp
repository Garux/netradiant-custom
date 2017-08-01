/**
 * @file SetScaleDialog.cpp
 * Implements the SetScaleDialog class.
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
#include "SetScaleDialog.h"
#include "PluginUIMessages.h"

#include "iundo.h"


/**
 * Size of buffer for the text conversion of float values written to various
 * widgets.
 */
#define ENTRY_BUFFER_SIZE 128


/**
 * Constructor.
 *
 * @param rowArgs The row (S axis) arguments; NULL if none.
 * @param colArgs The column (T axis) arguments; NULL if none.
 */
SetScaleDialog::SetScaleVisitor::SetScaleVisitor(
   const SliceArgs *rowArgs,
   const SliceArgs *colArgs) :
   _rowArgs(rowArgs),
   _colArgs(colArgs)
{
}

/**
 * Visitor action; invoke MeshEntity::SetScale on a mesh.
 *
 * @param [in,out] meshEntity The mesh entity.
 *
 * @return true.
 */
bool
SetScaleDialog::SetScaleVisitor::Execute(MeshEntity& meshEntity) const
{
   if (_rowArgs != NULL)
   {
      meshEntity.SetScale(MeshEntity::ROW_SLICE_TYPE,
                          _rowArgs->alignSlice, _rowArgs->refSlice,
                          _rowArgs->naturalScale, _rowArgs->scaleOrTiles);
   }
   if (_colArgs != NULL)
   {
      meshEntity.SetScale(MeshEntity::COL_SLICE_TYPE,
                          _colArgs->alignSlice, _colArgs->refSlice,
                          _colArgs->naturalScale, _colArgs->scaleOrTiles);
   }
   return true;
}

/**
 * Constructor. Configure the dialog window and create all the contained
 * widgets. Connect widgets to callbacks as necessary.
 *
 * @param key The unique key identifying this dialog.
 */
SetScaleDialog::SetScaleDialog(const std::string& key) :
   GenericDialog(key),
   _nullVisitor(new MeshVisitor())
{
   // Enable the usual handling of the close event.
   CreateWindowCloseCallback();

   // Configure the dialog window.
   gtk_window_set_resizable(GTK_WINDOW(_dialog), FALSE);
   gtk_window_set_title(GTK_WINDOW(_dialog), DIALOG_SET_SCALE_TITLE);
   gtk_container_set_border_width(GTK_CONTAINER(_dialog), 10);

   // Create the contained widgets.

   GtkWidget *table;
   GtkWidget *entry;
   GtkWidget *applybutton, *refbutton, *button;
   GtkWidget *label;
   GtkWidget *mainvbox, *vbox, *hbox;
   GtkWidget *frame;

   table = gtk_table_new(2, 2, FALSE);
   gtk_table_set_row_spacing(GTK_TABLE(table), 0, 15);
   gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);
   gtk_container_add(GTK_CONTAINER(_dialog), table);
   gtk_widget_show(table);

   // Checkbox for the "S" grouping of widgets. All the widgets in that
   // grouping will have a dependence registered on this checkbox; i.e. they
   // will only be active when it is checked.

   applybutton = gtk_check_button_new_with_label(DIALOG_SET_SCALE_S_ACTIVE_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_apply", applybutton);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(applybutton), TRUE);
   gtk_widget_show(applybutton);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), applybutton);
   gtk_table_attach_defaults(GTK_TABLE(table), frame, 0, 1, 0, 1);
   gtk_widget_show(frame);

   mainvbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), mainvbox);
   gtk_widget_show(mainvbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying S scaling.

   label = gtk_label_new(DIALOG_SET_SCALE_METHOD_FRAME_TITLE);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), label);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new_with_label(NULL, DIALOG_SET_SCALE_TILES_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_tiling", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_tiles", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1");
   gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_set_sensitive(entry, FALSE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_NATURAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_natural", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_scale", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1");
   gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_set_sensitive(entry, TRUE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the alignment column.

   label = gtk_label_new(DIALOG_SET_SCALE_S_ALIGN_FRAME_TITLE);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), label);
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
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_num_align", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_max_align", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the reference row & usage.

   refbutton = gtk_check_button_new_with_label(DIALOG_SET_SCALE_S_REF_ROW_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_ref", refbutton);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refbutton), TRUE);
   gtk_widget_show(refbutton);

   UIInstance().RegisterWidgetDependence(applybutton, refbutton);

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
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_num_ref", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(refbutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_max_ref", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_check_button_new_with_label(DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_ref_total", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   // Checkbox for the "T" grouping of widgets. All the widgets in that
   // grouping will have a dependence registered on this checkbox; i.e. they
   // will only be active when it is checked.

   applybutton = gtk_check_button_new_with_label(DIALOG_SET_SCALE_T_ACTIVE_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_apply", applybutton);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(applybutton), TRUE);
   gtk_widget_show(applybutton);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), applybutton);
   gtk_table_attach_defaults(GTK_TABLE(table), frame, 1, 2, 0, 1);
   gtk_widget_show(frame);

   mainvbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), mainvbox);
   gtk_widget_show(mainvbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying T scaling.

   label = gtk_label_new(DIALOG_SET_SCALE_METHOD_FRAME_TITLE);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), label);
   gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(frame), vbox);
   gtk_widget_show(vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new_with_label(NULL, DIALOG_SET_SCALE_TILES_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_tiling", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_tiles", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1");
   gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_set_sensitive(entry, FALSE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_NATURAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_natural", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_scale", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "1");
   gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_set_sensitive(entry, TRUE);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the alignment row.

   label = gtk_label_new(DIALOG_SET_SCALE_T_ALIGN_FRAME_TITLE);
   gtk_widget_show(label);

   UIInstance().RegisterWidgetDependence(applybutton, label);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_label_widget(GTK_FRAME(frame), label);
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
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_num_align", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "row_max_align", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   // Widgets for specifying the reference column & usage.

   refbutton = gtk_check_button_new_with_label(DIALOG_SET_SCALE_T_REF_COL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_ref", refbutton);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refbutton), TRUE);
   gtk_widget_show(refbutton);

   UIInstance().RegisterWidgetDependence(applybutton, refbutton);

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
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_num_ref", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
   gtk_widget_set_usize(entry, 25, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(applybutton, entry);
   UIInstance().RegisterWidgetDependence(refbutton, entry);
   UIInstance().RegisterWidgetDependence(button, entry);

   button = gtk_radio_button_new_with_label(
      gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
                             DIALOG_SET_SCALE_MAX_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_max_ref", button);
   gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, FALSE, 5);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
   gtk_widget_show(hbox);

   button = gtk_check_button_new_with_label(DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "col_ref_total", button);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   UIInstance().RegisterWidgetDependence(applybutton, button);
   UIInstance().RegisterWidgetDependence(refbutton, button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 1, 2);
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
SetScaleDialog::~SetScaleDialog()
{
}

/**
 * Handler for the Apply logic for this dialog. Apply the specified scaling to
 * the selected mesh entities.
 *
 * @return true if any meshes are selected, false otherwise.
 */
bool
SetScaleDialog::Apply()
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

   MeshEntity::SliceDesignation alignCol, alignRow;
   MeshEntity::RefSliceDescriptor refRow, refCol;
   SetScaleVisitor::SliceArgs row, col;
   SetScaleVisitor::SliceArgs *rowArgs = NULL;
   SetScaleVisitor::SliceArgs *colArgs = NULL;
   if (sApply)
   {
      // S axis is affected, so read the S info.
      row.naturalScale = NamedToggleWidgetActive("s_natural");
      if (row.naturalScale)
      {
         row.scaleOrTiles = (float)atof(NamedEntryWidgetText("s_scale"));
      }
      else
      {
         row.scaleOrTiles = (float)atof(NamedEntryWidgetText("s_tiles"));
      }
      alignCol.maxSlice = NamedToggleWidgetActive("col_max_align");
      alignCol.index = atoi(NamedEntryWidgetText("col_num_align"));
      row.alignSlice = &alignCol;
      row.refSlice = NULL;
      if (NamedToggleWidgetActive("row_ref"))
      {
         // Reference row is specified, so get that info.
         refRow.designation.maxSlice = NamedToggleWidgetActive("row_max_ref");
         refRow.designation.index = atoi(NamedEntryWidgetText("row_num_ref"));
         refRow.totalLengthOnly = NamedToggleWidgetActive("row_ref_total");
         row.refSlice = &refRow;
      }
      rowArgs = &row;
   }
   if (tApply)
   {
      // T axis is affected, so read the T info.
      col.naturalScale = NamedToggleWidgetActive("t_natural");
      if (col.naturalScale)
      {
         col.scaleOrTiles = (float)atof(NamedEntryWidgetText("t_scale"));
      }
      else
      {
         col.scaleOrTiles = (float)atof(NamedEntryWidgetText("t_tiles"));
      }
      alignRow.maxSlice = NamedToggleWidgetActive("row_max_align");
      alignRow.index = atoi(NamedEntryWidgetText("row_num_align"));
      col.alignSlice = &alignRow;
      col.refSlice = NULL;
      if (NamedToggleWidgetActive("col_ref"))
      {
         // Reference column is specified, so get that info.
         refCol.designation.maxSlice = NamedToggleWidgetActive("col_max_ref");
         refCol.designation.index = atoi(NamedEntryWidgetText("col_num_ref"));
         refCol.totalLengthOnly = NamedToggleWidgetActive("col_ref_total");
         col.refSlice = &refCol;
      }
      colArgs = &col;
   }

   // Let Radiant know the name of the operation responsible for the changes
   // that are about to happen.
   UndoableCommand undo(_triggerCommand.c_str());

   // Apply the specified scaling to every selected mesh.
   SmartPointer<SetScaleVisitor> scaleVisitor(new SetScaleVisitor(rowArgs, colArgs));
   GlobalSelectionSystem().foreachSelected(*scaleVisitor);

   // Done!
   return true;
}

/**
 * Allow an external caller to set some of the S-axis entries.
 *
 * @param scale Texture scaling.
 * @param tiles Texture tiles.
 */
void
SetScaleDialog::PopulateSWidgets(float scale,
                                 float tiles)
{
   // Use the texture info to populate some of our widgets.
   PopulateEntry("s_scale", scale);
   PopulateEntry("s_tiles", tiles);
}

/**
 * Allow an external caller to set some of the T-axis entries.
 *
 * @param scale Texture scaling.
 * @param tiles Texture tiles.
 */
void
SetScaleDialog::PopulateTWidgets(float scale,
                                 float tiles)
{
   // Use the texture info to populate some of our widgets.
   PopulateEntry("t_scale", scale);
   PopulateEntry("t_tiles", tiles);
}

/**
 * Populate a text widget with a floating point number.
 *
 * @param widgetName Name of the widget.
 * @param value      The number to write to the widget.
 */
void
SetScaleDialog::PopulateEntry(const char *widgetName,
                              float value)
{
   static char entryBuffer[ENTRY_BUFFER_SIZE + 1] = { 0 };
   snprintf(entryBuffer, ENTRY_BUFFER_SIZE, "%f", value);
   gtk_entry_set_text(GTK_ENTRY(NamedWidget(widgetName)), entryBuffer);
}
