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

#if !defined( INCLUDED_IBRUSH_H )
#define INCLUDED_IBRUSH_H

#include "generic/constant.h"
#include "generic/callback.h"
#include "generic/vector.h"
#include "itexdef.h"

namespace scene
{
class Node;
}

class _QERFaceData
{
public:
_QERFaceData() : m_shader( "" ), contents( 0 ), flags( 0 ), value( 0 ){
}
Vector3 m_p0;
Vector3 m_p1;
Vector3 m_p2;
texdef_t m_texdef;
const char* m_shader;
int contents;
int flags;
int value;
};

typedef Callback1<const _QERFaceData&> BrushFaceDataCallback;

class BrushCreator
{
public:
INTEGER_CONSTANT( Version, 1 );
STRING_CONSTANT( Name, "brush" );
virtual scene::Node& createBrush() = 0;
virtual bool useAlternativeTextureProjection() const = 0;
virtual void Brush_forEachFace( scene::Node& brush, const BrushFaceDataCallback& callback ) = 0;
virtual bool Brush_addFace( scene::Node& brush, const _QERFaceData& faceData ) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<BrushCreator> GlobalBrushModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<BrushCreator> GlobalBrushModuleRef;

inline BrushCreator& GlobalBrushCreator(){
	return GlobalBrushModule::getTable();
}

#endif
