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

#include "stream/stringstream.h"

class TexturePath
{
	CopiedString m_path;
public:
	// constructor is not strict wrt "textures/" prefix presence in the input
	// but this wont construct textures/textures/bla from textures/bla; 🤞🏿 this wont happen
	TexturePath( const char *path = "" ) : m_path( string_equal_prefix_nocase( path, "textures/" )
	                                               ? path
	                                               : StringStream<64>( "textures/", path ) )
	{}
	const char *get() const {
		return m_path.c_str();
	}
	const char *get_short() const {
		return m_path.c_str() + prefix_length;
	}
	static constexpr int max_length = ( 64 - 1 ); //minus null terminator
	static constexpr int prefix_length = ( std::size( "textures/" ) - 1 );
	static constexpr int max_prefixless_length = ( max_length - prefix_length );
};

struct ApertureDoorRS {
	bool fromWinding = false;
	int segments = 16;
	bool offsetStartAngle = false;

	TexturePath textureMain = "textures/base_floor/diamond2c";
	TexturePath textureTrim = "textures/base_trim/yellow_rustbx128";

	enum class Inner{
		none = 0,
		segmented = 1,
		sloped = 2,
	} innerType = Inner::none;
	TexturePath innerTextureMain = "textures/base_floor/clang_floor3blava";
	TexturePath innerTextureTrim = "textures/base_floor/achtung_clang";
	double innerDepth1 = 4;
	double innerDepth2 = 8;

	bool slopedSegments = false;
	bool slopedSegmentsRoundize = false;
	double slopedDepth1 = 16;
	double slopedDepth2 = 8;

	// sound is played in door entity origin
	// moving origin 1024u away makes it inaudible in Q3
	// note somehow setting origin via key or origin brush doesn't work; need to extend door body
	enum class Silence{
		none = 0,
		brush = 1,
	} silenceType = Silence::none;
	DoubleVector3 silenceBrushesOffset{ 4096, 0, 0 };

	bool speedUse = false; //or time
	double time = 2;
	double speed = 40;

	bool distanceSet = false; //or automatic from radius
	double distance = 256;
	double openAngle = 0;
	bool health = true;
	double wait = .25;
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
bool DoApertureDoorsBox( ApertureDoorRS* rs );
EMessageBoxReturn DoPathPlotterBox( PathPlotterRS* rs );
EMessageBoxReturn DoCTFColourChangeBox();
bool DoMakeChainBox( MakeChainRS* rs );
