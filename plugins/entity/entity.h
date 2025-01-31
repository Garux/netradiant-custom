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

class EntityCreator;
EntityCreator& GetEntityCreator();

enum EGameType
{
	eGameTypeQuake3,
	eGameTypeQuake1,
	eGameTypeRTCW,
	eGameTypeDoom3,
};

extern EGameType g_gameType;

class FilterSystem;
void Entity_Construct( EGameType gameType = eGameTypeQuake3 );
void Entity_Destroy();

extern bool g_showNames;
extern bool g_showBboxes;
extern bool g_showConnections;
extern int g_showNamesDist;
extern int g_showNamesRatio;
extern bool g_showTargetNames;
extern bool g_showAngles;
extern bool g_lightRadii;
extern bool g_lightColorize;

extern bool g_stupidQuakeBug;
