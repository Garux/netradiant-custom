/**
 * @file MeshVisitor.cpp
 * Implements the MeshVisitor class.
 * @ingroup meshtex-util
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

#include "MeshVisitor.h"
#include "GenericPluginUI.h"
#include "PluginUIMessages.h"


/**
 * Use GenericPluginUI::InfoReportDialog as MeshEntity info callback.
 */
const MeshEntity::MessageCallback MeshVisitor::_infoReportCallback(
   PointerCaller1<const char,
                  const char *,
                  &GenericPluginUI::InfoReportDialog>(DIALOG_MESH_INFO_TITLE));

/**
 * Use GenericPluginUI::WarningReportDialog as MeshEntity warning callback.
 */
const MeshEntity::MessageCallback MeshVisitor::_warningReportCallback(
   PointerCaller1<const char,
                  const char *,
                  &GenericPluginUI::WarningReportDialog>(DIALOG_WARNING_TITLE));

/**
 * Use GenericPluginUI::ErrorReportDialog as MeshEntity error callback.
 */
const MeshEntity::MessageCallback MeshVisitor::_errorReportCallback(
   PointerCaller1<const char,
                  const char *,
                  &GenericPluginUI::ErrorReportDialog>(DIALOG_ERROR_TITLE));


/**
 * Default constructor.
 */
MeshVisitor::MeshVisitor() :
   _visitedCount(0)
{
}

/**
 * Virtual destructor.
 */
MeshVisitor::~MeshVisitor()
{
}

/**
 * Reset the visited count to zero.
 */
void
MeshVisitor::ResetVisitedCount()
{
   _visitedCount = 0;
}

/**
 * Get the visited count. This is the number of times Execute has been
 * invoked since visitor construction or the most recent reset of the count.
 *
 * @return The visited count.
 */
unsigned
MeshVisitor::GetVisitedCount()
{
   return _visitedCount;
}

/**
 * Visit a specified scene graph node.
 *
 * @param [in,out] instance The scene graph node.
 */
void
MeshVisitor::visit(scene::Instance& instance) const
{
   if (Node_isPatch(instance.path().top()))
   {
      // If it's a patch mesh, try creating a MeshEntity.
      MeshEntity meshEntity(instance.path().top(),
                            _infoReportCallback,
                            _warningReportCallback,
                            _errorReportCallback);
      if (meshEntity.IsValid())
      {
         // If we have a valid MeshEntity, invoke Execute.
         if (Execute(meshEntity))
         {
            // Count the number of affected meshes.
            _visitedCount++;
         }
      }
   }
}

/**
 * Execute function performed for visited meshes. This implementation does
 * nothing; subclasses should override it.
 *
 * @param meshEntity The MeshEntity.
 *
 * @return true if the mesh was successfully visited; always the case in this
 *         skeleton implementation.
 */
bool
MeshVisitor::Execute(MeshEntity& meshEntity) const
{
   return true;
}
