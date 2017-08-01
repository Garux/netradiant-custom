/**
 * @file GetInfoDialog.h
 * Declares the GetInfoDialog class.
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

#if !defined(INCLUDED_GETINFODIALOG_H)
#define INCLUDED_GETINFODIALOG_H

#include "GenericDialog.h"
#include "SetScaleDialog.h"
#include "MeshVisitor.h"

#include "generic/referencecounted.h"

/**
 * Subclass of GenericDialog that implements the window summoned by selecting
 * the Get Info menu entry. This window allows the user to query information
 * about selected meshes and optionally transfer some of that information to
 * the Set S/T Scale dialog.
 * 
 * @image html getinfo.png
 *
 * @ingroup meshtex-ui
 */
class GetInfoDialog : public GenericDialog
{
private: // private types

   /**
    * Visitor for interrogating a mesh.
    */
   class GetInfoVisitor : public MeshVisitor
   {
   public:
      GetInfoVisitor(const int *refRow,
                     const int *refCol,
                     const MeshEntity::TexInfoCallback *rowTexInfoCallback,
                     const MeshEntity::TexInfoCallback *colTexInfoCallback);
   private:
      bool Execute(MeshEntity& meshEntity) const;
   private:
      const int *_refRow;
      const int *_refCol;
      const MeshEntity::TexInfoCallback *_rowTexInfoCallback;
      const MeshEntity::TexInfoCallback *_colTexInfoCallback;
   };

public: // public methods

   GetInfoDialog(const std::string& key,
                 SmartPointer<SetScaleDialog>& setScaleDialog);
   ~GetInfoDialog();
   bool Apply();

private: // private member vars

   /**
    * Handle on the Set S/T Scale dialog.
    */
   SmartPointer<SetScaleDialog> _setScaleDialog;

   /**
    * Callback to process row texture scale information from a query.
    */
   MeshEntity::TexInfoCallback _rowTexInfoCallback;

   /**
    * Callback to process column texture scale information from a query.
    */
   MeshEntity::TexInfoCallback _colTexInfoCallback;

   /**
    * Action-less mesh visitor used purely to count the number of selected mesh
    * entities.
    */
   SmartPointer<MeshVisitor> _nullVisitor;
};

#endif // #if !defined(INCLUDED_GETINFODIALOG_H)