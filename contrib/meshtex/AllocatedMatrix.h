/**
 * @file AllocatedMatrix.h
 * Declares and implements the AllocatedMatrix template class.
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

#if !defined(INCLUDED_ALLOCATEDMATRIX_H)
#define INCLUDED_ALLOCATEDMATRIX_H

#include "debugging/debugging.h"
#include "ipatch.h"

/**
 * Matrix subclass that allocates its data array on construction and
 * deallocates it on destruction.
 *
 * @ingroup meshtex-util
 */
template<typename Element>
class AllocatedMatrix : public Matrix<Element>
{
	std::size_t m_x, m_y;
	Element* m_data; 
public: // public methods

   /**
    * Constructor. Allocates a data array of the appropriate size.
    *
    * @param x Matrix x dimension.
    * @param y Matrix y dimension.
    */
   AllocatedMatrix(std::size_t x, std::size_t y) : m_x(x), m_y(y), m_data(_allocated = new Element[x*y]){}
//      Matrix(x, y, (_allocated = new Element[x*y])) {}

   /**
    * Destructor. Deallocates the data array.
    */
   ~AllocatedMatrix() { delete [] _allocated; }

private: // private member vars

   /**
    * Pointer to the data array so that the destructor can find it for deletion.
    */
   Element *_allocated;
};

#endif // #if !defined(INCLUDED_ALLOCATEDMATRIX_H)