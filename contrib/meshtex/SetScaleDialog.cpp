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
   // Configure the dialog window.
   _dialog->setWindowTitle(DIALOG_SET_SCALE_TITLE);

   // Create the contained widgets.
   {
      auto dialog_grid = new QGridLayout( _dialog );
      dialog_grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
      {
         // Checkbox for the "S" grouping of widgets. All the widgets in that
         // grouping will have a dependence registered on this checkbox; i.e. they
         // will only be active when it is checked.
         auto group = s_apply = new QGroupBox( DIALOG_SET_SCALE_S_ACTIVE_OPT_LABEL );
         dialog_grid->addWidget( group, 0, 0 );
         group->setCheckable( true );
         group->setChecked( true );
         {
            auto vbox = new QVBoxLayout( group );
            {
               // Widgets for specifying S scaling.
               auto group = new QGroupBox( DIALOG_SET_SCALE_METHOD_FRAME_TITLE );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_NATURAL_OPT_LABEL );
                  grid->addWidget( radio, 0, 0 );
                  auto spin = s_scale = new DoubleSpinBox( -999, 999, 1, 3 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_TILES_OPT_LABEL );
                  grid->addWidget( radio, 1, 0 );
                  auto spin = s_tiles = new DoubleSpinBox( -999, 999, 1, 3 );
                  grid->addWidget( spin, 1, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  spin->setEnabled( false );
               }
            }
            {
               // Widgets for specifying the alignment column.
               auto group = new QGroupBox( DIALOG_SET_SCALE_S_ALIGN_FRAME_TITLE );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               grid->setColumnStretch( 2, 1 );
               {
                  auto radio = new QRadioButton;
                  grid->addWidget( radio, 0, 0 );
                  auto spin = col_num_align = new SpinBox( 0, 30 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_MAX_OPT_LABEL );
                  grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
               }
            }
            {
               // Widgets for specifying the reference row & usage.
               auto group = row_ref = new QGroupBox( DIALOG_SET_SCALE_S_REF_ROW_OPT_LABEL );
               group->setCheckable( true );
               group->setChecked( true );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               grid->setColumnStretch( 2, 1 );
               {
                  auto radio = new QRadioButton;
                  grid->addWidget( radio, 0, 0 );
                  auto spin = row_num_ref = new SpinBox( 0, 30 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_MAX_OPT_LABEL );
                  grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
               }
               {
                  auto check = row_ref_total = new QCheckBox( DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL );
                  grid->addWidget( check, 1, 0, 1, 3 );
                  check->setChecked( true );
               }
            }
         }
      }
      {
         // Checkbox for the "T" grouping of widgets. All the widgets in that
         // grouping will have a dependence registered on this checkbox; i.e. they
         // will only be active when it is checked.
         auto group = t_apply = new QGroupBox( DIALOG_SET_SCALE_T_ACTIVE_OPT_LABEL );
         dialog_grid->addWidget( group, 0, 1 );
         group->setCheckable( true );
         group->setChecked( true );
         {
            auto vbox = new QVBoxLayout( group );
            {
               // Widgets for specifying T scaling.
               auto group = new QGroupBox( DIALOG_SET_SCALE_METHOD_FRAME_TITLE );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_NATURAL_OPT_LABEL );
                  grid->addWidget( radio, 0, 0 );
                  auto spin = t_scale = new DoubleSpinBox( -999, 999, 1, 3 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_TILES_OPT_LABEL );
                  grid->addWidget( radio, 1, 0 );
                  auto spin = t_tiles = new DoubleSpinBox( -999, 999, 1, 3 );
                  grid->addWidget( spin, 1, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  spin->setEnabled( false );
               }
            }
            {
               // Widgets for specifying the alignment row.
               auto group = new QGroupBox( DIALOG_SET_SCALE_T_ALIGN_FRAME_TITLE );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               grid->setColumnStretch( 2, 1 );
               {
                  auto radio = new QRadioButton;
                  grid->addWidget( radio, 0, 0 );
                  auto spin = row_num_align = new SpinBox( 0, 30 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_MAX_OPT_LABEL );
                  grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
               }
            }
            {
               // Widgets for specifying the reference column & usage.
               auto group = col_ref = new QGroupBox( DIALOG_SET_SCALE_T_REF_COL_OPT_LABEL );
               group->setCheckable( true );
               group->setChecked( true );
               vbox->addWidget( group );

               auto grid = new QGridLayout( group );
               grid->setColumnStretch( 2, 1 );
               {
                  auto radio = new QRadioButton;
                  grid->addWidget( radio, 0, 0 );
                  auto spin = col_num_ref = new SpinBox( 0, 30 );
                  grid->addWidget( spin, 0, 1 );
                  UIInstance().RegisterWidgetDependence( radio, spin );
                  radio->setChecked( true );
               }
               {
                  auto radio = new QRadioButton( DIALOG_SET_SCALE_MAX_OPT_LABEL );
                  grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
               }
               {
                  auto check = col_ref_total = new QCheckBox( DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL );
                  grid->addWidget( check, 1, 0, 1, 3 );
                  check->setChecked( true );
               }
            }
         }
      }
      {
         auto buttons = new QDialogButtonBox;
         dialog_grid->addWidget( buttons, 1, 0, 1, 2 );
         CreateOkButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Ok ) );
         CreateApplyButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Apply ) );
         CreateCancelButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ) );
      }
   }
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
   const bool sApply = s_apply->isChecked();
   const bool tApply = t_apply->isChecked();

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
      row.naturalScale = s_scale->isEnabled();
      if (row.naturalScale)
      {
         row.scaleOrTiles = s_scale->value();
      }
      else
      {
         row.scaleOrTiles = s_tiles->value();
      }
      alignCol.maxSlice = !col_num_align->isEnabled();
      alignCol.index = col_num_align->value();
      row.alignSlice = &alignCol;
      row.refSlice = NULL;
      if ( row_ref->isChecked() )
      {
         // Reference row is specified, so get that info.
         refRow.designation.maxSlice = !row_num_ref->isEnabled();
         refRow.designation.index = row_num_ref->value();
         refRow.totalLengthOnly = row_ref_total->isChecked();
         row.refSlice = &refRow;
      }
      rowArgs = &row;
   }
   if (tApply)
   {
      // T axis is affected, so read the T info.
      col.naturalScale = t_scale->isEnabled();
      if (col.naturalScale)
      {
         col.scaleOrTiles = t_scale->value();
      }
      else
      {
         col.scaleOrTiles = t_tiles->value();
      }
      alignRow.maxSlice = !row_num_align->isEnabled();
      alignRow.index = row_num_align->value();
      col.alignSlice = &alignRow;
      col.refSlice = NULL;
      if ( col_ref->isChecked() )
      {
         // Reference column is specified, so get that info.
         refCol.designation.maxSlice = !col_num_ref->isEnabled();
         refCol.designation.index = col_num_ref->value();
         refCol.totalLengthOnly = col_ref_total->isChecked();
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
   s_scale->setValue( scale);
   s_tiles->setValue( tiles);
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
   t_scale->setValue( scale);
   t_tiles->setValue( tiles);
}
