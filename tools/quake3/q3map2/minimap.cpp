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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"

/* minimap stuff */

struct minimap_t
{
	const bspModel_t *model;
	int width;
	int height;
	int samples;
	float *sample_offsets;
	float sharpen_boxmult;
	float sharpen_centermult;
	float boost, brightness, contrast;
	float *data1f;
	float *sharpendata1f;
	Vector3 mins, size;
};

static minimap_t minimap;

static bool BrushIntersectionWithLine( const bspBrush_t& brush, const Vector3& start, const Vector3& dir, float *t_in, float *t_out ){
	bool in = false, out = false;

	for ( const bspBrushSide_t& side : Span( &bspBrushSides[brush.firstSide], brush.numSides ) )
	{
		const bspPlane_t& p = bspPlanes[side.planeNum];
		float sn = vector3_dot( start, p.normal() );
		float dn = vector3_dot( dir, p.normal() );
		if ( dn == 0 ) {
			if ( sn > p.dist() ) {
				return false; // outside!
			}
		}
		else
		{
			float t = ( p.dist() - sn ) / dn;
			if ( dn < 0 ) {
				if ( !in || t > *t_in ) {
					*t_in = t;
					in = true;
					// as t_in can only increase, and t_out can only decrease, early out
					if ( out && *t_in >= *t_out ) {
						return false;
					}
				}
			}
			else
			{
				if ( !out || t < *t_out ) {
					*t_out = t;
					out = true;
					// as t_in can only increase, and t_out can only decrease, early out
					if ( in && *t_in >= *t_out ) {
						return false;
					}
				}
			}
		}
	}
	return in && out;
}

static float MiniMapSample( float x, float y ){
	float t0, t1;
	float samp;
	int cnt;

	const Vector3 org( x, y, 0 );
	const Vector3 dir( g_vector3_axis_z );

	cnt = 0;
	samp = 0;
	for ( int i = 0; i < minimap.model->numBSPBrushes; ++i )
	{
		const int bi = minimap.model->firstBSPBrush + i;
		if ( opaqueBrushes[bi] ) {
			const bspBrush_t& b = bspBrushes[bi];

			// sort out mins/maxs of the brush
			const bspBrushSide_t *s = &bspBrushSides[b.firstSide];
			if ( x < -bspPlanes[s[0].planeNum].dist() ) {
				continue;
			}
			if ( x > +bspPlanes[s[1].planeNum].dist() ) {
				continue;
			}
			if ( y < -bspPlanes[s[2].planeNum].dist() ) {
				continue;
			}
			if ( y > +bspPlanes[s[3].planeNum].dist() ) {
				continue;
			}

			if ( BrushIntersectionWithLine( b, org, dir, &t0, &t1 ) ) {
				samp += t1 - t0;
				++cnt;
			}
		}
	}

	return samp;
}

inline void RandomVector2f( float v[2] ){
	do
	{
		v[0] = 2 * Random() - 1;
		v[1] = 2 * Random() - 1;
	}
	while ( v[0] * v[0] + v[1] * v[1] > 1 );
}

static void MiniMapRandomlySupersampled( int y ){
	int x, i;
	float *p = &minimap.data1f[y * minimap.width];
	float ymin = minimap.mins[1] + minimap.size[1] * ( y / (float) minimap.height );
	float dx   =                   minimap.size[0]       / (float) minimap.width;
	float dy   =                   minimap.size[1]       / (float) minimap.height;
	float uv[2];
	float thisval;

	for ( x = 0; x < minimap.width; ++x )
	{
		float xmin = minimap.mins[0] + minimap.size[0] * ( x / (float) minimap.width );
		float val = 0;

		for ( i = 0; i < minimap.samples; ++i )
		{
			RandomVector2f( uv );
			thisval = MiniMapSample(
			              xmin + ( uv[0] + 0.5 ) * dx, /* exaggerated random pattern for better results */
			              ymin + ( uv[1] + 0.5 ) * dy  /* exaggerated random pattern for better results */
			          );
			val += thisval;
		}
		val /= minimap.samples * minimap.size[2];
		*p++ = val;
	}
}

static void MiniMapSupersampled( int y ){
	int x, i;
	float *p = &minimap.data1f[y * minimap.width];
	float ymin = minimap.mins[1] + minimap.size[1] * ( y / (float) minimap.height );
	float dx   =                   minimap.size[0]       / (float) minimap.width;
	float dy   =                   minimap.size[1]       / (float) minimap.height;

	for ( x = 0; x < minimap.width; ++x )
	{
		float xmin = minimap.mins[0] + minimap.size[0] * ( x / (float) minimap.width );
		float val = 0;

		for ( i = 0; i < minimap.samples; ++i )
		{
			float thisval = MiniMapSample(
			                    xmin + minimap.sample_offsets[2 * i + 0] * dx,
			                    ymin + minimap.sample_offsets[2 * i + 1] * dy
			                );
			val += thisval;
		}
		val /= minimap.samples * minimap.size[2];
		*p++ = val;
	}
}

static void MiniMapNoSupersampling( int y ){
	int x;
	float *p = &minimap.data1f[y * minimap.width];
	float ymin = minimap.mins[1] + minimap.size[1] * ( ( y + 0.5 ) / minimap.height );

	for ( x = 0; x < minimap.width; ++x )
	{
		float xmin = minimap.mins[0] + minimap.size[0] * ( ( x + 0.5 ) / minimap.width );
		*p++ = MiniMapSample( xmin, ymin ) / minimap.size[2];
	}
}

static void MiniMapSharpen( int y ){
	int x;
	const bool up = ( y > 0 );
	const bool down = ( y < minimap.height - 1 );
	float *p = &minimap.data1f[y * minimap.width];
	float *q = &minimap.sharpendata1f[y * minimap.width];

	for ( x = 0; x < minimap.width; ++x )
	{
		const bool left = ( x > 0 );
		const bool right = ( x < minimap.width - 1 );
		float val = p[0] * minimap.sharpen_centermult;

		if ( left && up ) {
			val += p[-1 - minimap.width] * minimap.sharpen_boxmult;
		}
		if ( left && down ) {
			val += p[-1 + minimap.width] * minimap.sharpen_boxmult;
		}
		if ( right && up ) {
			val += p[+1 - minimap.width] * minimap.sharpen_boxmult;
		}
		if ( right && down ) {
			val += p[+1 + minimap.width] * minimap.sharpen_boxmult;
		}

		if ( left ) {
			val += p[-1] * minimap.sharpen_boxmult;
		}
		if ( right ) {
			val += p[+1] * minimap.sharpen_boxmult;
		}
		if ( up ) {
			val += p[-minimap.width] * minimap.sharpen_boxmult;
		}
		if ( down ) {
			val += p[+minimap.width] * minimap.sharpen_boxmult;
		}

		++p;
		*q++ = val;
	}
}

static void MiniMapContrastBoost( int y ){
	int x;
	float *q = &minimap.data1f[y * minimap.width];
	for ( x = 0; x < minimap.width; ++x )
	{
		*q = *q * minimap.boost / ( ( minimap.boost - 1 ) * *q + 1 );
		++q;
	}
}

static void MiniMapBrightnessContrast( int y ){
	int x;
	float *q = &minimap.data1f[y * minimap.width];
	for ( x = 0; x < minimap.width; ++x )
	{
		*q = *q * minimap.contrast + minimap.brightness;
		++q;
	}
}

static void MiniMapMakeMinsMaxs( const Vector3& mins_in, const Vector3& maxs_in, float border, bool keepaspect ){
	Vector3 mins = mins_in;
	Vector3 maxs = maxs_in;
	Vector3 extend;

	// line compatible to nexuiz mapinfo
	Sys_Printf( "size %f %f %f %f %f %f\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2] );

	if ( keepaspect ) {
		extend = maxs - mins;
		if ( extend[1] > extend[0] ) {
			mins[0] -= ( extend[1] - extend[0] ) * 0.5;
			maxs[0] += ( extend[1] - extend[0] ) * 0.5;
		}
		else
		{
			mins[1] -= ( extend[0] - extend[1] ) * 0.5;
			maxs[1] += ( extend[0] - extend[1] ) * 0.5;
		}
	}

	/* border: amount of black area around the image */
	/* input: border, 1-2*border, border but we need border/(1-2*border) */

	extend = ( maxs - mins ) * ( border / ( 1 - 2 * border ) );

	mins -= extend;
	maxs += extend;

	minimap.mins = mins;
	minimap.size = maxs - mins;

	// line compatible to nexuiz mapinfo
	Sys_Printf( "size_texcoords %f %f %f %f %f %f\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2] );
}

/*
   MiniMapSetupBrushes()
   determines solid non-sky brushes in the world
 */

static void MiniMapSetupBrushes(){
	SetupBrushesFlags( C_SOLID | C_SKY, C_SOLID, 0, 0 );
	// at least one must be solid
	// none may be sky
	// not all may be nodraw
}

static bool MiniMapEvaluateSampleOffsets( int *bestj, int *bestk, float *bestval ){
	float val, dx, dy;
	int j, k;

	*bestj = *bestk = -1;
	*bestval = 3; /* max possible val is 2 */

	for ( j = 0; j < minimap.samples; ++j )
		for ( k = j + 1; k < minimap.samples; ++k )
		{
			dx = minimap.sample_offsets[2 * j + 0] - minimap.sample_offsets[2 * k + 0];
			dy = minimap.sample_offsets[2 * j + 1] - minimap.sample_offsets[2 * k + 1];
			if ( dx > +0.5f ) {
				dx -= 1;
			}
			if ( dx < -0.5f ) {
				dx += 1;
			}
			if ( dy > +0.5f ) {
				dy -= 1;
			}
			if ( dy < -0.5f ) {
				dy += 1;
			}
			val = dx * dx + dy * dy;
			if ( val < *bestval ) {
				*bestj = j;
				*bestk = k;
				*bestval = val;
			}
		}

	return *bestval < 3;
}

static void MiniMapMakeSampleOffsets(){
	int i, j, k, jj, kk;
	float val, valj, valk, sx, sy, rx, ry;

	Sys_Printf( "Generating good sample offsets (this may take a while)...\n" );

	/* start with entirely random samples */
	for ( i = 0; i < minimap.samples; ++i )
	{
		minimap.sample_offsets[2 * i + 0] = Random();
		minimap.sample_offsets[2 * i + 1] = Random();
	}

	for ( i = 0; i < 1000; ++i )
	{
		if ( MiniMapEvaluateSampleOffsets( &j, &k, &val ) ) {
			sx = minimap.sample_offsets[2 * j + 0];
			sy = minimap.sample_offsets[2 * j + 1];
			minimap.sample_offsets[2 * j + 0] = rx = Random();
			minimap.sample_offsets[2 * j + 1] = ry = Random();
			if ( !MiniMapEvaluateSampleOffsets( &jj, &kk, &valj ) ) {
				valj = -1;
			}
			minimap.sample_offsets[2 * j + 0] = sx;
			minimap.sample_offsets[2 * j + 1] = sy;

			sx = minimap.sample_offsets[2 * k + 0];
			sy = minimap.sample_offsets[2 * k + 1];
			minimap.sample_offsets[2 * k + 0] = rx;
			minimap.sample_offsets[2 * k + 1] = ry;
			if ( !MiniMapEvaluateSampleOffsets( &jj, &kk, &valk ) ) {
				valk = -1;
			}
			minimap.sample_offsets[2 * k + 0] = sx;
			minimap.sample_offsets[2 * k + 1] = sy;

			if ( valj > valk ) {
				if ( valj > val ) {
					/* valj is the greatest */
					minimap.sample_offsets[2 * j + 0] = rx;
					minimap.sample_offsets[2 * j + 1] = ry;
					i = -1;
				}
				else
				{
					/* valj is the greater and it is useless - forget it */
				}
			}
			else
			{
				if ( valk > val ) {
					/* valk is the greatest */
					minimap.sample_offsets[2 * k + 0] = rx;
					minimap.sample_offsets[2 * k + 1] = ry;
					i = -1;
				}
				else
				{
					/* valk is the greater and it is useless - forget it */
				}
			}
		}
		else{
			break;
		}
	}
}

static void MergeRelativePath( char *out, const char *absolute, const char *relative ){
	const char *endpos = absolute + strlen( absolute );
	while ( endpos != absolute && path_separator( endpos[-1] ) )
		--endpos;
	while ( strEqualPrefix( relative, "../" ) || strEqualPrefix( relative, "..\\" ) )
	{
		relative += 3;
		while ( endpos != absolute )
		{
			--endpos;
			if ( path_separator( *endpos ) ) {
				break;
			}
		}
		while ( endpos != absolute && path_separator( endpos[-1] ) )
			--endpos;
	}
	memcpy( out, absolute, endpos - absolute );
	out[endpos - absolute] = '/';
	strcpy( out + ( endpos - absolute + 1 ), relative );
}

int MiniMapBSPMain( Args& args ){
	char minimapFilename[1024];
	bool autolevel;
	float minimapSharpen;
	float border;
	byte *data4b, *p;
	float *q;
	int x, y;
	EMiniMapMode mode;
	bool keepaspect;

	/* arg checking */
	if ( args.empty() ) {
		Sys_Printf( "Usage: q3map2 [-v] -minimap [-size n] [-sharpen f] [-samples n | -random n] [-o filename.tga] [-minmax Xmin Ymin Zmin Xmax Ymax Zmax] <mapname>\n" );
		return 0;
	}

	/* load the BSP first */
	const char *fileName = args.takeBack();
	strcpy( source, ExpandArg( fileName ) );
	path_set_extension( source, ".bsp" );
	Sys_Printf( "Loading %s\n", source );
	LoadShaderInfo();
	LoadBSPFile( source );

	minimap.model = &bspModels[0];
	Vector3 mins = minimap.model->minmax.mins;
	Vector3 maxs = minimap.model->minmax.maxs;

	strClear( minimapFilename );
	minimapSharpen = g_game->miniMapSharpen;
	minimap.width  =
	minimap.height = g_game->miniMapSize;
	border         = g_game->miniMapBorder;
	keepaspect     = g_game->miniMapKeepAspect;
	mode           = g_game->miniMapMode;

	autolevel = false;
	minimap.samples = 1;
	minimap.sample_offsets = nullptr;
	minimap.boost = 1;
	minimap.brightness = 0;
	minimap.contrast = 1;

	/* process arguments */
	{
		while( args.takeArg( "-size" ) ) {
			minimap.width = minimap.height = atoi( args.takeNext() );
			Sys_Printf( "Image size set to %i\n", minimap.width );
		}
		while( args.takeArg( "-sharpen" ) ) {
			minimapSharpen = atof( args.takeNext() );
			Sys_Printf( "Sharpening coefficient set to %f\n", minimapSharpen );
		}
		while( args.takeArg( "-samples" ) ) {
			minimap.samples = atoi( args.takeNext() );
			Sys_Printf( "Samples set to %i\n", minimap.samples );
			free( minimap.sample_offsets );
			minimap.sample_offsets = safe_malloc( 2 * sizeof( *minimap.sample_offsets ) * minimap.samples );
			MiniMapMakeSampleOffsets();
		}
		while( args.takeArg( "-random" ) ) {
			minimap.samples = atoi( args.takeNext() );
			Sys_Printf( "Random samples set to %i\n", minimap.samples );
			free( minimap.sample_offsets );
			minimap.sample_offsets = nullptr;
		}
		while( args.takeArg( "-border" ) ) {
			border = atof( args.takeNext() );
			Sys_Printf( "Border set to %f\n", border );
		}
		while( args.takeArg( "-keepaspect" ) ) {
			keepaspect = true;
			Sys_Printf( "Keeping aspect ratio by letterboxing\n", border );
		}
		while( args.takeArg( "-nokeepaspect" ) ) {
			keepaspect = false;
			Sys_Printf( "Not keeping aspect ratio\n", border );
		}
		while( args.takeArg( "-o" ) ) {
			strcpy( minimapFilename, args.takeNext() );
			Sys_Printf( "Output file name set to %s\n", minimapFilename );
		}
		while( args.takeArg( "-minmax" ) ) {
			mins[0] = atof( args.takeNext() );
			mins[1] = atof( args.takeNext() );
			mins[2] = atof( args.takeNext() );
			maxs[0] = atof( args.takeNext() );
			maxs[1] = atof( args.takeNext() );
			maxs[2] = atof( args.takeNext() );
			Sys_Printf( "Map mins/maxs overridden\n" );
		}
		while( args.takeArg( "-gray" ) ) {
			mode = EMiniMapMode::Gray;
			Sys_Printf( "Writing as white-on-black image\n" );
		}
		while( args.takeArg( "-black" ) ) {
			mode = EMiniMapMode::Black;
			Sys_Printf( "Writing as black alpha image\n" );
		}
		while( args.takeArg( "-white" ) ) {
			mode = EMiniMapMode::White;
			Sys_Printf( "Writing as white alpha image\n" );
		}
		while( args.takeArg( "-boost" ) ) {
			minimap.boost = atof( args.takeNext() );
			Sys_Printf( "Contrast boost set to %f\n", minimap.boost );
		}
		while( args.takeArg( "-brightness" ) ) {
			minimap.brightness = atof( args.takeNext() );
			Sys_Printf( "Brightness set to %f\n", minimap.brightness );
		}
		while( args.takeArg( "-contrast" ) ) {
			minimap.contrast = atof( args.takeNext() );
			Sys_Printf( "Contrast set to %f\n", minimap.contrast );
		}
		while( args.takeArg( "-autolevel" ) ) {
			autolevel = true;
			Sys_Printf( "Auto level enabled\n", border );
		}
		while( args.takeArg( "-noautolevel" ) ) {
			autolevel = false;
			Sys_Printf( "Auto level disabled\n", border );
		}
	}

	MiniMapMakeMinsMaxs( mins, maxs, border, keepaspect );

	if ( strEmpty( minimapFilename ) ) {
		const CopiedString basename( PathFilename( source ) );
		const CopiedString path( PathFilenameless( source ) );
		char relativeMinimapFilename[1024];
		sprintf( relativeMinimapFilename, g_game->miniMapNameFormat, basename.c_str() );
		MergeRelativePath( minimapFilename, path.c_str(), relativeMinimapFilename );
		Sys_Printf( "Output file name automatically set to %s\n", minimapFilename );
	}
	Q_mkdir( CopiedString( PathFilenameless( minimapFilename ) ).c_str() );

	if ( minimapSharpen >= 0 ) {
		minimap.sharpen_centermult = 8 * minimapSharpen + 1;
		minimap.sharpen_boxmult    =    -minimapSharpen;
	}

	minimap.data1f = safe_malloc( minimap.width * minimap.height * sizeof( *minimap.data1f ) );
	data4b = safe_malloc( minimap.width * minimap.height * 4 );
	if ( minimapSharpen >= 0 ) {
		minimap.sharpendata1f = safe_malloc( minimap.width * minimap.height * sizeof( *minimap.data1f ) );
	}

	MiniMapSetupBrushes();

	if ( minimap.samples <= 1 ) {
		Sys_Printf( "\n--- MiniMapNoSupersampling (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, true, MiniMapNoSupersampling );
	}
	else
	{
		if ( minimap.sample_offsets ) {
			Sys_Printf( "\n--- MiniMapSupersampled (%d) ---\n", minimap.height );
			RunThreadsOnIndividual( minimap.height, true, MiniMapSupersampled );
		}
		else
		{
			Sys_Printf( "\n--- MiniMapRandomlySupersampled (%d) ---\n", minimap.height );
			RunThreadsOnIndividual( minimap.height, true, MiniMapRandomlySupersampled );
		}
	}

	if ( minimap.boost != 1 ) {
		Sys_Printf( "\n--- MiniMapContrastBoost (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, true, MiniMapContrastBoost );
	}

	if ( autolevel ) {
		Sys_Printf( "\n--- MiniMapAutoLevel (%d) ---\n", minimap.height );
		float mi = 1, ma = 0;
		float s, o;

		// TODO threads!
		q = minimap.data1f;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				float v = *q++;
				value_minimize( mi, v );
				value_maximize( ma, v );
			}
		if ( ma > mi ) {
			s = 1 / ( ma - mi );
			o = mi / ( ma - mi );

			// equations:
			//   brightness + contrast * v
			// after autolevel:
			//   brightness + contrast * (v * s - o)
			// =
			//   (brightness - contrast * o) + (contrast * s) * v
			minimap.brightness = minimap.brightness - minimap.contrast * o;
			minimap.contrast *= s;

			Sys_Printf( "Auto level: Brightness changed to %f\n", minimap.brightness );
			Sys_Printf( "Auto level: Contrast changed to %f\n", minimap.contrast );
		}
		else{
			Sys_Printf( "Auto level: failed because all pixels are the same value\n" );
		}
	}

	if ( minimap.brightness != 0 || minimap.contrast != 1 ) {
		Sys_Printf( "\n--- MiniMapBrightnessContrast (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, true, MiniMapBrightnessContrast );
	}

	if ( minimap.sharpendata1f ) {
		Sys_Printf( "\n--- MiniMapSharpen (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, true, MiniMapSharpen );
		q = minimap.sharpendata1f;
	}
	else
	{
		q = minimap.data1f;
	}

	Sys_Printf( "\nConverting..." );

	switch ( mode )
	{
	case EMiniMapMode::Gray:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				*p++ = std::clamp( *q++, 0.f, 255.f / 256.f ) * 256;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGAGray( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	case EMiniMapMode::Black:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				*p++ = 0;
				*p++ = 0;
				*p++ = 0;
				*p++ = std::clamp( *q++, 0.f, 255.f / 256.f ) * 256;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGA( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	case EMiniMapMode::White:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				*p++ = 255;
				*p++ = 255;
				*p++ = 255;
				*p++ = std::clamp( *q++, 0.f, 255.f / 256.f ) * 256;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGA( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	}

	Sys_Printf( " done.\n" );

	/* return to sender */
	return 0;
}
