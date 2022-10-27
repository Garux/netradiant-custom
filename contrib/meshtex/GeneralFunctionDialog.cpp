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
   // Configure the dialog window.
   _dialog->setWindowTitle(DIALOG_GEN_FUNC_TITLE);

   // Create the contained widgets.
   {
      auto grid = new QGridLayout( _dialog ); // 6 x 13
      grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
      grid->setColumnStretch( 0, 1 );
      grid->setColumnStretch( 3, 1 );
      {
         // Mutually exclusive "Surface values" and "Control values" radio buttons.
         {
            auto radio = surface = new QRadioButton( DIALOG_GEN_FUNC_SURFACE_VALUES );
            grid->addWidget( radio, 0, 1 );
            radio->setChecked( true );
         }
         {
            auto radio = new QRadioButton( DIALOG_GEN_FUNC_CONTROL_VALUES );
            grid->addWidget( radio, 0, 2 );
         }
         {
            auto line = new QFrame;
				line->setFrameShape( QFrame::Shape::HLine );
				line->setFrameShadow( QFrame::Shadow::Raised );
				grid->addWidget( line, 1, 0, 1, 4 );
         }
         // Checkbox for the "S" row of factors. All the other widgets on this row
         // will have a dependence registered on this checkbox; i.e. they will only
         // be active when it is checked.
         {
            auto hbox = new QHBoxLayout;
            grid->addLayout( hbox, 2, 0, 1, 4 );

            auto check = s_apply = new QCheckBox;
            check->setChecked( true );
            hbox->addWidget( check );

            auto container = new QWidget;
            hbox->addWidget( container );
            UIInstance().RegisterWidgetDependence( check, container );

            auto container_hbox = new QHBoxLayout( container );
            container_hbox->setContentsMargins( 0, 0, 0, 0 );
            {
               const struct{ const char* name; QDoubleSpinBox *&spin; } data[] = {
                  { DIALOG_GEN_FUNC_S_FUNC_LABEL, s_oldval },
                  { DIALOG_GEN_FUNC_OLD_S_LABEL, s_rowdist },
                  { DIALOG_GEN_FUNC_ROW_DIST_LABEL, s_coldist },
                  { DIALOG_GEN_FUNC_COL_DIST_LABEL, s_rownum },
                  { DIALOG_GEN_FUNC_ROW_NUM_LABEL, s_colnum },
                  { DIALOG_GEN_FUNC_COL_NUM_LABEL, s_constant },
               };
               for( auto [ name, spin ] : data )
               {
                  container_hbox->addWidget( new QLabel( name ) );
                  container_hbox->addWidget( spin = new DoubleSpinBox( -999, 999, 0, 3, .01 ) );
               }
               s_oldval->setValue( 1 );
            }
         }
         // Checkbox for the "T" row of factors. All the other widgets on this row
         // will have a dependence registered on this checkbox; i.e. they will only
         // be active when it is checked.
         {
            auto hbox = new QHBoxLayout;
            grid->addLayout( hbox, 3, 0, 1, 4 );

            auto check = t_apply = new QCheckBox;
            check->setChecked( true );
            hbox->addWidget( check );

            auto container = new QWidget;
            hbox->addWidget( container );
            UIInstance().RegisterWidgetDependence( check, container );

            auto container_hbox = new QHBoxLayout( container );
            container_hbox->setContentsMargins( 0, 0, 0, 0 );
            {
               const struct{ const char* name; QDoubleSpinBox *&spin; } data[] = {
                  { DIALOG_GEN_FUNC_T_FUNC_LABEL, t_oldval },
                  { DIALOG_GEN_FUNC_OLD_S_LABEL, t_rowdist },
                  { DIALOG_GEN_FUNC_ROW_DIST_LABEL, t_coldist },
                  { DIALOG_GEN_FUNC_COL_DIST_LABEL, t_rownum },
                  { DIALOG_GEN_FUNC_ROW_NUM_LABEL, t_colnum },
                  { DIALOG_GEN_FUNC_COL_NUM_LABEL, t_constant },
               };
               for( auto [ name, spin ] : data )
               {
                  container_hbox->addWidget( new QLabel( name ) );
                  container_hbox->addWidget( spin = new DoubleSpinBox( -999, 999, 0, 3, .01 ) );
               }
               t_oldval->setValue( 1 );
            }
         }
         {
            // Widgets for specifying the alignment column.
            auto group = new QGroupBox( DIALOG_GEN_FUNC_COL_ALIGN_FRAME_LABEL );
            grid->addWidget( group, 4, 1 );

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
               auto radio = new QRadioButton( DIALOG_GEN_FUNC_MAX_OPT_LABEL );
               grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
            }
         }
         {
            // Widgets for specifying the reference row & usage.
            auto group = row_ref = new QGroupBox( DIALOG_GEN_FUNC_REF_ROW_FRAME_LABEL );
            group->setCheckable( true );
            group->setChecked( true );
            grid->addWidget( group, 5, 1 );

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
               auto radio = new QRadioButton( DIALOG_GEN_FUNC_MAX_OPT_LABEL );
               grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
            }
            {
               auto check = row_ref_total = new QCheckBox( DIALOG_GEN_FUNC_REF_TOTAL_OPT_LABEL );
               grid->addWidget( check, 1, 0, 1, 3 );
               check->setChecked( true );
            }
         }
         {
            // Widgets for specifying the alignment row.
            auto group = new QGroupBox( DIALOG_GEN_FUNC_ROW_ALIGN_FRAME_LABEL );
            grid->addWidget( group, 4, 2 );

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
               auto radio = new QRadioButton( DIALOG_GEN_FUNC_MAX_OPT_LABEL );
               grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
            }
         }
         {
            // Widgets for specifying the reference column & usage.
            auto group = col_ref = new QGroupBox( DIALOG_GEN_FUNC_REF_COL_FRAME_LABEL );
            group->setCheckable( true );
            group->setChecked( true );
            grid->addWidget( group, 5, 2 );

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
               auto radio = new QRadioButton( DIALOG_GEN_FUNC_MAX_OPT_LABEL );
               grid->addWidget( radio, 0, 2, Qt::AlignmentFlag::AlignRight );
            }
            {
               auto check = col_ref_total = new QCheckBox( DIALOG_GEN_FUNC_REF_TOTAL_OPT_LABEL );
               grid->addWidget( check, 1, 0, 1, 3 );
               check->setChecked( true );
            }
         }
         {
            auto buttons = new QDialogButtonBox;
            grid->addWidget( buttons, 6, 0, 1, 4 );
            CreateOkButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Ok ) );
            CreateApplyButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Apply ) );
            CreateCancelButtonCallback( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ) );
         }
      }
   }
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
   const bool sApply = s_apply->isChecked();
   const bool tApply = t_apply->isChecked();

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
      s.oldValue = s_oldval->value();
      s.rowDistance = s_rowdist->value();
      s.colDistance = s_coldist->value();
      s.rowNumber = s_rownum->value();
      s.colNumber = s_colnum->value();
      s.constant = s_constant->value();
      sFactors = &s;
   }
   if (tApply)
   {
      // T axis is affected, so read the T factors.
      t.oldValue = t_oldval->value();
      t.rowDistance = t_rowdist->value();
      t.colDistance = t_coldist->value();
      t.rowNumber = t_rownum->value();
      t.colNumber = t_colnum->value();
      t.constant = t_constant->value();
      tFactors = &t;
   }
   MeshEntity::SliceDesignation alignRow, alignCol;
   alignRow.maxSlice = !row_num_align->isEnabled();
   alignRow.index = row_num_align->value();
   alignCol.maxSlice = !col_num_align->isEnabled();
   alignCol.index = col_num_align->value();
   MeshEntity::RefSliceDescriptor row, col;
   MeshEntity::RefSliceDescriptor *refRow = NULL;
   MeshEntity::RefSliceDescriptor *refCol = NULL;
   if ( row_ref->isChecked() )
   {
      // Reference row is specified, so get that info.
      row.designation.maxSlice = !row_num_ref->isEnabled();
      row.designation.index = row_num_ref->value();
      row.totalLengthOnly = row_ref_total->isChecked();
      refRow = &row;
   }
   if ( col_ref->isChecked() )
   {
      // Reference column is specified, so get that info.
      col.designation.maxSlice = !col_num_ref->isEnabled();
      col.designation.index = col_num_ref->value();
      col.totalLengthOnly = col_ref_total->isChecked();
      refCol = &col;
   }
   const bool surfaceValues = surface->isChecked();

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
