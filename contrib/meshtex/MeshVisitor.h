/**
 * @file MeshVisitor.h
 * Declares the MeshVisitor class.
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

#if !defined(INCLUDED_MESHVISITOR_H)
#define INCLUDED_MESHVISITOR_H

#include "RefCounted.h"
#include "MeshEntity.h"

#include "iselection.h"

/**
 * Visitor that will invoke Execute with a MeshEntity argument if the visited
 * node is a valid mesh. Subclasses should override Execute to perform
 * operations on the MeshEntity.
 *
 * @ingroup meshtex-util
 */
class MeshVisitor : public SelectionSystem::Visitor, public RefCounted
{
public: // public methods

   MeshVisitor();
   virtual ~MeshVisitor();
   void ResetVisitedCount();
   unsigned GetVisitedCount();
   void visit(scene::Instance& instance) const;

protected: // protected methods

   virtual bool Execute(MeshEntity& meshEntity) const;

private: // private static member vars

   /// @name Callbacks to use when constructing the MeshEntity
   //@{
   static const MeshEntity::MessageCallback _infoReportCallback;
   static const MeshEntity::MessageCallback _warningReportCallback;
   static const MeshEntity::MessageCallback _errorReportCallback;
   //@}

private: // private member vars

   /**
    * Track the number of Execute invocations.
    */
   mutable unsigned _visitedCount;
};

#endif // #if !defined(INCLUDED_MESHVISITOR_H)