/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "generic/constant.h"
#include "math/vectorfwd.h"

template<typename T> class Plane3___;
typedef Plane3___<double> Plane3;
class Matrix4;
class AABB;
class Segment;

template<typename Enumeration> class EnumeratedValue;
struct VolumeIntersection;
typedef EnumeratedValue<VolumeIntersection> VolumeIntersectionValue;

class VolumeTest
{
public:

	/// \brief Returns true if \p point intersects volume.
	virtual bool TestPoint( const Vector3& point ) const = 0;
	/// \brief Returns true if \p segment intersects volume.
	virtual bool TestLine( const Segment& segment ) const = 0;
	/// \brief Returns true if \p plane faces towards volume.
	virtual bool TestPlane( const Plane3& plane ) const = 0;
	/// \brief Returns true if \p plane transformed by \p localToWorld faces the viewer.
	virtual bool TestPlane( const Plane3& plane, const Matrix4& localToWorld ) const = 0;
	/// \brief Returns the intersection of \p aabb and volume.
	virtual VolumeIntersectionValue TestAABB( const AABB& aabb ) const = 0;
	/// \brief Returns the intersection of \p aabb transformed by \p localToWorld and volume.
	virtual VolumeIntersectionValue TestAABB( const AABB& aabb, const Matrix4& localToWorld ) const = 0;

	virtual bool fill() const = 0;

	virtual const Matrix4& GetViewport() const = 0;
	virtual const Matrix4& GetProjection() const = 0;
	virtual const Matrix4& GetModelview() const = 0;

	virtual const Matrix4& GetViewMatrix() const = 0; //viewproj
	virtual const Vector3& getViewer() const = 0;
	virtual const Vector3& getViewDir() const = 0;
};

class Cullable
{
public:
	STRING_CONSTANT( Name, "Cullable" );

	virtual VolumeIntersectionValue intersectVolume( const VolumeTest& test, const Matrix4& localToWorld ) const = 0;
};
