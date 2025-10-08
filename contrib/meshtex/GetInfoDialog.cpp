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
   // Configure the dialog window.
   _dialog->setWindowTitle(DIALOG_GET_INFO_TITLE);

   // Create the contained widgets.
   {
      auto *form = new QFormLayout( _dialog );
      form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
      {
         // Widgets for specifying the reference row if any.
         auto *check = new QCheckBox( DIALOG_GET_INFO_S_ROW_HEADER );
         check->setChecked( true );
         auto *spin = s_ref_row = new SpinBox( 0, 30 );
         form->addRow( check, spin );
         UIInstance().RegisterWidgetDependence( check, spin );
      }
      {
         // Widgets for specifying the reference column if any.
         auto *check = new QCheckBox( DIALOG_GET_INFO_T_COL_HEADER );
         check->setChecked( true );
         auto *spin = t_ref_col = new SpinBox( 0, 30 );
         form->addRow( check, spin );
         UIInstance().RegisterWidgetDependence( check, spin );
      }
      {
         // Checkbox to enable the callbacks to Set S/T Scale.
         auto *check = check_transfer = new QCheckBox( DIALOG_GET_INFO_XFER_OPT_LABEL );
         form->addRow( check );
      }
      {
         auto *buttons = new QDialogButtonBox;
         form->addWidget( buttons );
         CreateOkButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Ok ) );
         CreateApplyButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Apply ) );
         CreateCancelButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ) );
      }
   }
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
   const bool transfer = check_transfer->isChecked();
   if (transfer && _nullVisitor->GetVisitedCount() != 1)
   {
      // Multiple selected. Warn and bail out.
      GenericPluginUI::ErrorReportDialog(DIALOG_ERROR_TITLE,
                                         DIALOG_MULTIMESHES_ERROR);
      return false;
   }

   // OK read the remaining info from the widgets.

   int row, col;
   int *refRow = nullptr;
   int *refCol = nullptr;
   MeshEntity::TexInfoCallback *rowTexInfoCallback = nullptr;
   MeshEntity::TexInfoCallback *colTexInfoCallback = nullptr;
   if ( s_ref_row->isEnabled() )
   {
      // Reference row is specified, so get that info.
      row = s_ref_row->value();
      refRow = &row;
      if (transfer)
      {
         // If transferring to Set S/T Scale, get that callback.
         rowTexInfoCallback = &_rowTexInfoCallback;
      }
   }
   if ( t_ref_col->isEnabled() )
   {
      // Reference column is specified, so get that info.
      col = t_ref_col->value();
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
