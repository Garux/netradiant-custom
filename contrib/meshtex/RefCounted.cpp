/**
 * @file RefCounted.cpp
 * Implements the RefCounted class.
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

#include "RefCounted.h"


/**
 * Default constructor. Initialize reference count to zero.
 */
RefCounted::RefCounted() :
   _refCount(0)
{
}

/**
 * Virtual destructor.
 */
RefCounted::~RefCounted()
{
}

/**
 * Increment reference count.
 */
void
RefCounted::IncRef()
{
   ++_refCount;
}

/**
 * Decrement reference count, and self-delete if count is <= 0.
 */
void
RefCounted::DecRef()
{
   if (--_refCount <= 0)
   {
      delete this;
   }
}
