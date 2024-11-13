/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"
#include "surface_extra.h"


/* -------------------------------------------------------------------------------

   ydnar: srf file module

   ------------------------------------------------------------------------------- */


static std::vector<surfaceExtra_t> surfaceExtras;
static surfaceExtra_t seDefault;



/*
   SetDefaultSampleSize()
   sets the default lightmap sample size
 */

void SetDefaultSampleSize( int sampleSize ){
	seDefault.sampleSize = sampleSize;
}



/*
   SetSurfaceExtra()
   stores extra (q3map2) data for the specific numbered drawsurface
 */

void SetSurfaceExtra( const mapDrawSurface_t& ds ){
	/* get a new extra */
	surfaceExtra_t& se = surfaceExtras.emplace_back( seDefault );

	/* copy out the relevant bits */
	se.mds = &ds;
	se.si = ds.shaderInfo;
	se.parentSurfaceNum = ds.parent != NULL ? ds.parent->outputNum : -1;
	se.entityNum = ds.entityNum;
	se.castShadows = ds.castShadows;
	se.recvShadows = ds.recvShadows;
	se.sampleSize = ds.sampleSize;
	se.longestCurve = ds.longestCurve;
	se.lightmapAxis = ds.lightmapAxis;

	/* debug code */
	//%	Sys_FPrintf( SYS_VRB, "SetSurfaceExtra(): entityNum = %d\n", ds.entityNum );
}



/*
   GetSurfaceExtra*()
   getter functions for extra surface data
 */

const surfaceExtra_t& GetSurfaceExtra( int num ){
	if ( num < 0 || size_t( num ) >= surfaceExtras.size() ) {
		return seDefault;
	}
	return surfaceExtras[ num ];
}



/*
   WriteSurfaceExtraFile()
   writes out a surface info file (<map>.srf)
 */

void WriteSurfaceExtraFile( const char *path ){
	/* dummy check */
	if ( strEmptyOrNull( path ) ) {
		return;
	}

	/* note it */
	Sys_Printf( "--- WriteSurfaceExtraFile ---\n" );

	/* open the file */
	const auto srfPath = StringStream( path, ".srf" );
	Sys_Printf( "Writing %s\n", srfPath.c_str() );
	FILE *sf = SafeOpenWrite( srfPath, "wt" );

	/* lap through the extras list */
	for ( int i = -1, size = surfaceExtras.size(); i < size; ++i )
	{
		/* get extra */
		const surfaceExtra_t * const se = &GetSurfaceExtra( i );

		/* default or surface num? */
		if ( i < 0 ) {
			fprintf( sf, "default" );
		}
		else{
			fprintf( sf, "%d", i );
		}

		/* valid map drawsurf? */
		if ( se->mds == NULL ) {
			fprintf( sf, "\n" );
		}
		else
		{
			fprintf( sf, " // %s V: %d I: %d %s\n",
			         surfaceTypeName( se->mds->type ),
			         se->mds->numVerts,
			         se->mds->numIndexes,
			         ( se->mds->planar ? "planar" : "" ) );
		}

		/* open braces */
		fprintf( sf, "{\n" );

		/* shader */
		if ( se->si != NULL ) {
			fprintf( sf, "\tshader %s\n", se->si->shader.c_str() );
		}

		/* parent surface number */
		if ( se->parentSurfaceNum != seDefault.parentSurfaceNum ) {
			fprintf( sf, "\tparent %d\n", se->parentSurfaceNum );
		}

		/* entity number */
		if ( se->entityNum != seDefault.entityNum ) {
			fprintf( sf, "\tentity %d\n", se->entityNum );
		}

		/* cast shadows */
		if ( se->castShadows != seDefault.castShadows || se == &seDefault ) {
			fprintf( sf, "\tcastShadows %d\n", se->castShadows );
		}

		/* recv shadows */
		if ( se->recvShadows != seDefault.recvShadows || se == &seDefault ) {
			fprintf( sf, "\treceiveShadows %d\n", se->recvShadows );
		}

		/* lightmap sample size */
		if ( se->sampleSize != seDefault.sampleSize || se == &seDefault ) {
			fprintf( sf, "\tsampleSize %d\n", se->sampleSize );
		}

		/* longest curve */
		if ( se->longestCurve != seDefault.longestCurve || se == &seDefault ) {
			fprintf( sf, "\tlongestCurve %f\n", se->longestCurve );
		}

		/* lightmap axis vector */
		if ( !VectorCompare( se->lightmapAxis, seDefault.lightmapAxis ) ) {
			fprintf( sf, "\tlightmapAxis ( %f %f %f )\n", se->lightmapAxis[ 0 ], se->lightmapAxis[ 1 ], se->lightmapAxis[ 2 ] );
		}

		/* close braces */
		fprintf( sf, "}\n\n" );
	}

	/* close the file */
	fclose( sf );
}



/*
   LoadSurfaceExtraFile()
   reads a surface info file (<map>.srf)
 */

void LoadSurfaceExtraFile( const char *path ){
	/* dummy check */
	if ( strEmptyOrNull( path ) ) {
		return;
	}

	/* load the file */
	const auto srfPath = StringStream( PathExtensionless( path ), ".srf" );

	/* parse the file */
	if( !LoadScriptFile( srfPath, -1 ) )
		Error( "" );

	/* tokenize it */
	while ( GetToken( true ) ) /* test for end of file */
	{
		surfaceExtra_t  *se;
		/* default? */
		if ( striEqual( token, "default" ) ) {
			se = &seDefault;
		}

		/* surface number */
		else
		{
			const int surfaceNum = atoi( token );
			if ( surfaceNum < 0 ) {
				Error( "ReadSurfaceExtraFile(): %s, line %d: bogus surface num %d", srfPath.c_str(), scriptline, surfaceNum );
			}
			if( size_t( surfaceNum ) >= surfaceExtras.size() ){
				if( size_t( surfaceNum ) >= surfaceExtras.capacity() ) // ensure that capacity grows efficiently, as it's not guaranteed for vector::resize()
					surfaceExtras.reserve( surfaceExtras.capacity() << 1 );
				surfaceExtras.resize( surfaceNum + 1, seDefault );
			}
			se = &surfaceExtras[ surfaceNum ];
		}

		/* handle { } section */
		if ( !( GetToken( true ) && strEqual( token, "{" ) ) ) {
			Error( "ReadSurfaceExtraFile(): %s, line %d: { not found", srfPath.c_str(), scriptline );
		}
		while ( GetToken( true ) && !strEqual( token, "}" ) )
		{
			/* shader */
			if ( striEqual( token, "shader" ) ) {
				GetToken( false );
				se->si = ShaderInfoForShader( token );
			}

			/* parent surface number */
			else if ( striEqual( token, "parent" ) ) {
				GetToken( false );
				se->parentSurfaceNum = atoi( token );
			}

			/* entity number */
			else if ( striEqual( token, "entity" ) ) {
				GetToken( false );
				se->entityNum = atoi( token );
			}

			/* cast shadows */
			else if ( striEqual( token, "castShadows" ) ) {
				GetToken( false );
				se->castShadows = atoi( token );
			}

			/* recv shadows */
			else if ( striEqual( token, "receiveShadows" ) ) {
				GetToken( false );
				se->recvShadows = atoi( token );
			}

			/* lightmap sample size */
			else if ( striEqual( token, "sampleSize" ) ) {
				GetToken( false );
				se->sampleSize = atoi( token );
			}

			/* longest curve */
			else if ( striEqual( token, "longestCurve" ) ) {
				GetToken( false );
				se->longestCurve = atof( token );
			}

			/* lightmap axis vector */
			else if ( striEqual( token, "lightmapAxis" ) ) {
				Parse1DMatrix( 3, se->lightmapAxis.data() );
			}

			/* ignore all other tokens on the line */
			while ( TokenAvailable() )
				GetToken( false );
		}
	}
}
