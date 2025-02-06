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

#include "windowobserver.h"
#include "generic/callback.h"
#include "rect_t.h"

typedef Callback<void(rect_t)> RectangleCallback;

class View;

class SelectionSystemWindowObserver : public WindowObserver
{
public:
	virtual void setView( const View& view ) = 0;
	virtual void setRectangleDrawCallback( const RectangleCallback& callback ) = 0;
	virtual void incMouseMove( const WindowVector& delta ) = 0;
};

SelectionSystemWindowObserver* NewWindowObserver();

void SelectionSystem_connectTransformsCallbacks( const std::array<Callback<void(const char*)>, 4>& callbacks );

#include "math/vectorfwd.h"

void Scene_Intersect( const View& view, const Vector2& device_point, const Vector2& device_epsilon, Vector3& intersection );
