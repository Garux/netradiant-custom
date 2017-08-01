/**
 * @file SetScaleDialog.h
 * Declares the SetScaleDialog class.
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

#if !defined(INCLUDED_SETSCALEDIALOG_H)
#define INCLUDED_SETSCALEDIALOG_H

#include "GenericDialog.h"
#include "MeshVisitor.h"

/**
 * Subclass of GenericDialog that implements the window summoned by selecting
 * the Set S/T Scale menu entry. This window is used to control the scaling
 * of the S and/or T texture axes.
 * 
 * @image html setscale.png
 *
 * @ingroup meshtex-ui
 */
class SetScaleDialog : public GenericDialog
{
private: // private types

   /**
    * Visitor for applying the chosen scaling to a mesh.
    */
   class SetScaleVisitor : public MeshVisitor
   {
   public:
      /**
       * Encapsulation of all the arguments that can affect either the S or T
       * texture axis. See MeshEntity::SetScale for details of how these arguments
       * are interpreted.
       */
      typedef struct {
         /**
          * Pointer to alignment slice description; if NULL, slice 0 is assumed.
          */
         const MeshEntity::SliceDesignation *alignSlice;
         /**
          * Pointer to reference slice description, including how to use the reference;
          * NULL if no reference.
          */
         const MeshEntity::RefSliceDescriptor *refSlice;
         /**
          * true if naturalScaleOrTiles is a factor of the Radiant natural scale;
          * false if naturalScaleOrTiles is a number of tiles.
          */
         bool naturalScale;
         /**
          * Scaling determinant, interpreted according to the naturalScale parameter.
          */
         float scaleOrTiles;
      } SliceArgs;
   public:
      SetScaleVisitor(const SliceArgs *rowArgs,
                      const SliceArgs *colArgs);
   private:
      bool Execute(MeshEntity& meshEntity) const;
   private:
      const SliceArgs *_rowArgs;
      const SliceArgs *_colArgs;
   };

public: // public methods

   SetScaleDialog(const std::string& key);
   ~SetScaleDialog();
   bool Apply();
   void PopulateSWidgets(float scale,
                         float tiles);
   void PopulateTWidgets(float scale,
                         float tiles);

private: // private methods

   void PopulateEntry(const char *widgetName,
                      float value);

private: // private member vars

   /**
    * Action-less mesh visitor used purely to count the number of selected mesh
    * entities.
    */
   SmartPointer<MeshVisitor> _nullVisitor;
};

#endif // #if !defined(INCLUDED_SETSCALEDIALOG_H)