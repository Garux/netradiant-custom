/**
 * @file GetInfoDialog.cpp
 * Implements the GetInfoDialog class.
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
#include "GetInfoDialog.h"
#include "PluginUIMessages.h"


/**
 * Constructor. See MeshEntity::GetInfo for details of how these arguments
 * are interpreted.
 *
 * @param refRow             Pointer to reference row number; NULL if none.
 * @param refCol             Pointer to reference column number; NULL if none.
 * @param rowTexInfoCallback Pointer to callback for reference row info; NULL
 *                           if none.
 * @param colTexInfoCallback Pointer to callback for reference column info;
 *                           NULL if none.
 */
GetInfoDialog::GetInfoVisitor::GetInfoVisitor(
   const int *refRow,
   const int *refCol,
   const MeshEntity::TexInfoCallback *rowTexInfoCallback,
   const MeshEntity::TexInfoCallback *colTexInfoCallback) :
   _refRow(refRow),
   _refCol(refCol),
   _rowTexInfoCallback(rowTexInfoCallback),
   _colTexInfoCallback(colTexInfoCallback)
{
}

/**
 * Visitor action; invoke MeshEntity::GetInfo on a mesh.
 *
 * @param [in,out] meshEntity The mesh.
 *
 * @return true.
 */
bool
GetInfoDialog::GetInfoVisitor::Execute(MeshEntity& meshEntity) const
{
   meshEntity.GetInfo(_refRow, _refCol, _rowTexInfoCallback, _colTexInfoCallback);
   return true;
}

/**
 * Constructor. Connect the row and column texture info callbacks to the
 * appropriate methods on the Set S/T Scale dialog object. Configure the
 * dialog window and create all the contained widgets. Connect widgets to
 * callbacks as necessary.
 *
 * @param key            The unique key identifying this dialog.
 * @param setScaleDialog Reference-counted handle on the Set S/T Scale dialog.
 */
GetInfoDialog::GetInfoDialog(const std::string& key,
                             SmartPointer<SetScaleDialog>& setScaleDialog) :
   GenericDialog(key),
   _setScaleDialog(setScaleDialog),
   _rowTexInfoCallback(
      MeshEntity::TexInfoCallbackMethod<SetScaleDialog,
                                        &SetScaleDialog::PopulateSWidgets>(*setScaleDialog)),
   _colTexInfoCallback(
      MeshEntity::TexInfoCallbackMethod<SetScaleDialog,
                                        &SetScaleDialog::PopulateTWidgets>(*setScaleDialog)),
   _nullVisitor(new MeshVisitor())
{
   // Enable the usual handling of the close event.
   CreateWindowCloseCallback();

   // Configure the dialog window.
   gtk_window_set_resizable(GTK_WINDOW(_dialog), FALSE);
   gtk_window_set_title(GTK_WINDOW(_dialog), DIALOG_GET_INFO_TITLE);
   gtk_container_set_border_width(GTK_CONTAINER(_dialog), 10);

   // Create the contained widgets.

   GtkWidget *table;
   GtkWidget *entry;
   GtkWidget *button;
   GtkWidget *label;
   GtkWidget *hbox;

   table = gtk_table_new(4, 3, FALSE);
   gtk_table_set_row_spacing(GTK_TABLE(table), 1, 10);
   gtk_table_set_row_spacing(GTK_TABLE(table), 2, 15);
   gtk_container_add(GTK_CONTAINER(_dialog), table);
   gtk_widget_show(table);

   // Widgets for specifying the reference row if any.

   button = gtk_check_button_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_apply", button);
   gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, 0, 1);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   label = gtk_label_new(DIALOG_GET_INFO_S_ROW_HEADER);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 0, 1);
   gtk_widget_show(label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "s_ref_row", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 0, 1);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(button, label);
   UIInstance().RegisterWidgetDependence(button, entry);

   // Widgets for specifying the reference column if any.

   button = gtk_check_button_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_apply", button);
   gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, 1, 2);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   gtk_widget_show(button);

   label = gtk_label_new(DIALOG_GET_INFO_T_COL_HEADER);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
   gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 1, 2);
   gtk_widget_show(label);

   entry = gtk_entry_new();
   gtk_object_set_data(GTK_OBJECT(_dialog), "t_ref_col", entry);
   gtk_entry_set_text(GTK_ENTRY(entry), "0");
   gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 1, 2);
   gtk_widget_set_usize(entry, 50, -2);
   gtk_widget_show(entry);

   UIInstance().RegisterWidgetDependence(button, label);
   UIInstance().RegisterWidgetDependence(button, entry);

   // Checkbox to enable the callbacks to Set S/T Scale.

   button = gtk_check_button_new_with_label(DIALOG_GET_INFO_XFER_OPT_LABEL);
   gtk_object_set_data(GTK_OBJECT(_dialog), "transfer", button);
   gtk_table_attach(GTK_TABLE(table), button, 0, 3, 2, 3, GTK_EXPAND, GTK_EXPAND, 0, 0);
   gtk_widget_show(button);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 3, 3, 4);
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
GetInfoDialog::~GetInfoDialog()
{
}

/**
 * Handler for the Apply logic for this dialog. Interrogate the selected mesh
 * entities.
 *
 * @return false if no meshes are selected, false if multiple meshes are
 *         selected along with the transfer option, true otherwise.
 */
bool
GetInfoDialog::Apply()
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

   // If the option to transfer info to Set S/T Scale is active, then only one
   // mesh may be selected.
   bool transfer = NamedToggleWidgetActive("transfer");
   if (transfer && _nullVisitor->GetVisitedCount() != 1)
   {
      // Multiple selected. Warn and bail out.
      GenericPluginUI::ErrorReportDialog(DIALOG_ERROR_TITLE,
                                         DIALOG_MULTIMESHES_ERROR);
      return false;
   }

   // OK read the remaining info from the widgets.

   bool sApply = NamedToggleWidgetActive("s_apply");
   bool tApply = NamedToggleWidgetActive("t_apply");

   int row, col;
   int *refRow = NULL;
   int *refCol = NULL;
   MeshEntity::TexInfoCallback *rowTexInfoCallback = NULL;
   MeshEntity::TexInfoCallback *colTexInfoCallback = NULL;
   if (sApply)
   {
      // Reference row is specified, so get that info.
      row = atoi(NamedEntryWidgetText("s_ref_row"));
      refRow = &row;
      if (transfer)
      {
         // If transferring to Set S/T Scale, get that callback.
         rowTexInfoCallback = &_rowTexInfoCallback;
      }
   }
   if (tApply)
   {
      // Reference column is specified, so get that info.
      col = atoi(NamedEntryWidgetText("t_ref_col"));
      refCol = &col;
      if (transfer)
      {
         // If transferring to Set S/T Scale, get that callback.
         colTexInfoCallback = &_colTexInfoCallback;
      }
   }

   // We don't need to instantiate an UndoableCommand since we won't be making
   // any changes.

   // Interrogate every selected mesh.
   SmartPointer<GetInfoVisitor> infoVisitor(
      new GetInfoVisitor(refRow, refCol, rowTexInfoCallback, colTexInfoCallback));
   GlobalSelectionSystem().foreachSelected(*infoVisitor);

   // If we populated something in the Set S/T Scale dialog, give that dialog a
   // courtesy raise.
   if (transfer)
   {
      _setScaleDialog->Raise();
   }

   // Done!
   return true;
}
