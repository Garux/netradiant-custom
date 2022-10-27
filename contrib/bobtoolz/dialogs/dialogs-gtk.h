/*
   BobToolz plugin for GtkRadiant
   Copyright (C) 2001 Gordon Biggans

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#include "qerplugin.h"

struct BuildStairsRS {
	char mainTexture[256];
	char riserTexture[256];
	int direction;
	int style;
	int stairHeight;
	bool bUseDetail;
};

struct ResetTextureRS {
	bool bResetTextureName;
	char textureName[256];
	char newTextureName[256];

	bool bResetScale[2];
	float fScale[2];

	bool bResetShift[2];
	float fShift[2];

	bool bResetRotation;
	int rotation;
};

struct IntersectRS {
	int nBrushOptions;
	bool bUseDetail;
	bool bDuplicateOnly;
};

struct PolygonRS {
	bool bUseBorder;
	bool bInverse;
	bool bAlignTop;
	int nSides;
	int nBorderWidth;
};

struct DoorRS {
	char mainTexture[256];
	char trimTexture[256];
	bool bScaleMainH;
	bool bScaleMainV;
	bool bScaleTrimH;
	bool bScaleTrimV;
	int nOrientation;
};

struct PathPlotterRS {
	int nPoints;
	float fMultiplier;
	float fGravity;
	bool bNoUpdate;
	bool bShowExtra;
};

struct MakeChainRS {
	char linkName[256];
	int linkNum;
};


EMessageBoxReturn DoMessageBox( const char* lpText, const char* lpCaption, EMessageBoxType type = EMessageBoxType::Info );
bool DoIntersectBox( IntersectRS* rs );
bool DoPolygonBox( PolygonRS* rs );
EMessageBoxReturn DoResetTextureBox( ResetTextureRS* rs );
bool DoBuildStairsBox( BuildStairsRS* rs );
bool DoDoorsBox( DoorRS* rs );
EMessageBoxReturn DoPathPlotterBox( PathPlotterRS* rs );
EMessageBoxReturn DoCTFColourChangeBox();
bool DoMakeChainBox( MakeChainRS* rs );
