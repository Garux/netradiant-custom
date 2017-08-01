/**
 * @file RefCounted.h
 * Declares the RefCounted class.
 * @ingroup generic-util
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

#if !defined(INCLUDED_REFCOUNTED_H)
#define INCLUDED_REFCOUNTED_H

/**
 * A mixin for maintaining a reference count associated with an object, and
 * destroying that object when the count falls to zero. Since this class
 * implements the IncRef and DecRef interfaces used by the Radiant
 * SmartPointer and SmartReference templates, those templates can
 * automatically handle the reference counting for classes that inherit from
 * RefCounted.
 *
 * @ingroup generic-util
 */
class RefCounted
{
public: // public methods

   /// @name Lifecycle
   //@{
   RefCounted();
   virtual ~RefCounted();
   //@}
   /// @name Reference counting
   //@{
   void IncRef();
   void DecRef();
   //@}

private: // private member vars

   /**
    * Reference count.
    */
   int _refCount;
};

#endif // #if !defined(INCLUDED_REFCOUNTED_H)