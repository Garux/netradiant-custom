/* -------------------------------------------------------------------------------;

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



/* marker */
#define MAIN_C



/* dependencies */
#include "q3map2.h"



/*
   Random()
   returns a pseudorandom number between 0 and 1
 */

vec_t Random( void ){
	return (vec_t) rand() / RAND_MAX;
}


char *Q_strncpyz( char *dst, const char *src, size_t len ) {
	if ( len == 0 ) {
		abort();
	}

	strncpy( dst, src, len );
	dst[ len - 1 ] = '\0';
	return dst;
}


char *Q_strcat( char *dst, size_t dlen, const char *src ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, dlen - n );
}


char *Q_strncat( char *dst, size_t dlen, const char *src, size_t slen ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, MIN( slen, dlen - n ) );
}


/*
   ExitQ3Map()
   cleanup routine
 */

static void ExitQ3Map( void ){
	BSPFilesCleanup();
	if ( mapDrawSurfs != NULL ) {
		free( mapDrawSurfs );
	}
}



/* minimap stuff */

typedef struct minimap_s
{
	bspModel_t *model;
	int width;
	int height;
	int samples;
	float *sample_offsets;
	float sharpen_boxmult;
	float sharpen_centermult;
	float boost, brightness, contrast;
	float *data1f;
	float *sharpendata1f;
	vec3_t mins, size;
}
minimap_t;
static minimap_t minimap;

qboolean BrushIntersectionWithLine( bspBrush_t *brush, vec3_t start, vec3_t dir, float *t_in, float *t_out ){
	int i;
	qboolean in = qfalse, out = qfalse;
	bspBrushSide_t *sides = &bspBrushSides[brush->firstSide];

	for ( i = 0; i < brush->numSides; ++i )
	{
		bspPlane_t *p = &bspPlanes[sides[i].planeNum];
		float sn = DotProduct( start, p->normal );
		float dn = DotProduct( dir, p->normal );
		if ( dn == 0 ) {
			if ( sn > p->dist ) {
				return qfalse; // outside!
			}
		}
		else
		{
			float t = ( p->dist - sn ) / dn;
			if ( dn < 0 ) {
				if ( !in || t > *t_in ) {
					*t_in = t;
					in = qtrue;
					// as t_in can only increase, and t_out can only decrease, early out
					if ( out && *t_in >= *t_out ) {
						return qfalse;
					}
				}
			}
			else
			{
				if ( !out || t < *t_out ) {
					*t_out = t;
					out = qtrue;
					// as t_in can only increase, and t_out can only decrease, early out
					if ( in && *t_in >= *t_out ) {
						return qfalse;
					}
				}
			}
		}
	}
	return in && out;
}

static float MiniMapSample( float x, float y ){
	vec3_t org, dir;
	int i, bi;
	float t0, t1;
	float samp;
	bspBrush_t *b;
	bspBrushSide_t *s;
	int cnt;

	org[0] = x;
	org[1] = y;
	org[2] = 0;
	dir[0] = 0;
	dir[1] = 0;
	dir[2] = 1;

	cnt = 0;
	samp = 0;
	for ( i = 0; i < minimap.model->numBSPBrushes; ++i )
	{
		bi = minimap.model->firstBSPBrush + i;
		if ( opaqueBrushes[bi >> 3] & ( 1 << ( bi & 7 ) ) ) {
			b = &bspBrushes[bi];

			// sort out mins/maxs of the brush
			s = &bspBrushSides[b->firstSide];
			if ( x < -bspPlanes[s[0].planeNum].dist ) {
				continue;
			}
			if ( x > +bspPlanes[s[1].planeNum].dist ) {
				continue;
			}
			if ( y < -bspPlanes[s[2].planeNum].dist ) {
				continue;
			}
			if ( y > +bspPlanes[s[3].planeNum].dist ) {
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

void RandomVector2f( float v[2] ){
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
	float dx   =                   minimap.size[0]      / (float) minimap.width;
	float dy   =                   minimap.size[1]      / (float) minimap.height;
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
	float dx   =                   minimap.size[0]      / (float) minimap.width;
	float dy   =                   minimap.size[1]      / (float) minimap.height;

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
	float ymin = minimap.mins[1] + minimap.size[1] * ( ( y + 0.5 ) / (float) minimap.height );

	for ( x = 0; x < minimap.width; ++x )
	{
		float xmin = minimap.mins[0] + minimap.size[0] * ( ( x + 0.5 ) / (float) minimap.width );
		*p++ = MiniMapSample( xmin, ymin ) / minimap.size[2];
	}
}

static void MiniMapSharpen( int y ){
	int x;
	qboolean up = ( y > 0 );
	qboolean down = ( y < minimap.height - 1 );
	float *p = &minimap.data1f[y * minimap.width];
	float *q = &minimap.sharpendata1f[y * minimap.width];

	for ( x = 0; x < minimap.width; ++x )
	{
		qboolean left = ( x > 0 );
		qboolean right = ( x < minimap.width - 1 );
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

void MiniMapMakeMinsMaxs( vec3_t mins_in, vec3_t maxs_in, float border, qboolean keepaspect ){
	vec3_t mins, maxs, extend;
	VectorCopy( mins_in, mins );
	VectorCopy( maxs_in, maxs );

	// line compatible to nexuiz mapinfo
	Sys_Printf( "size %f %f %f %f %f %f\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2] );

	if ( keepaspect ) {
		VectorSubtract( maxs, mins, extend );
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

	VectorSubtract( maxs, mins, extend );
	VectorScale( extend, border / ( 1 - 2 * border ), extend );

	VectorSubtract( mins, extend, mins );
	VectorAdd( maxs, extend, maxs );

	VectorCopy( mins, minimap.mins );
	VectorSubtract( maxs, mins, minimap.size );

	// line compatible to nexuiz mapinfo
	Sys_Printf( "size_texcoords %f %f %f %f %f %f\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2] );
}

/*
   MiniMapSetupBrushes()
   determines solid non-sky brushes in the world
 */

void MiniMapSetupBrushes( void ){
	SetupBrushesFlags( C_SOLID | C_SKY, C_SOLID, 0, 0 );
	// at least one must be solid
	// none may be sky
	// not all may be nodraw
}

qboolean MiniMapEvaluateSampleOffsets( int *bestj, int *bestk, float *bestval ){
	float val, dx, dy;
	int j, k;

	*bestj = *bestk = -1;
	*bestval = 3; /* max possible val is 2 */

	for ( j = 0; j < minimap.samples; ++j )
		for ( k = j + 1; k < minimap.samples; ++k )
		{
			dx = minimap.sample_offsets[2 * j + 0] - minimap.sample_offsets[2 * k + 0];
			dy = minimap.sample_offsets[2 * j + 1] - minimap.sample_offsets[2 * k + 1];
			if ( dx > +0.5 ) {
				dx -= 1;
			}
			if ( dx < -0.5 ) {
				dx += 1;
			}
			if ( dy > +0.5 ) {
				dy -= 1;
			}
			if ( dy < -0.5 ) {
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

void MiniMapMakeSampleOffsets(){
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

void MergeRelativePath( char *out, const char *absolute, const char *relative ){
	const char *endpos = absolute + strlen( absolute );
	while ( endpos != absolute && ( endpos[-1] == '/' || endpos[-1] == '\\' ) )
		--endpos;
	while ( relative[0] == '.' && relative[1] == '.' && ( relative[2] == '/' || relative[2] == '\\' ) )
	{
		relative += 3;
		while ( endpos != absolute )
		{
			--endpos;
			if ( *endpos == '/' || *endpos == '\\' ) {
				break;
			}
		}
		while ( endpos != absolute && ( endpos[-1] == '/' || endpos[-1] == '\\' ) )
			--endpos;
	}
	memcpy( out, absolute, endpos - absolute );
	out[endpos - absolute] = '/';
	strcpy( out + ( endpos - absolute + 1 ), relative );
}

int MiniMapBSPMain( int argc, char **argv ){
	char minimapFilename[1024];
	char basename[1024];
	char path[1024];
	char relativeMinimapFilename[1024];
	qboolean autolevel;
	float minimapSharpen;
	float border;
	byte *data4b, *p;
	float *q;
	int x, y;
	int i;
	miniMapMode_t mode;
	vec3_t mins, maxs;
	qboolean keepaspect;

	/* arg checking */
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map [-v] -minimap [-size n] [-sharpen f] [-samples n | -random n] [-o filename.tga] [-minmax Xmin Ymin Zmin Xmax Ymax Zmax] <mapname>\n" );
		return 0;
	}

	/* load the BSP first */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	Sys_Printf( "Loading %s\n", source );
	//BeginMapShaderFile( source ); //do not delete q3map2_*.shader on minimap generation
	LoadShaderInfo();
	LoadBSPFile( source );

	minimap.model = &bspModels[0];
	VectorCopy( minimap.model->mins, mins );
	VectorCopy( minimap.model->maxs, maxs );

	*minimapFilename = 0;
	minimapSharpen = game->miniMapSharpen;
	minimap.width = minimap.height = game->miniMapSize;
	border = game->miniMapBorder;
	keepaspect = game->miniMapKeepAspect;
	mode = game->miniMapMode;

	autolevel = qfalse;
	minimap.samples = 1;
	minimap.sample_offsets = NULL;
	minimap.boost = 1.0;
	minimap.brightness = 0.0;
	minimap.contrast = 1.0;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		if ( !strcmp( argv[ i ],  "-size" ) ) {
			minimap.width = minimap.height = atoi( argv[i + 1] );
			i++;
			Sys_Printf( "Image size set to %i\n", minimap.width );
		}
		else if ( !strcmp( argv[ i ],  "-sharpen" ) ) {
			minimapSharpen = atof( argv[i + 1] );
			i++;
			Sys_Printf( "Sharpening coefficient set to %f\n", minimapSharpen );
		}
		else if ( !strcmp( argv[ i ],  "-samples" ) ) {
			minimap.samples = atoi( argv[i + 1] );
			i++;
			Sys_Printf( "Samples set to %i\n", minimap.samples );
			if ( minimap.sample_offsets ) {
				free( minimap.sample_offsets );
			}
			minimap.sample_offsets = malloc( 2 * sizeof( *minimap.sample_offsets ) * minimap.samples );
			MiniMapMakeSampleOffsets();
		}
		else if ( !strcmp( argv[ i ],  "-random" ) ) {
			minimap.samples = atoi( argv[i + 1] );
			i++;
			Sys_Printf( "Random samples set to %i\n", minimap.samples );
			if ( minimap.sample_offsets ) {
				free( minimap.sample_offsets );
			}
			minimap.sample_offsets = NULL;
		}
		else if ( !strcmp( argv[ i ],  "-border" ) ) {
			border = atof( argv[i + 1] );
			i++;
			Sys_Printf( "Border set to %f\n", border );
		}
		else if ( !strcmp( argv[ i ],  "-keepaspect" ) ) {
			keepaspect = qtrue;
			Sys_Printf( "Keeping aspect ratio by letterboxing\n", border );
		}
		else if ( !strcmp( argv[ i ],  "-nokeepaspect" ) ) {
			keepaspect = qfalse;
			Sys_Printf( "Not keeping aspect ratio\n", border );
		}
		else if ( !strcmp( argv[ i ],  "-o" ) ) {
			strcpy( minimapFilename, argv[i + 1] );
			i++;
			Sys_Printf( "Output file name set to %s\n", minimapFilename );
		}
		else if ( !strcmp( argv[ i ],  "-minmax" ) && i < ( argc - 7 ) ) {
			mins[0] = atof( argv[i + 1] );
			mins[1] = atof( argv[i + 2] );
			mins[2] = atof( argv[i + 3] );
			maxs[0] = atof( argv[i + 4] );
			maxs[1] = atof( argv[i + 5] );
			maxs[2] = atof( argv[i + 6] );
			i += 6;
			Sys_Printf( "Map mins/maxs overridden\n" );
		}
		else if ( !strcmp( argv[ i ],  "-gray" ) ) {
			mode = MINIMAP_MODE_GRAY;
			Sys_Printf( "Writing as white-on-black image\n" );
		}
		else if ( !strcmp( argv[ i ],  "-black" ) ) {
			mode = MINIMAP_MODE_BLACK;
			Sys_Printf( "Writing as black alpha image\n" );
		}
		else if ( !strcmp( argv[ i ],  "-white" ) ) {
			mode = MINIMAP_MODE_WHITE;
			Sys_Printf( "Writing as white alpha image\n" );
		}
		else if ( !strcmp( argv[ i ],  "-boost" ) && i < ( argc - 2 ) ) {
			minimap.boost = atof( argv[i + 1] );
			i++;
			Sys_Printf( "Contrast boost set to %f\n", minimap.boost );
		}
		else if ( !strcmp( argv[ i ],  "-brightness" ) && i < ( argc - 2 ) ) {
			minimap.brightness = atof( argv[i + 1] );
			i++;
			Sys_Printf( "Brightness set to %f\n", minimap.brightness );
		}
		else if ( !strcmp( argv[ i ],  "-contrast" ) && i < ( argc - 2 ) ) {
			minimap.contrast = atof( argv[i + 1] );
			i++;
			Sys_Printf( "Contrast set to %f\n", minimap.contrast );
		}
		else if ( !strcmp( argv[ i ],  "-autolevel" ) ) {
			autolevel = qtrue;
			Sys_Printf( "Auto level enabled\n", border );
		}
		else if ( !strcmp( argv[ i ],  "-noautolevel" ) ) {
			autolevel = qfalse;
			Sys_Printf( "Auto level disabled\n", border );
		}
	}

	MiniMapMakeMinsMaxs( mins, maxs, border, keepaspect );

	if ( !*minimapFilename ) {
		ExtractFileBase( source, basename );
		ExtractFilePath( source, path );
		sprintf( relativeMinimapFilename, game->miniMapNameFormat, basename );
		MergeRelativePath( minimapFilename, path, relativeMinimapFilename );
		Sys_Printf( "Output file name automatically set to %s\n", minimapFilename );
	}
	ExtractFilePath( minimapFilename, path );
	Q_mkdir( path );

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
		RunThreadsOnIndividual( minimap.height, qtrue, MiniMapNoSupersampling );
	}
	else
	{
		if ( minimap.sample_offsets ) {
			Sys_Printf( "\n--- MiniMapSupersampled (%d) ---\n", minimap.height );
			RunThreadsOnIndividual( minimap.height, qtrue, MiniMapSupersampled );
		}
		else
		{
			Sys_Printf( "\n--- MiniMapRandomlySupersampled (%d) ---\n", minimap.height );
			RunThreadsOnIndividual( minimap.height, qtrue, MiniMapRandomlySupersampled );
		}
	}

	if ( minimap.boost != 1.0 ) {
		Sys_Printf( "\n--- MiniMapContrastBoost (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, qtrue, MiniMapContrastBoost );
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
				if ( v < mi ) {
					mi = v;
				}
				if ( v > ma ) {
					ma = v;
				}
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
		RunThreadsOnIndividual( minimap.height, qtrue, MiniMapBrightnessContrast );
	}

	if ( minimap.sharpendata1f ) {
		Sys_Printf( "\n--- MiniMapSharpen (%d) ---\n", minimap.height );
		RunThreadsOnIndividual( minimap.height, qtrue, MiniMapSharpen );
		q = minimap.sharpendata1f;
	}
	else
	{
		q = minimap.data1f;
	}

	Sys_Printf( "\nConverting..." );

	switch ( mode )
	{
	case MINIMAP_MODE_GRAY:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				byte b;
				float v = *q++;
				if ( v < 0 ) {
					v = 0;
				}
				if ( v > 255.0 / 256.0 ) {
					v = 255.0 / 256.0;
				}
				b = v * 256;
				*p++ = b;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGAGray( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	case MINIMAP_MODE_BLACK:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				byte b;
				float v = *q++;
				if ( v < 0 ) {
					v = 0;
				}
				if ( v > 255.0 / 256.0 ) {
					v = 255.0 / 256.0;
				}
				b = v * 256;
				*p++ = 0;
				*p++ = 0;
				*p++ = 0;
				*p++ = b;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGA( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	case MINIMAP_MODE_WHITE:
		p = data4b;
		for ( y = 0; y < minimap.height; ++y )
			for ( x = 0; x < minimap.width; ++x )
			{
				byte b;
				float v = *q++;
				if ( v < 0 ) {
					v = 0;
				}
				if ( v > 255.0 / 256.0 ) {
					v = 255.0 / 256.0;
				}
				b = v * 256;
				*p++ = 255;
				*p++ = 255;
				*p++ = 255;
				*p++ = b;
			}
		Sys_Printf( " writing to %s...", minimapFilename );
		WriteTGA( minimapFilename, data4b, minimap.width, minimap.height );
		break;
	}

	Sys_Printf( " done.\n" );

	/* return to sender */
	return 0;
}





/*
   MD4BlockChecksum()
   calculates an md4 checksum for a block of data
 */

static int MD4BlockChecksum( void *buffer, int length ){
	return Com_BlockChecksum( buffer, length );
}

/*
   FixAAS()
   resets an aas checksum to match the given BSP
 */

int FixAAS( int argc, char **argv ){
	int length, checksum;
	void        *buffer;
	FILE        *file;
	char aas[ 1024 ], **ext;
	char        *exts[] =
	{
		".aas",
		"_b0.aas",
		"_b1.aas",
		NULL
	};


	/* arg checking */
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map -fixaas [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* note it */
	Sys_Printf( "--- FixAAS ---\n" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	length = LoadFile( source, &buffer );

	/* create bsp checksum */
	Sys_Printf( "Creating checksum...\n" );
	checksum = LittleLong( MD4BlockChecksum( buffer, length ) );

	/* write checksum to aas */
	ext = exts;
	while ( *ext )
	{
		/* mangle name */
		strcpy( aas, source );
		StripExtension( aas );
		strcat( aas, *ext );
		Sys_Printf( "Trying %s\n", aas );
		ext++;

		/* fix it */
		file = fopen( aas, "r+b" );
		if ( !file ) {
			continue;
		}
		if ( fwrite( &checksum, 4, 1, file ) != 1 ) {
			Error( "Error writing checksum to %s", aas );
		}
		fclose( file );
	}

	/* return to sender */
	return 0;
}



/*
   AnalyzeBSP() - ydnar
   analyzes a Quake engine BSP file
 */

typedef struct abspHeader_s
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ 1 ];       /* unknown size */
}
abspHeader_t;

typedef struct abspLumpTest_s
{
	int radix, minCount;
	char            *name;
}
abspLumpTest_t;

int AnalyzeBSP( int argc, char **argv ){
	abspHeader_t            *header;
	int size, i, version, offset, length, lumpInt, count;
	char ident[ 5 ];
	void                    *lump;
	float lumpFloat;
	char lumpString[ 1024 ], source[ 1024 ];
	qboolean lumpSwap = qfalse;
	abspLumpTest_t          *lumpTest;
	static abspLumpTest_t lumpTests[] =
	{
		{ sizeof( bspPlane_t ),         6,      "IBSP LUMP_PLANES" },
		{ sizeof( bspBrush_t ),         1,      "IBSP LUMP_BRUSHES" },
		{ 8,                            6,      "IBSP LUMP_BRUSHSIDES" },
		{ sizeof( bspBrushSide_t ),     6,      "RBSP LUMP_BRUSHSIDES" },
		{ sizeof( bspModel_t ),         1,      "IBSP LUMP_MODELS" },
		{ sizeof( bspNode_t ),          2,      "IBSP LUMP_NODES" },
		{ sizeof( bspLeaf_t ),          1,      "IBSP LUMP_LEAFS" },
		{ 104,                          3,      "IBSP LUMP_DRAWSURFS" },
		{ 44,                           3,      "IBSP LUMP_DRAWVERTS" },
		{ 4,                            6,      "IBSP LUMP_DRAWINDEXES" },
		{ 128 * 128 * 3,                1,      "IBSP LUMP_LIGHTMAPS" },
		{ 256 * 256 * 3,                1,      "IBSP LUMP_LIGHTMAPS (256 x 256)" },
		{ 512 * 512 * 3,                1,      "IBSP LUMP_LIGHTMAPS (512 x 512)" },
		{ 0, 0, NULL }
	};


	/* arg checking */
	if ( argc < 1 ) {
		Sys_Printf( "Usage: q3map -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( !strcmp( argv[ i ],  "-lumpswap" ) ) {
			Sys_Printf( "Swapped lump structs enabled\n" );
			lumpSwap = qtrue;
		}
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	Sys_Printf( "Loading %s\n", source );

	/* load the file */
	size = LoadFile( source, (void**) &header );
	if ( size == 0 || header == NULL ) {
		Sys_Printf( "Unable to load %s.\n", source );
		return -1;
	}

	/* analyze ident/version */
	memcpy( ident, header->ident, 4 );
	ident[ 4 ] = '\0';
	version = LittleLong( header->version );

	Sys_Printf( "Identity:      %s\n", ident );
	Sys_Printf( "Version:       %d\n", version );
	Sys_Printf( "---------------------------------------\n" );

	/* analyze each lump */
	for ( i = 0; i < 100; i++ )
	{
		/* call of duty swapped lump pairs */
		if ( lumpSwap ) {
			offset = LittleLong( header->lumps[ i ].length );
			length = LittleLong( header->lumps[ i ].offset );
		}

		/* standard lump pairs */
		else
		{
			offset = LittleLong( header->lumps[ i ].offset );
			length = LittleLong( header->lumps[ i ].length );
		}

		/* extract data */
		lump = (byte*) header + offset;
		lumpInt = LittleLong( (int) *( (int*) lump ) );
		lumpFloat = LittleFloat( (float) *( (float*) lump ) );
		memcpy( lumpString, (char*) lump, ( (size_t)length < sizeof( lumpString ) ? (size_t)length : sizeof( lumpString ) - 1 ) );
		lumpString[ sizeof( lumpString ) - 1 ] = '\0';

		/* print basic lump info */
		Sys_Printf( "Lump:          %d\n", i );
		Sys_Printf( "Offset:        %d bytes\n", offset );
		Sys_Printf( "Length:        %d bytes\n", length );

		/* only operate on valid lumps */
		if ( length > 0 ) {
			/* print data in 4 formats */
			Sys_Printf( "As hex:        %08X\n", lumpInt );
			Sys_Printf( "As int:        %d\n", lumpInt );
			Sys_Printf( "As float:      %f\n", lumpFloat );
			Sys_Printf( "As string:     %s\n", lumpString );

			/* guess lump type */
			if ( lumpString[ 0 ] == '{' && lumpString[ 2 ] == '"' ) {
				Sys_Printf( "Type guess:    IBSP LUMP_ENTITIES\n" );
			}
			else if ( strstr( lumpString, "textures/" ) ) {
				Sys_Printf( "Type guess:    IBSP LUMP_SHADERS\n" );
			}
			else
			{
				/* guess based on size/count */
				for ( lumpTest = lumpTests; lumpTest->radix > 0; lumpTest++ )
				{
					if ( ( length % lumpTest->radix ) != 0 ) {
						continue;
					}
					count = length / lumpTest->radix;
					if ( count < lumpTest->minCount ) {
						continue;
					}
					Sys_Printf( "Type guess:    %s (%d x %d)\n", lumpTest->name, count, lumpTest->radix );
				}
			}
		}

		Sys_Printf( "---------------------------------------\n" );

		/* end of file */
		if ( offset + length >= size ) {
			break;
		}
	}

	/* last stats */
	Sys_Printf( "Lump count:    %d\n", i + 1 );
	Sys_Printf( "File size:     %d bytes\n", size );

	/* return to caller */
	return 0;
}



/*
   BSPInfo()
   emits statistics about the bsp file
 */

int BSPInfo( int count, char **fileNames ){
	int i;
	char source[ 1024 ], ext[ 64 ];
	int size;
	FILE        *f;


	/* dummy check */
	if ( count < 1 ) {
		Sys_Printf( "No files to dump info for.\n" );
		return -1;
	}

	/* enable info mode */
	infoMode = qtrue;

	/* walk file list */
	for ( i = 0; i < count; i++ )
	{
		Sys_Printf( "---------------------------------\n" );

		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		ExtractFileExtension( source, ext );
		if ( !Q_stricmp( ext, "map" ) ) {
			StripExtension( source );
		}
		DefaultExtension( source, ".bsp" );
		f = fopen( source, "rb" );
		if ( f ) {
			size = Q_filelength( f );
			fclose( f );
		}
		else{
			size = 0;
		}

		/* load the bsp file and print lump sizes */
		Sys_Printf( "%s\n", source );
		LoadBSPFile( source );
		PrintBSPFileSizes();

		/* print sizes */
		Sys_Printf( "\n" );
		Sys_Printf( "          total         %9d\n", size );
		Sys_Printf( "                        %9d KB\n", size / 1024 );
		Sys_Printf( "                        %9d MB\n", size / ( 1024 * 1024 ) );

		Sys_Printf( "---------------------------------\n" );
	}

	/* return count */
	return i;
}


static void ExtrapolateTexcoords( const float *axyz, const float *ast, const float *bxyz, const float *bst, const float *cxyz, const float *cst, const float *axyz_new, float *ast_out, const float *bxyz_new, float *bst_out, const float *cxyz_new, float *cst_out ){
	vec4_t scoeffs, tcoeffs;
	float md;
	m4x4_t solvematrix;

	vec3_t norm;
	vec3_t dab, dac;
	VectorSubtract( bxyz, axyz, dab );
	VectorSubtract( cxyz, axyz, dac );
	CrossProduct( dab, dac, norm );

	// assume:
	//   s = f(x, y, z)
	//   s(v + norm) = s(v) when n ortho xyz

	// s(v) = DotProduct(v, scoeffs) + scoeffs[3]

	// solve:
	//   scoeffs * (axyz, 1) == ast[0]
	//   scoeffs * (bxyz, 1) == bst[0]
	//   scoeffs * (cxyz, 1) == cst[0]
	//   scoeffs * (norm, 0) == 0
	// scoeffs * [axyz, 1 | bxyz, 1 | cxyz, 1 | norm, 0] = [ast[0], bst[0], cst[0], 0]
	solvematrix[0] = axyz[0];
	solvematrix[4] = axyz[1];
	solvematrix[8] = axyz[2];
	solvematrix[12] = 1;
	solvematrix[1] = bxyz[0];
	solvematrix[5] = bxyz[1];
	solvematrix[9] = bxyz[2];
	solvematrix[13] = 1;
	solvematrix[2] = cxyz[0];
	solvematrix[6] = cxyz[1];
	solvematrix[10] = cxyz[2];
	solvematrix[14] = 1;
	solvematrix[3] = norm[0];
	solvematrix[7] = norm[1];
	solvematrix[11] = norm[2];
	solvematrix[15] = 0;

	md = m4_det( solvematrix );
	if ( md * md < 1e-10 ) {
		Sys_Printf( "Cannot invert some matrix, some texcoords aren't extrapolated!" );
		return;
	}

	m4x4_invert( solvematrix );

	scoeffs[0] = ast[0];
	scoeffs[1] = bst[0];
	scoeffs[2] = cst[0];
	scoeffs[3] = 0;
	m4x4_transform_vec4( solvematrix, scoeffs );
	tcoeffs[0] = ast[1];
	tcoeffs[1] = bst[1];
	tcoeffs[2] = cst[1];
	tcoeffs[3] = 0;
	m4x4_transform_vec4( solvematrix, tcoeffs );

	ast_out[0] = scoeffs[0] * axyz_new[0] + scoeffs[1] * axyz_new[1] + scoeffs[2] * axyz_new[2] + scoeffs[3];
	ast_out[1] = tcoeffs[0] * axyz_new[0] + tcoeffs[1] * axyz_new[1] + tcoeffs[2] * axyz_new[2] + tcoeffs[3];
	bst_out[0] = scoeffs[0] * bxyz_new[0] + scoeffs[1] * bxyz_new[1] + scoeffs[2] * bxyz_new[2] + scoeffs[3];
	bst_out[1] = tcoeffs[0] * bxyz_new[0] + tcoeffs[1] * bxyz_new[1] + tcoeffs[2] * bxyz_new[2] + tcoeffs[3];
	cst_out[0] = scoeffs[0] * cxyz_new[0] + scoeffs[1] * cxyz_new[1] + scoeffs[2] * cxyz_new[2] + scoeffs[3];
	cst_out[1] = tcoeffs[0] * cxyz_new[0] + tcoeffs[1] * cxyz_new[1] + tcoeffs[2] * cxyz_new[2] + tcoeffs[3];
}

/*
   ScaleBSPMain()
   amaze and confuse your enemies with wierd scaled maps!
 */

int ScaleBSPMain( int argc, char **argv ){
	int i, j;
	float f, a;
	vec3_t scale;
	vec3_t vec;
	char str[ 1024 ];
	int uniform, axis;
	qboolean texscale;
	float *old_xyzst = NULL;
	float spawn_ref = 0;


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		return 0;
	}

	texscale = qfalse;
	for ( i = 1; i < argc - 2; ++i )
	{
		if ( !strcmp( argv[i], "-tex" ) ) {
			texscale = qtrue;
		}
		else if ( !strcmp( argv[i], "-spawn_ref" ) ) {
			spawn_ref = atof( argv[i + 1] );
			++i;
		}
		else{
			break;
		}
	}

	/* get scale */
	// if(argc-2 >= i) // always true
	scale[2] = scale[1] = scale[0] = atof( argv[ argc - 2 ] );
	if ( argc - 3 >= i ) {
		scale[1] = scale[0] = atof( argv[ argc - 3 ] );
	}
	if ( argc - 4 >= i ) {
		scale[0] = atof( argv[ argc - 4 ] );
	}

	uniform = ( ( scale[0] == scale[1] ) && ( scale[1] == scale[2] ) );

	if ( scale[0] == 0.0f || scale[1] == 0.0f || scale[2] == 0.0f ) {
		Sys_Printf( "Usage: q3map [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		Sys_Printf( "Non-zero scale value required.\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();

	/* note it */
	Sys_Printf( "--- ScaleBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9d entities\n", numEntities );

	/* scale entity keys */
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* scale origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if ( ( vec[ 0 ] || vec[ 1 ] || vec[ 2 ] ) ) {
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] += spawn_ref;
			}
			vec[0] *= scale[0];
			vec[1] *= scale[1];
			vec[2] *= scale[2];
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] -= spawn_ref;
			}
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}

		a = FloatForKey( &entities[ i ], "angle" );
		if ( a == -1 || a == -2 ) { // z scale
			axis = 2;
		}
		else if ( fabs( sin( DEG2RAD( a ) ) ) < 0.707 ) {
			axis = 0;
		}
		else{
			axis = 1;
		}

		/* scale door lip */
		f = FloatForKey( &entities[ i ], "lip" );
		if ( f ) {
			f *= scale[axis];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "lip", str );
		}

		/* scale plat height */
		f = FloatForKey( &entities[ i ], "height" );
		if ( f ) {
			f *= scale[2];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "height", str );
		}

		// TODO maybe allow a definition file for entities to specify which values are scaled how?
	}

	/* scale models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] *= scale[0];
		bspModels[ i ].mins[1] *= scale[1];
		bspModels[ i ].mins[2] *= scale[2];
		bspModels[ i ].maxs[0] *= scale[0];
		bspModels[ i ].maxs[1] *= scale[1];
		bspModels[ i ].maxs[2] *= scale[2];
	}

	/* scale nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] *= scale[0];
		bspNodes[ i ].mins[1] *= scale[1];
		bspNodes[ i ].mins[2] *= scale[2];
		bspNodes[ i ].maxs[0] *= scale[0];
		bspNodes[ i ].maxs[1] *= scale[1];
		bspNodes[ i ].maxs[2] *= scale[2];
	}

	/* scale leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] *= scale[0];
		bspLeafs[ i ].mins[1] *= scale[1];
		bspLeafs[ i ].mins[2] *= scale[2];
		bspLeafs[ i ].maxs[0] *= scale[0];
		bspLeafs[ i ].maxs[1] *= scale[1];
		bspLeafs[ i ].maxs[2] *= scale[2];
	}

	if ( texscale ) {
		Sys_Printf( "Using texture unlocking (and probably breaking texture alignment a lot)\n" );
		old_xyzst = safe_malloc( sizeof( *old_xyzst ) * numBSPDrawVerts * 5 );
		for ( i = 0; i < numBSPDrawVerts; i++ )
		{
			old_xyzst[5 * i + 0] = bspDrawVerts[i].xyz[0];
			old_xyzst[5 * i + 1] = bspDrawVerts[i].xyz[1];
			old_xyzst[5 * i + 2] = bspDrawVerts[i].xyz[2];
			old_xyzst[5 * i + 3] = bspDrawVerts[i].st[0];
			old_xyzst[5 * i + 4] = bspDrawVerts[i].st[1];
		}
	}

	/* scale drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] *= scale[0];
		bspDrawVerts[i].xyz[1] *= scale[1];
		bspDrawVerts[i].xyz[2] *= scale[2];
		bspDrawVerts[i].normal[0] /= scale[0];
		bspDrawVerts[i].normal[1] /= scale[1];
		bspDrawVerts[i].normal[2] /= scale[2];
		VectorNormalize( bspDrawVerts[i].normal, bspDrawVerts[i].normal );
	}

	if ( texscale ) {
		for ( i = 0; i < numBSPDrawSurfaces; i++ )
		{
			switch ( bspDrawSurfaces[i].surfaceType )
			{
			case SURFACE_FACE:
			case SURFACE_META:
				if ( bspDrawSurfaces[i].numIndexes % 3 ) {
					Error( "Not a triangulation!" );
				}
				for ( j = bspDrawSurfaces[i].firstIndex; j < bspDrawSurfaces[i].firstIndex + bspDrawSurfaces[i].numIndexes; j += 3 )
				{
					int ia = bspDrawIndexes[j] + bspDrawSurfaces[i].firstVert, ib = bspDrawIndexes[j + 1] + bspDrawSurfaces[i].firstVert, ic = bspDrawIndexes[j + 2] + bspDrawSurfaces[i].firstVert;
					bspDrawVert_t *a = &bspDrawVerts[ia], *b = &bspDrawVerts[ib], *c = &bspDrawVerts[ic];
					float *oa = &old_xyzst[ia * 5], *ob = &old_xyzst[ib * 5], *oc = &old_xyzst[ic * 5];
					// extrapolate:
					//   a->xyz -> oa
					//   b->xyz -> ob
					//   c->xyz -> oc
					ExtrapolateTexcoords(
						&oa[0], &oa[3],
						&ob[0], &ob[3],
						&oc[0], &oc[3],
						a->xyz, a->st,
						b->xyz, b->st,
						c->xyz, c->st );
				}
				break;
			}
		}
	}

	/* scale planes */
	if ( uniform ) {
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].dist *= scale[0];
		}
	}
	else
	{
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].normal[0] /= scale[0];
			bspPlanes[ i ].normal[1] /= scale[1];
			bspPlanes[ i ].normal[2] /= scale[2];
			f = 1 / VectorLength( bspPlanes[i].normal );
			VectorScale( bspPlanes[i].normal, f, bspPlanes[i].normal );
			bspPlanes[ i ].dist *= f;
		}
	}

	/* scale gridsize */
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if ( ( vec[ 0 ] + vec[ 1 ] + vec[ 2 ] ) == 0.0f ) {
		VectorCopy( gridSize, vec );
	}
	vec[0] *= scale[0];
	vec[1] *= scale[1];
	vec[2] *= scale[2];
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* write the bsp */
	UnparseEntities();
	StripExtension( source );
	DefaultExtension( source, "_s.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}


/*
   ShiftBSPMain()
   shifts a map: for testing physics with huge coordinates
 */

int ShiftBSPMain( int argc, char **argv ){
	int i, j;
	vec3_t scale;
	vec3_t vec;
	char str[ 1024 ];
	float spawn_ref = 0;


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map [-v] -shift [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		return 0;
	}

	for ( i = 1; i < argc - 2; ++i )
	{
		if ( !strcmp( argv[i], "-tex" ) ) {
		}
		else if ( !strcmp( argv[i], "-spawn_ref" ) ) {
			spawn_ref = atof( argv[i + 1] );
			++i;
		}
		else{
			break;
		}
	}

	/* get shift */
	// if(argc-2 >= i) // always true
	scale[2] = scale[1] = scale[0] = atof( argv[ argc - 2 ] );
	if ( argc - 3 >= i ) {
		scale[1] = scale[0] = atof( argv[ argc - 3 ] );
	}
	if ( argc - 4 >= i ) {
		scale[0] = atof( argv[ argc - 4 ] );
	}


	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();

	/* note it */
	Sys_Printf( "--- ShiftBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9d entities\n", numEntities );

	/* shift entity keys */
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* shift origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if ( ( vec[ 0 ] || vec[ 1 ] || vec[ 2 ] ) ) {
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] += spawn_ref;
			}
			vec[0] += scale[0];
			vec[1] += scale[1];
			vec[2] += scale[2];
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] -= spawn_ref;
			}
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}

	}

	/* shift models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] += scale[0];
		bspModels[ i ].mins[1] += scale[1];
		bspModels[ i ].mins[2] += scale[2];
		bspModels[ i ].maxs[0] += scale[0];
		bspModels[ i ].maxs[1] += scale[1];
		bspModels[ i ].maxs[2] += scale[2];
	}

	/* shift nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] += scale[0];
		bspNodes[ i ].mins[1] += scale[1];
		bspNodes[ i ].mins[2] += scale[2];
		bspNodes[ i ].maxs[0] += scale[0];
		bspNodes[ i ].maxs[1] += scale[1];
		bspNodes[ i ].maxs[2] += scale[2];
	}

	/* shift leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] += scale[0];
		bspLeafs[ i ].mins[1] += scale[1];
		bspLeafs[ i ].mins[2] += scale[2];
		bspLeafs[ i ].maxs[0] += scale[0];
		bspLeafs[ i ].maxs[1] += scale[1];
		bspLeafs[ i ].maxs[2] += scale[2];
	}

	/* shift drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] += scale[0];
		bspDrawVerts[i].xyz[1] += scale[1];
		bspDrawVerts[i].xyz[2] += scale[2];
	}

	/* shift planes */

	vec3_t point;

	for ( i = 0; i < numBSPPlanes; i++ )
	{
		//find point on plane
		for ( j=0; j<3; j++ ){
			if ( fabs( bspPlanes[ i ].normal[j] ) > 0.5 ){
				point[j] = bspPlanes[ i ].dist / bspPlanes[ i ].normal[j];
				point[(j+1)%3] = point[(j+2)%3] = 0;
				break;
			}
		}
		//shift point
		for ( j=0; j<3; j++ ){
			point[j] += scale[j];
		}
		//calc new plane dist
		bspPlanes[ i ].dist = DotProduct( point, bspPlanes[ i ].normal );
	}

	/* scale gridsize */
	/*
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if ( ( vec[ 0 ] + vec[ 1 ] + vec[ 2 ] ) == 0.0f ) {
		VectorCopy( gridSize, vec );
	}
	vec[0] *= scale[0];
	vec[1] *= scale[1];
	vec[2] *= scale[2];
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );
*/
	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* write the bsp */
	UnparseEntities();
	StripExtension( source );
	DefaultExtension( source, "_sh.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}


void FixDOSName( char *src ){
	if ( src == NULL ) {
		return;
	}

	while ( *src )
	{
		if ( *src == '\\' ) {
			*src = '/';
		}
		src++;
	}
}

/*
	Check if newcoming texture is unique and not excluded
*/
void tex2list( char* texlist, int *texnum, char* EXtex, int *EXtexnum ){
	int i;
	if ( token[0] == '\0') return;
	StripExtension( token );
	FixDOSName( token );
	for ( i = 0; i < *texnum; i++ ){
		if ( !Q_stricmp( texlist + i*65, token ) ) return;
	}
	for ( i = 0; i < *EXtexnum; i++ ){
		if ( !Q_stricmp( EXtex + i*65, token ) ) return;
	}
	strcpy ( texlist + (*texnum)*65, token );
	(*texnum)++;
	return;
}

/* 4 repack */
void tex2list2( char* texlist, int *texnum, char* EXtex, int *EXtexnum, char* rEXtex, int *rEXtexnum ){
	int i;
	if ( token[0] == '\0') return;
	//StripExtension( token );
	char* dot = strrchr( token, '.' );
	if ( dot != NULL){
		if ( Q_stricmp( dot, ".tga" ) && Q_stricmp( dot, ".jpg" ) && Q_stricmp( dot, ".png" ) ){
			Sys_Printf( "WARNING4: %s : weird or missing extension in shader\n", token );
		}
		else{
			*dot = '\0';
		}
	}
	FixDOSName( token );
	strcpy ( texlist + (*texnum)*65, token );
	strcat( token, ".tga" );

	/* exclude */
	for ( i = 0; i < *texnum; i++ ){
		if ( !Q_stricmp( texlist + i*65, texlist + (*texnum)*65 ) ) return;
	}
	for ( i = 0; i < *EXtexnum; i++ ){
		if ( !Q_stricmp( EXtex + i*65, texlist + (*texnum)*65 ) ) return;
	}
	for ( i = 0; i < *rEXtexnum; i++ ){
		if ( !Q_stricmp( rEXtex + i*65, texlist + (*texnum)*65 ) ) return;
	}
	(*texnum)++;
	return;
}


/*
	Check if newcoming res is unique
*/
void res2list( char* data, int *num ){
	int i;
	if ( strlen( data + (*num)*65 ) > 64 ){
		Sys_Printf( "WARNING6: %s : path too long.\n", data + (*num)*65 );
	}
	while ( *( data + (*num)*65 ) == '\\' || *( data + (*num)*65 ) == '/' ){
		char* cut = data + (*num)*65 + 1;
		strcpy( data + (*num)*65, cut );
	}
	if ( *( data + (*num)*65 ) == '\0') return;
	for ( i = 0; i < *num; i++ ){
		if ( !Q_stricmp( data + i*65, data + (*num)*65 ) ) return;
	}
	(*num)++;
	return;
}

void parseEXblock ( char* data, int *num, const char *exName ){
	if ( !GetToken( qtrue ) || strcmp( token, "{" ) ) {
		Error( "ReadExclusionsFile: %s, line %d: { not found", exName, scriptline );
	}
	while ( 1 )
	{
		if ( !GetToken( qtrue ) ) {
			break;
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}
		if ( token[0] == '{' ) {
			Error( "ReadExclusionsFile: %s, line %d: brace, opening twice in a row.", exName, scriptline );
		}

		/* add to list */
		strcpy( data + (*num)*65, token );
		(*num)++;
	}
	return;
}

char q3map2path[1024];
/*
   pk3BSPMain()
   map autopackager, works for Q3 type of shaders and ents
 */

int pk3BSPMain( int argc, char **argv ){
	int i, j, len;
	qboolean dbg = qfalse, png = qfalse, packFAIL = qfalse;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ ){
		if ( !strcmp( argv[ i ],  "-dbg" ) ) {
			dbg = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-png" ) ) {
			png = qtrue;
		}
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();


	char packname[ 1024 ], packFailName[ 1024 ], base[ 1024 ], nameOFmap[ 1024 ], temp[ 1024 ];

	/* copy map name */
	strcpy( base, source );
	StripExtension( base );

	/* extract map name */
	len = strlen( base ) - 1;
	while ( len > 0 && base[ len ] != '/' && base[ len ] != '\\' )
		len--;
	strcpy( nameOFmap, &base[ len + 1 ] );


	qboolean drawsurfSHs[1024] = { qfalse };

	for ( i = 0; i < numBSPDrawSurfaces; i++ ){
		/* can't exclude nodraw patches here (they want shaders :0!) */
		//if ( !( bspDrawSurfaces[i].surfaceType == 2 && bspDrawSurfaces[i].numIndexes == 0 ) ) drawsurfSHs[bspDrawSurfaces[i].shaderNum] = qtrue;
		drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = qtrue;
		//Sys_Printf( "%s\n", bspShaders[bspDrawSurfaces[i].shaderNum].shader );
	}

	int pk3ShadersN = 0;
	char* pk3Shaders = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3SoundsN = 0;
	char* pk3Sounds = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3ShaderfilesN = 0;
	char* pk3Shaderfiles = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3TexturesN = 0;
	char* pk3Textures = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3VideosN = 0;
	char* pk3Videos = (char *)calloc( 1024*65, sizeof( char ) );



	for ( i = 0; i < numBSPShaders; i++ ){
		if ( drawsurfSHs[i] ){
			strcpy( pk3Shaders + pk3ShadersN*65, bspShaders[i].shader );
			res2list( pk3Shaders, &pk3ShadersN );
			//pk3ShadersN++;
			//Sys_Printf( "%s\n", bspShaders[i].shader );
		}
	}

	/* Ent keys */
	epair_t *ep;
	for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
	{
		if ( !Q_strncasecmp( ep->key, "vertexremapshader", 17 ) ) {
			sscanf( ep->value, "%*[^;] %*[;] %s", pk3Shaders + pk3ShadersN*65 );
			res2list( pk3Shaders, &pk3ShadersN );
		}
	}
	strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[0], "music" ) );
	if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' ){
		FixDOSName( pk3Sounds + pk3SoundsN*65 );
		DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
		res2list( pk3Sounds, &pk3SoundsN );
	}

	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[i], "noise" ) );
		if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' && *( pk3Sounds + pk3SoundsN*65 ) != '*' ){
			FixDOSName( pk3Sounds + pk3SoundsN*65 );
			DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
			res2list( pk3Sounds, &pk3SoundsN );
		}

		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "func_plat" ) ){
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_strt.wav");
			res2list( pk3Sounds, &pk3SoundsN );
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_end.wav");
			res2list( pk3Sounds, &pk3SoundsN );
		}
		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "target_push" ) ){
			if ( !(IntForKey( &entities[i], "spawnflags") & 1) ){
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/misc/windfly.wav");
				res2list( pk3Sounds, &pk3SoundsN );
			}
		}
		strcpy( pk3Shaders + pk3ShadersN*65, ValueForKey( &entities[i], "targetShaderNewName" ) );
		res2list( pk3Shaders, &pk3ShadersN );
	}

	//levelshot
	sprintf( pk3Shaders + pk3ShadersN*65, "levelshots/%s", nameOFmap );
	res2list( pk3Shaders, &pk3ShadersN );


	if( dbg ){
		Sys_Printf( "\n\tDrawsurface+ent calls....%i\n", pk3ShadersN );
		for ( i = 0; i < pk3ShadersN; i++ ){
			Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
		Sys_Printf( "\n\tSounds....%i\n", pk3SoundsN );
		for ( i = 0; i < pk3SoundsN; i++ ){
			Sys_Printf( "%s\n", pk3Sounds + i*65 );
		}
	}

	vfsListShaderFiles( pk3Shaderfiles, &pk3ShaderfilesN );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3ShaderfilesN );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
	}


	/* load exclusions file */
	int ExTexturesN = 0;
	char* ExTextures = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShadersN = 0;
	char* ExShaders = (char *)calloc( 4096*65, sizeof( char ) );
	int ExSoundsN = 0;
	char* ExSounds = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShaderfilesN = 0;
	char* ExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int ExVideosN = 0;
	char* ExVideos = (char *)calloc( 4096*65, sizeof( char ) );
	int ExPureTexturesN = 0;
	char* ExPureTextures = (char *)calloc( 4096*65, sizeof( char ) );

	char* ExReasonShader[4096] = { NULL };
	char* ExReasonShaderFile[4096] = { NULL };

	char exName[ 1024 ];
	byte *buffer;
	int size;

	strcpy( exName, q3map2path );
	char *cut = strrchr( exName, '\\' );
	char *cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Printf( "WARNING: Unable to load exclusions file.\n" );
		goto skipEXfile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, game->arg );
	strcat( exName, ".exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Printf( "WARNING: Unable to find exclusions file %s.\n", exName );
		goto skipEXfile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( ExTextures, &ExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( ExShaders, &ExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( ExShaderfiles, &ExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( ExSounds, &ExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( ExVideos, &ExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

	for ( i = 0; i < ExTexturesN; i++ ){
		for ( j = 0; j < ExShadersN; j++ ){
			if ( !Q_stricmp( ExTextures + i*65, ExShaders + j*65 ) ){
				break;
			}
		}
		if ( j == ExShadersN ){
			strcpy ( ExPureTextures + ExPureTexturesN*65, ExTextures + i*65 );
			ExPureTexturesN++;
		}
	}

skipEXfile:

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTexturesN );
		for ( i = 0; i < ExTexturesN; i++ ) Sys_Printf( "%s\n", ExTextures + i*65 );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTexturesN );
		for ( i = 0; i < ExPureTexturesN; i++ ) Sys_Printf( "%s\n", ExPureTextures + i*65 );
		Sys_Printf( "\n\tExShaders....%i\n", ExShadersN );
		for ( i = 0; i < ExShadersN; i++ ) Sys_Printf( "%s\n", ExShaders + i*65 );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfilesN );
		for ( i = 0; i < ExShaderfilesN; i++ ) Sys_Printf( "%s\n", ExShaderfiles + i*65 );
		Sys_Printf( "\n\tExSounds....%i\n", ExSoundsN );
		for ( i = 0; i < ExSoundsN; i++ ) Sys_Printf( "%s\n", ExSounds + i*65 );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideosN );
		for ( i = 0; i < ExVideosN; i++ ) Sys_Printf( "%s\n", ExVideos + i*65 );
	}

	/* can exclude pure textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < ExPureTexturesN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, ExPureTextures + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}

	//Parse Shader Files
	 /* hack */
	endofscript = qtrue;

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		qboolean wantShader = qfalse, wantShaderFile = qfalse, ShaderFileExcluded = qfalse;
		int shader;
		char* reasonShader = NULL;
		char* reasonShaderFile = NULL;

		/* load the shader */
		sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
		SilentLoadScriptFile( temp, 0 );
		if( dbg ) Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles + i*65 );

		/* do wanna le shader file? */
		for ( j = 0; j < ExShaderfilesN; j++ ){
			if ( !Q_stricmp( ExShaderfiles + j*65, pk3Shaderfiles + i*65 ) ){
				ShaderFileExcluded = qtrue;
				reasonShaderFile = ExShaderfiles + j*65;
				break;
			}
		}
		/* tokenize it */
		/* check if shader file has to be excluded */
		while ( !ShaderFileExcluded )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}

			/* does it contain restricted shaders/textures? */
			for ( j = 0; j < ExShadersN; j++ ){
				if ( !Q_stricmp( ExShaders + j*65, token ) ){
					ShaderFileExcluded = qtrue;
					reasonShader = ExShaders + j*65;
					break;
				}
			}
			if ( ShaderFileExcluded )
				break;
			for ( j = 0; j < ExPureTexturesN; j++ ){
				if ( !Q_stricmp( ExPureTextures + j*65, token ) ){
					ShaderFileExcluded = qtrue;
					reasonShader = ExPureTextures + j*65;
					break;
				}
			}
			if ( ShaderFileExcluded )
				break;

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}

			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					break;
				}
				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							break;
						}
					}
				}
			}
		}

		/* tokenize it again */
		SilentLoadScriptFile( temp, 0 );
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			/* do wanna le shader? */
			wantShader = qfalse;
			for ( j = 0; j < pk3ShadersN; j++ ){
				if ( !Q_stricmp( pk3Shaders + j*65, token) ){
					shader = j;
					wantShader = qtrue;
					break;
				}
			}

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}

			qboolean hasmap = qfalse;
			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					break;
				}


				/* -----------------------------------------------------------------
				shader stages (passes)
				----------------------------------------------------------------- */

				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							break;
						}
						if ( !strcmp( token, "{" ) ) {
							Sys_Printf( "WARNING9: %s : line %d : opening brace inside shader stage\n", temp, scriptline );
						}
						if ( !Q_stricmp( token, "mapComp" ) || !Q_stricmp( token, "mapNoComp" ) || !Q_stricmp( token, "animmapcomp" ) || !Q_stricmp( token, "animmapnocomp" ) ){
							Sys_Printf( "WARNING7: %s : line %d : unsupported '%s' map directive\n", temp, scriptline, token );
						}
						/* skip the shader */
						if ( !wantShader ) continue;

						/* digest any images */
						if ( !Q_stricmp( token, "map" ) ||
							!Q_stricmp( token, "clampMap" ) ) {
							hasmap = qtrue;
							/* get an image */
							GetToken( qfalse );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
							}
						}
						else if ( !Q_stricmp( token, "animMap" ) ||
							!Q_stricmp( token, "clampAnimMap" ) ) {
							hasmap = qtrue;
							GetToken( qfalse );// skip num
							while ( TokenAvailable() ){
								GetToken( qfalse );
								tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
							}
						}
						else if ( !Q_stricmp( token, "videoMap" ) ){
							hasmap = qtrue;
							GetToken( qfalse );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL && strchr( token, '\\' ) == NULL ){
								sprintf( temp, "video/%s", token );
								strcpy( token, temp );
							}
							FixDOSName( token );
							for ( j = 0; j < pk3VideosN; j++ ){
								if ( !Q_stricmp( pk3Videos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < ExVideosN; j++ ){
								if ( !Q_stricmp( ExVideos + j*65, token ) ){
									goto away;
								}
							}
							strcpy ( pk3Videos + pk3VideosN*65, token );
							pk3VideosN++;
							away:
							j = 0;
						}
					}
				}
				else if ( !Q_strncasecmp( token, "implicit", 8 ) ){
					Sys_Printf( "WARNING5: %s : line %d : unsupported %s shader\n", temp, scriptline, token );
				}
				/* skip the shader */
				else if ( !wantShader ) continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( !Q_stricmp( token, "surfaceparm" ) ) {
					GetToken( qfalse );
					if ( !Q_stricmp( token, "nodraw" ) ) {
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( !Q_stricmp( token, "skyParms" ) ) {
					hasmap = qtrue;
					/* get image base */
					GetToken( qfalse );

					/* ignore bogus paths */
					if ( Q_stricmp( token, "-" ) && Q_stricmp( token, "full" ) ) {
						strcpy ( temp, token );
						sprintf( token, "%s_up", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_dn", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_lf", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_rt", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_bk", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_ft", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
					}
					/* skip rest of line */
					GetToken( qfalse );
					GetToken( qfalse );
				}
				else if ( !Q_stricmp( token, "fogparms" ) ){
					hasmap = qtrue;
				}
			}

			//exclude shader
			if ( wantShader ){
				for ( j = 0; j < ExShadersN; j++ ){
					if ( !Q_stricmp( ExShaders + j*65, pk3Shaders + shader*65 ) ){
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
						break;
					}
				}
				if ( !hasmap ){
					wantShader = qfalse;
				}
				if ( wantShader ){
					if ( ShaderFileExcluded ){
						if ( reasonShaderFile != NULL ){
							ExReasonShaderFile[ shader ] = reasonShaderFile;
						}
						else{
							ExReasonShaderFile[ shader ] = ( char* ) calloc( 65, sizeof( char ) );
							strcpy( ExReasonShaderFile[ shader ], pk3Shaderfiles + i*65 );
						}
						ExReasonShader[ shader ] = reasonShader;
					}
					else{
						wantShaderFile = qtrue;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}
			}
		}
		if ( !wantShaderFile ){
			*( pk3Shaderfiles + i*65 ) = '\0';
		}
	}



/* exclude stuff */
//wanted shaders from excluded .shaders
	Sys_Printf( "\n" );
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' && ( ExReasonShader[i] != NULL || ExReasonShaderFile[i] != NULL ) ){
			Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
			packFAIL = qtrue;
			if ( ExReasonShader[i] != NULL ){
				Sys_Printf( "     reason: is located in %s,\n     containing restricted shader %s\n", ExReasonShaderFile[i], ExReasonShader[i] );
			}
			else{
				Sys_Printf( "     reason: is located in restricted %s\n", ExReasonShaderFile[i] );
			}
			*( pk3Shaders + i*65 ) = '\0';
		}
	}
//pure textures (shader ones are done)
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			FixDOSName( pk3Shaders + i*65 );
			for ( j = 0; j < pk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, pk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < ExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, ExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
		}
	}

//snds
	for ( i = 0; i < pk3SoundsN; i++ ){
		for ( j = 0; j < ExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, ExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
	}

	/* make a pack */
	sprintf( packname, "%s/%s_autopacked.pk3", EnginePath, nameOFmap );
	remove( packname );
	sprintf( packFailName, "%s/%s_FAILEDpack.pk3", EnginePath, nameOFmap );
	remove( packFailName );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3TexturesN; i++ ){
		if ( png ){
			sprintf( temp, "%s.png", pk3Textures + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
		}
		sprintf( temp, "%s.tga", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		sprintf( temp, "%s.jpg", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Textures + i*65 );
		packFAIL = qtrue;
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( png ){
				sprintf( temp, "%s.png", pk3Shaders + i*65 );
				if ( vfsPackFile( temp, packname, 10 ) ){
					Sys_Printf( "++%s\n", temp );
					continue;
				}
			}
			sprintf( temp, "%s.tga", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			sprintf( temp, "%s.jpg", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}

			if ( i == pk3ShadersN - 1 ){ //levelshot typically
				Sys_Printf( "  ~fail  %s\n", pk3Shaders + i*65 );
			}
			else{
				Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
				packFAIL = qtrue;
			}
		}
	}

	Sys_Printf( "\n\tShaizers....\n" );

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		if ( *( pk3Shaderfiles + i*65 ) != '\0' ){
			sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
			packFAIL = qtrue;
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3SoundsN; i++ ){
		if ( *( pk3Sounds + i*65 ) != '\0' ){
			if ( vfsPackFile( pk3Sounds + i*65, packname, 10 ) ){
				Sys_Printf( "++%s\n", pk3Sounds + i*65 );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Sounds + i*65 );
			packFAIL = qtrue;
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3VideosN; i++ ){
		if ( vfsPackFile( pk3Videos + i*65, packname, 10 ) ){
			Sys_Printf( "++%s\n", pk3Videos + i*65 );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Videos + i*65 );
		packFAIL = qtrue;
	}

	Sys_Printf( "\n\t.bsp and stuff\n" );

	sprintf( temp, "maps/%s.bsp", nameOFmap );
	//if ( vfsPackFile( temp, packname, 10 ) ){
	if ( vfsPackFile_Absolute_Path( source, temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  !FAIL! %s\n", temp );
		packFAIL = qtrue;
	}

	sprintf( temp, "maps/%s.aas", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

	sprintf( temp, "scripts/%s.arena", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

	sprintf( temp, "scripts/%s.defi", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

	if ( !packFAIL ){
	Sys_Printf( "\nSaved to %s\n", packname );
	}
	else{
		rename( packname, packFailName );
		Sys_Printf( "\nSaved to %s\n", packFailName );
	}
	/* return to sender */
	return 0;
}


/*
   repackBSPMain()
   repack multiple maps, strip out only required shaders
   works for Q3 type of shaders and ents
 */

int repackBSPMain( int argc, char **argv ){
	int i, j, len, compLevel = 0;
	qboolean dbg = qfalse, png = qfalse;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ ){
		if ( !strcmp( argv[ i ],  "-dbg" ) ) {
			dbg = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-png" ) ) {
			png = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-complevel" ) ) {
			compLevel = atoi( argv[ i + 1 ] );
			i++;
			if ( compLevel < -1 ) compLevel = -1;
			if ( compLevel > 10 ) compLevel = 10;
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

/* load exclusions file */
	int ExTexturesN = 0;
	char* ExTextures = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShadersN = 0;
	char* ExShaders = (char *)calloc( 4096*65, sizeof( char ) );
	int ExSoundsN = 0;
	char* ExSounds = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShaderfilesN = 0;
	char* ExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int ExVideosN = 0;
	char* ExVideos = (char *)calloc( 4096*65, sizeof( char ) );
	int ExPureTexturesN = 0;
	char* ExPureTextures = (char *)calloc( 4096*65, sizeof( char ) );


	char exName[ 1024 ];
	byte *buffer;
	int size;

	strcpy( exName, q3map2path );
	char *cut = strrchr( exName, '\\' );
	char *cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Printf( "WARNING: Unable to load exclusions file.\n" );
		goto skipEXfile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, game->arg );
	strcat( exName, ".exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Printf( "WARNING: Unable to find exclusions file %s.\n", exName );
		goto skipEXfile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( ExTextures, &ExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( ExShaders, &ExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( ExShaderfiles, &ExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( ExSounds, &ExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( ExVideos, &ExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

	for ( i = 0; i < ExTexturesN; i++ ){
		for ( j = 0; j < ExShadersN; j++ ){
			if ( !Q_stricmp( ExTextures + i*65, ExShaders + j*65 ) ){
				break;
			}
		}
		if ( j == ExShadersN ){
			strcpy ( ExPureTextures + ExPureTexturesN*65, ExTextures + i*65 );
			ExPureTexturesN++;
		}
	}

skipEXfile:

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTexturesN );
		for ( i = 0; i < ExTexturesN; i++ ) Sys_Printf( "%s\n", ExTextures + i*65 );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTexturesN );
		for ( i = 0; i < ExPureTexturesN; i++ ) Sys_Printf( "%s\n", ExPureTextures + i*65 );
		Sys_Printf( "\n\tExShaders....%i\n", ExShadersN );
		for ( i = 0; i < ExShadersN; i++ ) Sys_Printf( "%s\n", ExShaders + i*65 );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfilesN );
		for ( i = 0; i < ExShaderfilesN; i++ ) Sys_Printf( "%s\n", ExShaderfiles + i*65 );
		Sys_Printf( "\n\tExSounds....%i\n", ExSoundsN );
		for ( i = 0; i < ExSoundsN; i++ ) Sys_Printf( "%s\n", ExSounds + i*65 );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideosN );
		for ( i = 0; i < ExVideosN; i++ ) Sys_Printf( "%s\n", ExVideos + i*65 );
	}




/* load repack.exclude */
	int rExTexturesN = 0;
	char* rExTextures = (char *)calloc( 65536*65, sizeof( char ) );
	int rExShadersN = 0;
	char* rExShaders = (char *)calloc( 32768*65, sizeof( char ) );
	int rExSoundsN = 0;
	char* rExSounds = (char *)calloc( 8192*65, sizeof( char ) );
	int rExShaderfilesN = 0;
	char* rExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int rExVideosN = 0;
	char* rExVideos = (char *)calloc( 4096*65, sizeof( char ) );

	strcpy( exName, q3map2path );
	cut = strrchr( exName, '\\' );
	cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Printf( "WARNING: Unable to load repack exclusions file.\n" );
		goto skipEXrefile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, "repack.exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Printf( "WARNING: Unable to find repack exclusions file %s.\n", exName );
		goto skipEXrefile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( rExTextures, &rExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( rExShaders, &rExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( rExShaderfiles, &rExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( rExSounds, &rExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( rExVideos, &rExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

skipEXrefile:

	if( dbg ){
		Sys_Printf( "\n\trExTextures....%i\n", rExTexturesN );
		for ( i = 0; i < rExTexturesN; i++ ) Sys_Printf( "%s\n", rExTextures + i*65 );
		Sys_Printf( "\n\trExShaders....%i\n", rExShadersN );
		for ( i = 0; i < rExShadersN; i++ ) Sys_Printf( "%s\n", rExShaders + i*65 );
		Sys_Printf( "\n\trExShaderfiles....%i\n", rExShaderfilesN );
		for ( i = 0; i < rExShaderfilesN; i++ ) Sys_Printf( "%s\n", rExShaderfiles + i*65 );
		Sys_Printf( "\n\trExSounds....%i\n", rExSoundsN );
		for ( i = 0; i < rExSoundsN; i++ ) Sys_Printf( "%s\n", rExSounds + i*65 );
		Sys_Printf( "\n\trExVideos....%i\n", rExVideosN );
		for ( i = 0; i < rExVideosN; i++ ) Sys_Printf( "%s\n", rExVideos + i*65 );
	}




	int bspListN = 0;
	char* bspList = (char *)calloc( 8192*1024, sizeof( char ) );

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	if ( !Q_stricmp( strrchr( source, '.' ), ".bsp" ) ){
		strcpy( bspList, source );
		bspListN++;
	}
	else{
		/* load bsps paths list */
		Sys_Printf( "Loading %s\n", source );
		size = TryLoadFile( source, (void**) &buffer );
		if ( size <= 0 ) {
			Sys_Printf( "WARNING: Unable to open bsps paths list file %s.\n", source );
		}

		/* parse the file */
		ParseFromMemory( (char *) buffer, size );

		/* tokenize it */
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			strcpy( bspList + bspListN * 1024 , token );
			bspListN++;
		}

		/* free the buffer */
		free( buffer );
	}

	char packname[ 1024 ], nameOFrepack[ 1024 ], nameOFmap[ 1024 ], temp[ 1024 ];

	/* copy input file name */
	strcpy( temp, source );
	StripExtension( temp );

	/* extract input file name */
	len = strlen( temp ) - 1;
	while ( len > 0 && temp[ len ] != '/' && temp[ len ] != '\\' )
		len--;
	strcpy( nameOFrepack, &temp[ len + 1 ] );


/* load bsps */
	int pk3ShadersN = 0;
	char* pk3Shaders = (char *)calloc( 65536*65, sizeof( char ) );
	int pk3SoundsN = 0;
	char* pk3Sounds = (char *)calloc( 4096*65, sizeof( char ) );
	int pk3ShaderfilesN = 0;
	char* pk3Shaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int pk3TexturesN = 0;
	char* pk3Textures = (char *)calloc( 65536*65, sizeof( char ) );
	int pk3VideosN = 0;
	char* pk3Videos = (char *)calloc( 1024*65, sizeof( char ) );

	for( j = 0; j < bspListN; j++ ){

		int pk3SoundsNold = pk3SoundsN;
		int pk3ShadersNold = pk3ShadersN;

		strcpy( source, bspList + j*1024 );
		StripExtension( source );
		DefaultExtension( source, ".bsp" );

		/* load the bsp */
		Sys_Printf( "\nLoading %s\n", source );
		PartialLoadBSPFile( source );
		ParseEntities();

		/* copy map name */
		strcpy( temp, source );
		StripExtension( temp );

		/* extract map name */
		len = strlen( temp ) - 1;
		while ( len > 0 && temp[ len ] != '/' && temp[ len ] != '\\' )
			len--;
		strcpy( nameOFmap, &temp[ len + 1 ] );


		qboolean drawsurfSHs[1024] = { qfalse };

		for ( i = 0; i < numBSPDrawSurfaces; i++ ){
			drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = qtrue;
		}

		for ( i = 0; i < numBSPShaders; i++ ){
			if ( drawsurfSHs[i] ){
				strcpy( pk3Shaders + pk3ShadersN*65, bspShaders[i].shader );
				res2list( pk3Shaders, &pk3ShadersN );
			}
		}

		/* Ent keys */
		epair_t *ep;
		for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
		{
			if ( !Q_strncasecmp( ep->key, "vertexremapshader", 17 ) ) {
				sscanf( ep->value, "%*[^;] %*[;] %s", pk3Shaders + pk3ShadersN*65 );
				res2list( pk3Shaders, &pk3ShadersN );
			}
		}
		strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[0], "music" ) );
		if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' ){
			FixDOSName( pk3Sounds + pk3SoundsN*65 );
			DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
			res2list( pk3Sounds, &pk3SoundsN );
		}

		for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
		{
			strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[i], "noise" ) );
			if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' && *( pk3Sounds + pk3SoundsN*65 ) != '*' ){
				FixDOSName( pk3Sounds + pk3SoundsN*65 );
				DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
				res2list( pk3Sounds, &pk3SoundsN );
			}

			if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "func_plat" ) ){
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_strt.wav");
				res2list( pk3Sounds, &pk3SoundsN );
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_end.wav");
				res2list( pk3Sounds, &pk3SoundsN );
			}
			if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "target_push" ) ){
				if ( !(IntForKey( &entities[i], "spawnflags") & 1) ){
					strcpy( pk3Sounds + pk3SoundsN*65, "sound/misc/windfly.wav");
					res2list( pk3Sounds, &pk3SoundsN );
				}
			}
			strcpy( pk3Shaders + pk3ShadersN*65, ValueForKey( &entities[i], "targetShaderNewName" ) );
			res2list( pk3Shaders, &pk3ShadersN );
		}

		//levelshot
		sprintf( pk3Shaders + pk3ShadersN*65, "levelshots/%s", nameOFmap );
		res2list( pk3Shaders, &pk3ShadersN );



		Sys_Printf( "\n\t+Drawsurface+ent calls....%i\n", pk3ShadersN - pk3ShadersNold );
		for ( i = pk3ShadersNold; i < pk3ShadersN; i++ ){
			Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
		Sys_Printf( "\n\t+Sounds....%i\n", pk3SoundsN - pk3SoundsNold );
		for ( i = pk3SoundsNold; i < pk3SoundsN; i++ ){
			Sys_Printf( "%s\n", pk3Sounds + i*65 );
		}
		/* free bsp data */
/*
		if ( bspDrawVerts != 0 ) {
			free( bspDrawVerts );
			bspDrawVerts = NULL;
			//numBSPDrawVerts = 0;
			Sys_Printf( "freed BSPDrawVerts\n" );
		}
*/		if ( bspDrawSurfaces != 0 ) {
			free( bspDrawSurfaces );
			bspDrawSurfaces = NULL;
			//numBSPDrawSurfaces = 0;
			//Sys_Printf( "freed bspDrawSurfaces\n" );
		}
/*		if ( bspLightBytes != 0 ) {
			free( bspLightBytes );
			bspLightBytes = NULL;
			//numBSPLightBytes = 0;
			Sys_Printf( "freed BSPLightBytes\n" );
		}
		if ( bspGridPoints != 0 ) {
			free( bspGridPoints );
			bspGridPoints = NULL;
			//numBSPGridPoints = 0;
			Sys_Printf( "freed BSPGridPoints\n" );
		}
		if ( bspPlanes != 0 ) {
			free( bspPlanes );
			bspPlanes = NULL;
			Sys_Printf( "freed bspPlanes\n" );
			//numBSPPlanes = 0;
			//allocatedBSPPlanes = 0;
		}
		if ( bspBrushes != 0 ) {
			free( bspBrushes );
			bspBrushes = NULL;
			Sys_Printf( "freed bspBrushes\n" );
			//numBSPBrushes = 0;
			//allocatedBSPBrushes = 0;
		}
*/		if ( entities != 0 ) {
			epair_t *ep2free;
			for ( i = 0; i < numBSPEntities && i < numEntities; i++ ){
				ep = entities[i].epairs;
				while( ep != NULL){
					ep2free = ep;
					ep = ep->next;
					free( ep2free );
				}
			}
			free( entities );
			entities = NULL;
			//Sys_Printf( "freed entities\n" );
			numEntities = 0;
			numBSPEntities = 0;
			allocatedEntities = 0;
		}
/*		if ( bspModels != 0 ) {
			free( bspModels );
			bspModels = NULL;
			Sys_Printf( "freed bspModels\n" );
			//numBSPModels = 0;
			//allocatedBSPModels = 0;
		}
*/		if ( bspShaders != 0 ) {
			free( bspShaders );
			bspShaders = NULL;
			//Sys_Printf( "freed bspShaders\n" );
			//numBSPShaders = 0;
			//allocatedBSPShaders = 0;
		}
		if ( bspEntData != 0 ) {
			free( bspEntData );
			bspEntData = NULL;
			//Sys_Printf( "freed bspEntData\n" );
			//bspEntDataSize = 0;
			//allocatedBSPEntData = 0;
		}
/*		if ( bspNodes != 0 ) {
			free( bspNodes );
			bspNodes = NULL;
			Sys_Printf( "freed bspNodes\n" );
			//numBSPNodes = 0;
			//allocatedBSPNodes = 0;
		}
		if ( bspDrawIndexes != 0 ) {
			free( bspDrawIndexes );
			bspDrawIndexes = NULL;
			Sys_Printf( "freed bspDrawIndexes\n" );
			//numBSPDrawIndexes = 0;
			//allocatedBSPDrawIndexes = 0;
		}
		if ( bspLeafSurfaces != 0 ) {
			free( bspLeafSurfaces );
			bspLeafSurfaces = NULL;
			Sys_Printf( "freed bspLeafSurfaces\n" );
			//numBSPLeafSurfaces = 0;
			//allocatedBSPLeafSurfaces = 0;
		}
		if ( bspLeafBrushes != 0 ) {
			free( bspLeafBrushes );
			bspLeafBrushes = NULL;
			Sys_Printf( "freed bspLeafBrushes\n" );
			//numBSPLeafBrushes = 0;
			//allocatedBSPLeafBrushes = 0;
		}
		if ( bspBrushSides != 0 ) {
			free( bspBrushSides );
			bspBrushSides = NULL;
			Sys_Printf( "freed bspBrushSides\n" );
			numBSPBrushSides = 0;
			allocatedBSPBrushSides = 0;
		}
		if ( numBSPFogs != 0 ) {
			Sys_Printf( "freed numBSPFogs\n" );
			numBSPFogs = 0;
		}
		if ( numBSPAds != 0 ) {
			Sys_Printf( "freed numBSPAds\n" );
			numBSPAds = 0;
		}
		if ( numBSPLeafs != 0 ) {
			Sys_Printf( "freed numBSPLeafs\n" );
			numBSPLeafs = 0;
		}
		if ( numBSPVisBytes != 0 ) {
			Sys_Printf( "freed numBSPVisBytes\n" );
			numBSPVisBytes = 0;
		}
*/	}



	vfsListShaderFiles( pk3Shaderfiles, &pk3ShaderfilesN );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3ShaderfilesN );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
	}



	/* can exclude pure *base* textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < ExPureTexturesN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, ExPureTextures + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}
	/* can exclude repack.exclude shaders, assuming they got all their images */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < rExShadersN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, rExShaders + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}

	//Parse Shader Files
	Sys_Printf( "\t\nParsing shaders....\n\n" );
	char shaderText[ 8192 ];
	char* allShaders = (char *)calloc( 16777216, sizeof( char ) );
	 /* hack */
	endofscript = qtrue;

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		qboolean wantShader = qfalse;
		int shader;

		/* load the shader */
		sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
		if ( dbg ) Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles + i*65 );
		SilentLoadScriptFile( temp, 0 );

		/* tokenize it */
		while ( 1 )
		{
			int line = scriptline;
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			strcpy( shaderText, token );

			if ( strchr( token, '\\') != NULL  ){
				Sys_Printf( "WARNING1: %s : %s : shader name with backslash\n", pk3Shaderfiles + i*65, token );
			}

			/* do wanna le shader? */
			wantShader = qfalse;
			for ( j = 0; j < pk3ShadersN; j++ ){
				if ( !Q_stricmp( pk3Shaders + j*65, token) ){
					shader = j;
					wantShader = qtrue;
					break;
				}
			}
			if ( wantShader ){
				for ( j = 0; j < rExTexturesN ; j++ ){
					if ( !Q_stricmp( pk3Shaders + shader*65, rExTextures + j*65 ) ){
						Sys_Printf( "WARNING3: %s : about to include shader for excluded texture\n", pk3Shaders + shader*65 );
						break;
					}
				}
			}

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}
			strcat( shaderText, "\n{" );
			qboolean hasmap = qfalse;

			while ( 1 )
			{
				line = scriptline;
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					strcat( shaderText, "\n}\n\n" );
					break;
				}
				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					qboolean tokenready = qfalse;
					strcat( shaderText, "\n\t{" );
					while ( 1 )
					{
						/* detour of TokenAvailable() '~' */
						if ( tokenready ) tokenready = qfalse;
						else line = scriptline;
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							strcat( shaderText, "\n\t}" );
							break;
						}
						if ( !strcmp( token, "{" ) ) {
							strcat( shaderText, "\n\t{" );
							Sys_Printf( "WARNING9: %s : line %d : opening brace inside shader stage\n", temp, scriptline );
						}
						/* skip the shader */
						if ( !wantShader ) continue;

						/* digest any images */
						if ( !Q_stricmp( token, "map" ) ||
							!Q_stricmp( token, "clampMap" ) ) {
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;

							/* get an image */
							GetToken( qfalse );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
							}
							strcat( shaderText, " " );
							strcat( shaderText, token );
						}
						else if ( !Q_stricmp( token, "animMap" ) ||
							!Q_stricmp( token, "clampAnimMap" ) ) {
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;

							GetToken( qfalse );// skip num
							strcat( shaderText, " " );
							strcat( shaderText, token );
							while ( TokenAvailable() ){
								GetToken( qfalse );
								tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
								strcat( shaderText, " " );
								strcat( shaderText, token );
							}
							tokenready = qtrue;
						}
						else if ( !Q_stricmp( token, "videoMap" ) ){
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;
							GetToken( qfalse );
							strcat( shaderText, " " );
							strcat( shaderText, token );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL && strchr( token, '\\' ) == NULL ){
								sprintf( temp, "video/%s", token );
								strcpy( token, temp );
							}
							FixDOSName( token );
							for ( j = 0; j < pk3VideosN; j++ ){
								if ( !Q_stricmp( pk3Videos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < ExVideosN; j++ ){
								if ( !Q_stricmp( ExVideos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < rExVideosN; j++ ){
								if ( !Q_stricmp( rExVideos + j*65, token ) ){
									goto away;
								}
							}
							strcpy ( pk3Videos + pk3VideosN*65, token );
							pk3VideosN++;
							away:
							j = 0;
						}
						else if ( !Q_stricmp( token, "mapComp" ) || !Q_stricmp( token, "mapNoComp" ) || !Q_stricmp( token, "animmapcomp" ) || !Q_stricmp( token, "animmapnocomp" ) ){
							Sys_Printf( "WARNING7: %s : %s shader\n", pk3Shaders + shader*65, token );
							hasmap = qtrue;
							if ( line == scriptline ){
								strcat( shaderText, " " );
								strcat( shaderText, token );
							}
							else{
								strcat( shaderText, "\n\t\t" );
								strcat( shaderText, token );
							}
						}
						else if ( line == scriptline ){
							strcat( shaderText, " " );
							strcat( shaderText, token );
						}
						else{
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
						}
					}
				}
				/* skip the shader */
				else if ( !wantShader ) continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( !Q_stricmp( token, "surfaceparm" ) ) {
					strcat( shaderText, "\n\tsurfaceparm " );
					GetToken( qfalse );
					strcat( shaderText, token );
					if ( !Q_stricmp( token, "nodraw" ) ) {
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( !Q_stricmp( token, "skyParms" ) ) {
					strcat( shaderText, "\n\tskyParms " );
					hasmap = qtrue;
					/* get image base */
					GetToken( qfalse );
					strcat( shaderText, token );

					/* ignore bogus paths */
					if ( Q_stricmp( token, "-" ) && Q_stricmp( token, "full" ) ) {
						strcpy ( temp, token );
						sprintf( token, "%s_up", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_dn", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_lf", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_rt", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_bk", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_ft", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
					}
					/* skip rest of line */
					GetToken( qfalse );
					strcat( shaderText, " " );
					strcat( shaderText, token );
					GetToken( qfalse );
					strcat( shaderText, " " );
					strcat( shaderText, token );
				}
				else if ( !Q_strncasecmp( token, "implicit", 8 ) ){
					Sys_Printf( "WARNING5: %s : %s shader\n", pk3Shaders + shader*65, token );
					hasmap = qtrue;
					if ( line == scriptline ){
						strcat( shaderText, " " );
						strcat( shaderText, token );
					}
					else{
						strcat( shaderText, "\n\t" );
						strcat( shaderText, token );
					}
				}
				else if ( !Q_stricmp( token, "fogparms" ) ){
					hasmap = qtrue;
					if ( line == scriptline ){
						strcat( shaderText, " " );
						strcat( shaderText, token );
					}
					else{
						strcat( shaderText, "\n\t" );
						strcat( shaderText, token );
					}
				}
				else if ( line == scriptline ){
					strcat( shaderText, " " );
					strcat( shaderText, token );
				}
				else{
					strcat( shaderText, "\n\t" );
					strcat( shaderText, token );
				}
			}

			//exclude shader
			if ( wantShader ){
				for ( j = 0; j < ExShadersN; j++ ){
					if ( !Q_stricmp( ExShaders + j*65, pk3Shaders + shader*65 ) ){
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
						break;
					}
				}
				if ( wantShader && !hasmap ){
					Sys_Printf( "WARNING8: %s : shader has no known maps\n", pk3Shaders + shader*65 );
					wantShader = qfalse;
					*( pk3Shaders + shader*65 ) = '\0';
				}
				if ( wantShader ){
					strcat( allShaders, shaderText );
					*( pk3Shaders + shader*65 ) = '\0';
				}
			}
		}
	}
/* TODO: RTCW's mapComp, mapNoComp, animmapcomp, animmapnocomp; nocompress?; ET's implicitmap, implicitblend, implicitmask */


/* exclude stuff */

//pure textures (shader ones are done)
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( strchr( pk3Shaders + i*65, '\\') != NULL  ){
				Sys_Printf( "WARNING2: %s : bsp shader path with backslash\n", pk3Shaders + i*65 );
				FixDOSName( pk3Shaders + i*65 );
				//what if theres properly slashed one in the list?
				for ( j = 0; j < pk3ShadersN; j++ ){
					if ( !Q_stricmp( pk3Shaders + i*65, pk3Shaders + j*65 ) && (i != j) ){
						*( pk3Shaders + i*65 ) = '\0';
						break;
					}
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < pk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, pk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < ExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, ExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < rExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, rExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
		}
	}

//snds
	for ( i = 0; i < pk3SoundsN; i++ ){
		for ( j = 0; j < ExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, ExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
		if ( *( pk3Sounds + i*65 ) == '\0' ) continue;
		for ( j = 0; j < rExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, rExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
	}

	/* write shader */
	sprintf( temp, "%s/%s_strippedBYrepacker.shader", EnginePath, nameOFrepack );
	FILE *f;
	f = fopen( temp, "wb" );
	fwrite( allShaders, sizeof( char ), strlen( allShaders ), f );
	fclose( f );
	Sys_Printf( "Shaders saved to %s\n", temp );

	/* make a pack */
	sprintf( packname, "%s/%s_repacked.pk3", EnginePath, nameOFrepack );
	remove( packname );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3TexturesN; i++ ){
		if ( png ){
			sprintf( temp, "%s.png", pk3Textures + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
		}
		sprintf( temp, "%s.tga", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, compLevel ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		sprintf( temp, "%s.jpg", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, compLevel ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Textures + i*65 );
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( png ){
				sprintf( temp, "%s.png", pk3Shaders + i*65 );
				if ( vfsPackFile( temp, packname, compLevel ) ){
					Sys_Printf( "++%s\n", temp );
					continue;
				}
			}
			sprintf( temp, "%s.tga", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			sprintf( temp, "%s.jpg", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3SoundsN; i++ ){
		if ( *( pk3Sounds + i*65 ) != '\0' ){
			if ( vfsPackFile( pk3Sounds + i*65, packname, compLevel ) ){
				Sys_Printf( "++%s\n", pk3Sounds + i*65 );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Sounds + i*65 );
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3VideosN; i++ ){
		if ( vfsPackFile( pk3Videos + i*65, packname, compLevel ) ){
			Sys_Printf( "++%s\n", pk3Videos + i*65 );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Videos + i*65 );
	}

	Sys_Printf( "\nSaved to %s\n", packname );

	/* return to sender */
	return 0;
}



/*
   PseudoCompileBSP()
   a stripped down ProcessModels
 */
void PseudoCompileBSP( qboolean need_tree ){
	int models;
	char modelValue[10];
	entity_t *entity;
	face_t *faces;
	tree_t *tree;
	node_t *node;
	brush_t *brush;
	side_t *side;
	int i;

	SetDrawSurfacesBuffer();
	mapDrawSurfs = safe_malloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	memset( mapDrawSurfs, 0, sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;

	BeginBSPFile();
	models = 1;
	for ( mapEntityNum = 0; mapEntityNum < numEntities; mapEntityNum++ )
	{
		/* get entity */
		entity = &entities[ mapEntityNum ];
		if ( entity->brushes == NULL && entity->patches == NULL ) {
			continue;
		}

		if ( mapEntityNum != 0 ) {
			sprintf( modelValue, "*%d", models++ );
			SetKeyValue( entity, "model", modelValue );
		}

		/* process the model */
		Sys_FPrintf( SYS_VRB, "############### model %i ###############\n", numBSPModels );
		BeginModel();

		entity->firstDrawSurf = numMapDrawSurfs;

		ClearMetaTriangles();
		PatchMapDrawSurfs( entity );

		if ( mapEntityNum == 0 && need_tree ) {
			faces = MakeStructuralBSPFaceList( entities[0].brushes );
			tree = FaceBSP( faces );
			node = tree->headnode;
		}
		else
		{
			node = AllocNode();
			node->planenum = PLANENUM_LEAF;
			tree = AllocTree();
			tree->headnode = node;
		}

		/* a minimized ClipSidesIntoTree */
		for ( brush = entity->brushes; brush; brush = brush->next )
		{
			/* walk the brush sides */
			for ( i = 0; i < brush->numsides; i++ )
			{
				/* get side */
				side = &brush->sides[ i ];
				if ( side->winding == NULL ) {
					continue;
				}
				/* shader? */
				if ( side->shaderInfo == NULL ) {
					continue;
				}
				/* save this winding as a visible surface */
				DrawSurfaceForSide( entity, brush, side, side->winding );
			}
		}

		if ( meta ) {
			ClassifyEntitySurfaces( entity );
			MakeEntityDecals( entity );
			MakeEntityMetaTriangles( entity );
			SmoothMetaTriangles();
			MergeMetaTriangles();
		}
		FilterDrawsurfsIntoTree( entity, tree );

		FilterStructuralBrushesIntoTree( entity, tree );
		FilterDetailBrushesIntoTree( entity, tree );

		EmitBrushes( entity->brushes, &entity->firstBrush, &entity->numBrushes );
		EndModel( entity, node );
	}
	EndBSPFile( qfalse );
}

/*
   ConvertBSPMain()
   main argument processing function for bsp conversion
 */

int ConvertBSPMain( int argc, char **argv ){
	int i;
	int ( *convertFunc )( char * );
	game_t  *convertGame;
	char ext[1024];
	qboolean map_allowed, force_bsp, force_map;


	/* set default */
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	map_allowed = qfalse;
	force_bsp = qfalse;
	force_map = qfalse;

	/* arg checking */
	if ( argc < 1 ) {
		Sys_Printf( "Usage: q3map -convert [-format <ase|obj|map_bp|map>] [-shadersasbitmap|-lightmapsastexcoord|-deluxemapsastexcoord] [-readbsp|-readmap [-meta|-patchmeta]] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( !strcmp( argv[ i ],  "-format" ) ) {
			i++;
			if ( !Q_stricmp( argv[ i ], "ase" ) ) {
				convertFunc = ConvertBSPToASE;
				map_allowed = qfalse;
			}
			else if ( !Q_stricmp( argv[ i ], "obj" ) ) {
				convertFunc = ConvertBSPToOBJ;
				map_allowed = qfalse;
			}
			else if ( !Q_stricmp( argv[ i ], "map_bp" ) ) {
				convertFunc = ConvertBSPToMap_BP;
				map_allowed = qtrue;
			}
			else if ( !Q_stricmp( argv[ i ], "map" ) ) {
				convertFunc = ConvertBSPToMap;
				map_allowed = qtrue;
			}
			else
			{
				convertGame = GetGame( argv[ i ] );
				map_allowed = qfalse;
				if ( convertGame == NULL ) {
					Sys_Printf( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
				}
			}
		}
		else if ( !strcmp( argv[ i ],  "-ne" ) ) {
			normalEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
		}
		else if ( !strcmp( argv[ i ],  "-de" ) ) {
			distanceEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
		}
		else if ( !strcmp( argv[ i ],  "-shaderasbitmap" ) || !strcmp( argv[ i ],  "-shadersasbitmap" ) ) {
			shadersAsBitmap = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-lightmapastexcoord" ) || !strcmp( argv[ i ],  "-lightmapsastexcoord" ) ) {
			lightmapsAsTexcoord = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-deluxemapastexcoord" ) || !strcmp( argv[ i ],  "-deluxemapsastexcoord" ) ) {
			lightmapsAsTexcoord = qtrue;
			deluxemap = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-readbsp" ) ) {
			force_bsp = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-readmap" ) ) {
			force_map = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-meta" ) ) {
			meta = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-patchmeta" ) ) {
			meta = qtrue;
			patchMeta = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-fast" ) ) {
			fast = qtrue;
		}
	}

	LoadShaderInfo();

	/* clean up map name */
	strcpy( source, ExpandArg( argv[i] ) );
	ExtractFileExtension( source, ext );

	if ( !map_allowed && !force_map ) {
		force_bsp = qtrue;
	}

	if ( force_map || ( !force_bsp && !Q_stricmp( ext, "map" ) && map_allowed ) ) {
		if ( !map_allowed ) {
			Sys_Printf( "WARNING: the requested conversion should not be done from .map files. Compile a .bsp first.\n" );
		}
		StripExtension( source );
		DefaultExtension( source, ".map" );
		Sys_Printf( "Loading %s\n", source );
		LoadMapFile( source, qfalse, convertGame == NULL );
		PseudoCompileBSP( convertGame != NULL );
	}
	else
	{
		StripExtension( source );
		DefaultExtension( source, ".bsp" );
		Sys_Printf( "Loading %s\n", source );
		LoadBSPFile( source );
		ParseEntities();
	}

	/* bsp format convert? */
	if ( convertGame != NULL ) {
		/* set global game */
		game = convertGame;

		/* write bsp */
		StripExtension( source );
		DefaultExtension( source, "_c.bsp" );
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );

		/* return to sender */
		return 0;
	}

	/* normal convert */
	return convertFunc( source );
}



/*
   main()
   q3map mojo...
 */

int main( int argc, char **argv ){
	int i, r;
	double start, end;


	/* we want consistent 'randomness' */
	srand( 0 );

	/* start timer */
	start = I_FloatTime();

	/* this was changed to emit version number over the network */
	printf( Q3MAP_VERSION "\n" );

	/* set exit call */
	atexit( ExitQ3Map );

	/* read general options first */
	for ( i = 1; i < argc; i++ )
	{
		/* -connect */
		if ( !strcmp( argv[ i ], "-connect" ) ) {
			argv[ i ] = NULL;
			i++;
			Broadcast_Setup( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* verbose */
		else if ( !strcmp( argv[ i ], "-v" ) ) {
			if ( !verbose ) {
				verbose = qtrue;
				argv[ i ] = NULL;
			}
		}

		/* force */
		else if ( !strcmp( argv[ i ], "-force" ) ) {
			force = qtrue;
			argv[ i ] = NULL;
		}

		/* patch subdivisions */
		else if ( !strcmp( argv[ i ], "-subdivisions" ) ) {
			argv[ i ] = NULL;
			i++;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if ( patchSubdivisions <= 0 ) {
				patchSubdivisions = 1;
			}
		}

		/* threads */
		else if ( !strcmp( argv[ i ], "-threads" ) ) {
			argv[ i ] = NULL;
			i++;
			numthreads = atoi( argv[ i ] );
			argv[ i ] = NULL;
		}

		else if( !strcmp( argv[ i ], "-nocmdline" ) )
		{
			Sys_Printf( "noCmdLine\n" );
			nocmdline = qtrue;
			argv[ i ] = NULL;
		}

	}

	/* init model library */
	PicoInit();
	PicoSetMallocFunc( safe_malloc );
	PicoSetFreeFunc( free );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( free );

	/* set number of threads */
	ThreadSetDefault();

	/* generate sinusoid jitter table */
	for ( i = 0; i < MAX_JITTERS; i++ )
	{
		jitters[ i ] = sin( i * 139.54152147 );
		//%	Sys_Printf( "Jitter %4d: %f\n", i, jitters[ i ] );
	}

	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */

	Sys_Printf( "Q3Map         - v1.0r (c) 1999 Id Software Inc.\n" );
	Sys_Printf( "Q3Map (ydnar) - v" Q3MAP_VERSION "\n" );
	Sys_Printf( "NetRadiant    - v" RADIANT_VERSION " " __DATE__ " " __TIME__ "\n" );
	Sys_Printf( "%s\n", Q3MAP_MOTD );
	Sys_Printf( "%s\n", argv[0] );

	strcpy( q3map2path, argv[0] );//fuer autoPack func

	/* ydnar: new path initialization */
	InitPaths( &argc, argv );

	/* set game options */
	if ( !patchSubdivisions ) {
		patchSubdivisions = game->patchSubdivisions;
	}

	/* check if we have enough options left to attempt something */
	if ( argc < 2 ) {
		Error( "Usage: %s [general options] [options] mapfile", argv[ 0 ] );
	}

	/* fixaas */
	if ( !strcmp( argv[ 1 ], "-fixaas" ) ) {
		r = FixAAS( argc - 1, argv + 1 );
	}

	/* analyze */
	else if ( !strcmp( argv[ 1 ], "-analyze" ) ) {
		r = AnalyzeBSP( argc - 1, argv + 1 );
	}

	/* info */
	else if ( !strcmp( argv[ 1 ], "-info" ) ) {
		r = BSPInfo( argc - 2, argv + 2 );
	}

	/* vis */
	else if ( !strcmp( argv[ 1 ], "-vis" ) ) {
		r = VisMain( argc - 1, argv + 1 );
	}

	/* light */
	else if ( !strcmp( argv[ 1 ], "-light" ) ) {
		r = LightMain( argc - 1, argv + 1 );
	}

	/* vlight */
	else if ( !strcmp( argv[ 1 ], "-vlight" ) ) {
		Sys_Printf( "WARNING: VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";    /* eek a hack */
		r = LightMain( argc, argv );
	}

	/* ydnar: lightmap export */
	else if ( !strcmp( argv[ 1 ], "-export" ) ) {
		r = ExportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: lightmap import */
	else if ( !strcmp( argv[ 1 ], "-import" ) ) {
		r = ImportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp scaling */
	else if ( !strcmp( argv[ 1 ], "-scale" ) ) {
		r = ScaleBSPMain( argc - 1, argv + 1 );
	}

	/* bsp shifting */
	else if ( !strcmp( argv[ 1 ], "-shift" ) ) {
		r = ShiftBSPMain( argc - 1, argv + 1 );
	}

	/* autopacking */
	else if ( !strcmp( argv[ 1 ], "-pk3" ) ) {
		r = pk3BSPMain( argc - 1, argv + 1 );
	}

	/* repacker */
	else if ( !strcmp( argv[ 1 ], "-repack" ) ) {
		r = repackBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp conversion */
	else if ( !strcmp( argv[ 1 ], "-convert" ) ) {
		r = ConvertBSPMain( argc - 1, argv + 1 );
	}

	/* div0: minimap */
	else if ( !strcmp( argv[ 1 ], "-minimap" ) ) {
		r = MiniMapBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: otherwise create a bsp */
	else{
		r = BSPMain( argc, argv );
	}

	/* emit time */
	end = I_FloatTime();
	Sys_Printf( "%9.0f seconds elapsed\n", end - start );

	/* shut down connection */
	Broadcast_Shutdown();

	/* return any error code */
	return r;
}
