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
#include "generic/callback.h"

typedef Callback<void(const char*)> StringImportCallback;
typedef Callback<void(const StringImportCallback&)> StringExportCallback;

class PreferenceSystem
{
public:
	INTEGER_CONSTANT( Version, 1 );
	STRING_CONSTANT( Name, "preferences" );

	virtual void registerPreference( const char* name, const StringImportCallback& importer, const StringExportCallback& exporter ) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<PreferenceSystem> GlobalPreferenceSystemModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<PreferenceSystem> GlobalPreferenceSystemModuleRef;

inline PreferenceSystem& GlobalPreferenceSystem(){
	return GlobalPreferenceSystemModule::getTable();
}
