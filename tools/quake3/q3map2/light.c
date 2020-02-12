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



/* marker */
#define LIGHT_C



/* dependencies */
#include "q3map2.h"



/*
   CreateSunLight() - ydnar
   this creates a sun light
 */

static void CreateSunLight( sun_t *sun ){
	int i;
	float photons, d, angle, elevation, da, de;
	vec3_t direction;
	light_t     *light;


	/* dummy check */
	if ( sun == NULL ) {
		return;
	}

	/* fixup */
	if ( sun->numSamples < 1 ) {
		sun->numSamples = 1;
	}

	/* set photons */
	photons = sun->photons / sun->numSamples;

	/* create the right number of suns */
	for ( i = 0; i < sun->numSamples; i++ )
	{
		/* calculate sun direction */
		if ( i == 0 ) {
			VectorCopy( sun->direction, direction );
		}
		else
		{
			/*
			    sun->direction[ 0 ] = cos( angle ) * cos( elevation );
			    sun->direction[ 1 ] = sin( angle ) * cos( elevation );
			    sun->direction[ 2 ] = sin( elevation );

			    xz_dist   = sqrt( x*x + z*z )
			    latitude  = atan2( xz_dist, y ) * RADIANS
			    longitude = atan2( x,       z ) * RADIANS
			 */

			d = sqrt( sun->direction[ 0 ] * sun->direction[ 0 ] + sun->direction[ 1 ] * sun->direction[ 1 ] );
			angle = atan2( sun->direction[ 1 ], sun->direction[ 0 ] );
			elevation = atan2( sun->direction[ 2 ], d );

			/* jitter the angles (loop to keep random sample within sun->deviance steridians) */
			do
			{
				da = ( Random() * 2.0f - 1.0f ) * sun->deviance;
				de = ( Random() * 2.0f - 1.0f ) * sun->deviance;
			}
			while ( ( da * da + de * de ) > ( sun->deviance * sun->deviance ) );
			angle += da;
			elevation += de;

			/* debug code */
			//%	Sys_Printf( "%d: Angle: %3.4f Elevation: %3.3f\n", sun->numSamples, (angle / Q_PI * 180.0f), (elevation / Q_PI * 180.0f) );

			/* create new vector */
			direction[ 0 ] = cos( angle ) * cos( elevation );
			direction[ 1 ] = sin( angle ) * cos( elevation );
			direction[ 2 ] = sin( elevation );
		}

		/* create a light */
		numSunLights++;
		light = safe_calloc( sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* initialize the light */
		light->flags = LIGHT_SUN_DEFAULT;
		light->type = EMIT_SUN;
		light->fade = 1.0f;
		light->falloffTolerance = falloffTolerance;
		light->filterRadius = sun->filterRadius / sun->numSamples;
		light->style = noStyles ? LS_NORMAL : sun->style;

		/* set the light's position out to infinity */
		VectorMA( vec3_origin, ( MAX_WORLD_COORD * 8.0f ), direction, light->origin );    /* MAX_WORLD_COORD * 2.0f */

		/* set the facing to be the inverse of the sun direction */
		VectorScale( direction, -1.0, light->normal );
		light->dist = DotProduct( light->origin, light->normal );

		/* set color and photons */
		VectorCopy( sun->color, light->color );
		light->photons = photons * skyScale;
	}

	/* another sun? */
	if ( sun->next != NULL ) {
		CreateSunLight( sun->next );
	}
}



/*
   CreateSkyLights() - ydnar
   simulates sky light with multiple suns
 */

static void CreateSkyLights( vec3_t color, float value, int iterations, float filterRadius, int style ){
	int i, j, numSuns;
	int angleSteps, elevationSteps;
	float angle, elevation;
	float angleStep, elevationStep;
	sun_t sun;


	/* dummy check */
	if ( value <= 0.0f || iterations < 2 ) {
		return;
	}

	/* basic sun setup */
	VectorCopy( color, sun.color );
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;
	sun.next = NULL;

	/* setup */
	elevationSteps = iterations - 1;
	angleSteps = elevationSteps * 4;
	angle = 0.0f;
	elevationStep = DEG2RAD( 90.0f / iterations );  /* skip elevation 0 */
	angleStep = DEG2RAD( 360.0f / angleSteps );

	/* calc individual sun brightness */
	numSuns = angleSteps * elevationSteps + 1;
	sun.photons = value / numSuns;

	/* iterate elevation */
	elevation = elevationStep * 0.5f;
	angle = 0.0f;
	for ( i = 0, elevation = elevationStep * 0.5f; i < elevationSteps; i++ )
	{
		/* iterate angle */
		for ( j = 0; j < angleSteps; j++ )
		{
			/* create sun */
			sun.direction[ 0 ] = cos( angle ) * cos( elevation );
			sun.direction[ 1 ] = sin( angle ) * cos( elevation );
			sun.direction[ 2 ] = sin( elevation );
			CreateSunLight( &sun );

			/* move */
			angle += angleStep;
		}

		/* move */
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}

	/* create vertical sun */
	VectorSet( sun.direction, 0.0f, 0.0f, 1.0f );
	CreateSunLight( &sun );

	/* short circuit */
	return;
}



/*
   CreateEntityLights()
   creates lights from light entities
 */

void CreateEntityLights( void ){
	int i, j;
	light_t         *light, *light2;
	entity_t        *e, *e2;


	/* go throught entity list and find lights */
	for ( i = 0; i < numEntities; i++ )
	{
		/* get entity */
		e = &entities[ i ];
		/* ydnar: check for lightJunior */
		bool junior;
		if ( ent_class_is( e, "lightJunior" ) ) {
			junior = true;
		}
		else if ( ent_class_prefixed( e, "light" ) ) {
			junior = false;
		}
		else{
			continue;
		}

		/* lights with target names (and therefore styles) are only parsed from BSP */
		if ( !strEmpty( ValueForKey( e, "targetname" ) ) && i >= numBSPEntities ) {
			continue;
		}

		/* create a light */
		numPointLights++;
		light = safe_calloc( sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* handle spawnflags */
		const int spawnflags = IntForKey( e, "spawnflags" );

		int flags;
		/* ydnar: quake 3+ light behavior */
		if ( !wolfLight ) {
			/* set default flags */
			flags = LIGHT_Q3A_DEFAULT;

			/* linear attenuation? */
			if ( spawnflags & 1 ) {
				flags |= LIGHT_ATTEN_LINEAR;
				flags &= ~LIGHT_ATTEN_ANGLE;
			}

			/* no angle attenuate? */
			if ( spawnflags & 2 ) {
				flags &= ~LIGHT_ATTEN_ANGLE;
			}
		}

		/* ydnar: wolf light behavior */
		else
		{
			/* set default flags */
			flags = LIGHT_WOLF_DEFAULT;

			/* inverse distance squared attenuation? */
			if ( spawnflags & 1 ) {
				flags &= ~LIGHT_ATTEN_LINEAR;
				flags |= LIGHT_ATTEN_ANGLE;
			}

			/* angle attenuate? */
			if ( spawnflags & 2 ) {
				flags |= LIGHT_ATTEN_ANGLE;
			}
		}

		/* other flags (borrowed from wolf) */

		/* wolf dark light? */
		if ( ( spawnflags & 4 ) || ( spawnflags & 8 ) ) {
			flags |= LIGHT_DARK;
		}

		/* nogrid? */
		if ( spawnflags & 16 ) {
			flags &= ~LIGHT_GRID;
		}

		/* junior? */
		if ( junior ) {
			flags |= LIGHT_GRID;
			flags &= ~LIGHT_SURFACES;
		}

		/* vortex: unnormalized? */
		if ( spawnflags & 32 ) {
			flags |= LIGHT_UNNORMALIZED;
		}

		/* vortex: distance atten? */
		if ( spawnflags & 64 ) {
			flags |= LIGHT_ATTEN_DISTANCE;
		}

		/* store the flags */
		light->flags = flags;

		/* ydnar: set fade key (from wolf) */
		light->fade = 1.0f;
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			light->fade = FloatForKey( e, "fade" );
			if ( light->fade == 0.0f ) {
				light->fade = 1.0f;
			}
		}

		/* ydnar: set angle scaling (from vlight) */
		light->angleScale = FloatForKey( e, "_anglescale" );
		if ( light->angleScale != 0.0f ) {
			light->flags |= LIGHT_ATTEN_ANGLE;
		}

		/* set origin */
		GetVectorForKey( e, "origin", light->origin );
		ENT_READKV( &light->style, e, "_style", "style" );
		if ( light->style < LS_NORMAL || light->style >= LS_NONE ) {
			Error( "Invalid lightstyle (%d) on entity %d", light->style, i );
		}

		/* set light intensity */
		float intensity = 300.f;
		ENT_READKV( &intensity, e, "_light", "light" );
		if ( intensity == 0.0f ) {
			intensity = 300.0f;
		}

		{ /* ydnar: set light scale (sof2) */
			float scale;
			if( ENT_READKV( &scale, e, "scale" ) && scale != 0.f )
				intensity *= scale;
		}

		/* ydnar: get deviance and samples */
		float deviance = FloatForKey( e, "_deviance", "_deviation", "_jitter" );
		if ( deviance < 0.f )
			deviance = 0.f;
		int numSamples = IntForKey( e, "_samples" );
		if ( numSamples < 1 )
			numSamples = 1;

		intensity /= numSamples;

		{ /* ydnar: get filter radius */
			const float filterRadius = FloatForKey( e, "_filterradius", "_filteradius", "_filter" );
			light->filterRadius = filterRadius < 0.f? 0.f : filterRadius;
		}

		/* set light color */
		if ( ENT_READKV( &light->color, e, "_color" ) ) {
			if ( colorsRGB ) {
				light->color[0] = Image_LinearFloatFromsRGBFloat( light->color[0] );
				light->color[1] = Image_LinearFloatFromsRGBFloat( light->color[1] );
				light->color[2] = Image_LinearFloatFromsRGBFloat( light->color[2] );
			}
			if ( !( light->flags & LIGHT_UNNORMALIZED ) ) {
				ColorNormalize( light->color, light->color );
			}
		}
		else{
			VectorSet( light->color, 1.f, 1.f, 1.f );
		}


		if( !ENT_READKV( &light->extraDist, e, "_extradist" ) )
			light->extraDist = extraDist;

		light->photons = intensity;

		light->type = EMIT_POINT;

		/* set falloff threshold */
		light->falloffTolerance = falloffTolerance / numSamples;

		/* lights with a target will be spotlights */
		const char *target;
		if ( ENT_READKV( &target, e, "target" ) ) {
			/* get target */
			e2 = FindTargetEntity( target );
			if ( e2 == NULL ) {
				Sys_Warning( "light at (%i %i %i) has missing target\n",
							(int) light->origin[ 0 ], (int) light->origin[ 1 ], (int) light->origin[ 2 ] );
				light->photons *= pointScale;
			}
			else
			{
				/* not a point light */
				numPointLights--;
				numSpotLights++;

				/* make a spotlight */
				vec3_t dest;
				GetVectorForKey( e2, "origin", dest );
				VectorSubtract( dest, light->origin, light->normal );
				float dist = VectorNormalize( light->normal, light->normal );
				float radius = FloatForKey( e, "radius" );
				if ( !radius ) {
					radius = 64;
				}
				if ( !dist ) {
					dist = 64;
				}
				light->radiusByDist = ( radius + 16 ) / dist;
				light->type = EMIT_SPOT;

				/* ydnar: wolf mods: spotlights always use nonlinear + angle attenuation */
				light->flags &= ~LIGHT_ATTEN_LINEAR;
				light->flags |= LIGHT_ATTEN_ANGLE;
				light->fade = 1.0f;

				/* ydnar: is this a sun? */
				if ( BoolForKey( e, "_sun" ) ) {
					/* not a spot light */
					numSpotLights--;

					/* unlink this light */
					lights = light->next;

					/* make a sun */
					sun_t sun;
					VectorScale( light->normal, -1.0f, sun.direction );
					VectorCopy( light->color, sun.color );
					sun.photons = intensity;
					sun.deviance = deviance / 180.0f * Q_PI;
					sun.numSamples = numSamples;
					sun.style = noStyles ? LS_NORMAL : light->style;
					sun.next = NULL;

					/* make a sun light */
					CreateSunLight( &sun );

					/* free original light */
					free( light );
					light = NULL;

					/* skip the rest of this love story */
					continue;
				}
				else
				{
					light->photons *= spotScale;
				}
			}
		}
		else{
			light->photons *= pointScale;
		}

		/* jitter the light */
		for ( j = 1; j < numSamples; j++ )
		{
			/* create a light */
			light2 = safe_malloc( sizeof( *light ) );
			memcpy( light2, light, sizeof( *light ) );
			light2->next = lights;
			lights = light2;

			/* add to counts */
			if ( light->type == EMIT_SPOT ) {
				numSpotLights++;
			}
			else{
				numPointLights++;
			}

			/* jitter it */
			light2->origin[ 0 ] = light->origin[ 0 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2->origin[ 1 ] = light->origin[ 1 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2->origin[ 2 ] = light->origin[ 2 ] + ( Random() * 2.0f - 1.0f ) * deviance;
		}
	}
}



/*
   CreateSurfaceLights() - ydnar
   this hijacks the radiosity code to generate surface lights for first pass
 */

#define APPROX_BOUNCE   1.0f

void CreateSurfaceLights( void ){
	int i;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	shaderInfo_t        *si;
	light_t             *light;
	float subdivide;
	vec3_t origin;
	clipWork_t cw;


	/* get sun shader supressor */
	const bool nss = BoolForKey( &entities[ 0 ], "_noshadersun" );

	/* walk the list of surfaces */
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		/* get surface and other bits */
		ds = &bspDrawSurfaces[ i ];
		info = &surfaceInfos[ i ];
		si = info->si;

		/* sunlight? */
		if ( si->sun != NULL && !nss ) {
			Sys_FPrintf( SYS_VRB, "Sun: %s\n", si->shader );
			CreateSunLight( si->sun );
			si->sun = NULL; /* FIXME: leak! */
		}

		/* sky light? */
		if ( si->skyLightValue > 0.0f ) {
			Sys_FPrintf( SYS_VRB, "Sky: %s\n", si->shader );
			CreateSkyLights( si->color, si->skyLightValue, si->skyLightIterations, si->lightFilterRadius, si->lightStyle );
			si->skyLightValue = 0.0f;   /* FIXME: hack! */
		}

		/* try to early out */
		if ( si->value <= 0 ) {
			continue;
		}

		/* autosprite shaders become point lights */
		if ( si->autosprite ) {
			/* create an average xyz */
			VectorAdd( info->mins, info->maxs, origin );
			VectorScale( origin, 0.5f, origin );

			/* create a light */
			light = safe_calloc( sizeof( *light ) );
			light->next = lights;
			lights = light;

			/* set it up */
			light->flags = LIGHT_Q3A_DEFAULT;
			light->type = EMIT_POINT;
			light->photons = si->value * pointScale;
			light->fade = 1.0f;
			light->si = si;
			VectorCopy( origin, light->origin );
			VectorCopy( si->color, light->color );
			light->falloffTolerance = falloffTolerance;
			light->style = si->lightStyle;

			/* add to point light count and continue */
			numPointLights++;
			continue;
		}

		/* get subdivision amount */
		if ( si->lightSubdivide > 0 ) {
			subdivide = si->lightSubdivide;
		}
		else{
			subdivide = defaultLightSubdivide;
		}

		/* switch on type */
		switch ( ds->surfaceType )
		{
		case MST_PLANAR:
		case MST_TRIANGLE_SOUP:
			RadLightForTriangles( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;

		case MST_PATCH:
			RadLightForPatch( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;

		default:
			break;
		}
	}
}



/*
   SetEntityOrigins()
   find the offset values for inline models
 */

void SetEntityOrigins( void ){
	int i, j, k, f;
	entity_t            *e;
	const char          *key;
	int modelnum;
	bspModel_t          *dm;
	bspDrawSurface_t    *ds;


	/* ydnar: copy drawverts into private storage for nefarious purposes */
	yDrawVerts = safe_malloc( numBSPDrawVerts * sizeof( bspDrawVert_t ) );
	memcpy( yDrawVerts, bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVert_t ) );

	/* set the entity origins */
	for ( i = 0; i < numEntities; i++ )
	{
		/* get entity and model */
		e = &entities[ i ];
		key = ValueForKey( e, "model" );
		if ( key[ 0 ] != '*' ) {
			continue;
		}
		modelnum = atoi( key + 1 );
		dm = &bspModels[ modelnum ];

		/* get entity origin */
		vec3_t origin = { 0.f, 0.f, 0.f };
		if ( !ENT_READKV( &origin, e, "origin" ) ) {
			continue;
		}

		/* set origin for all surfaces for this model */
		for ( j = 0; j < dm->numBSPSurfaces; j++ )
		{
			/* get drawsurf */
			ds = &bspDrawSurfaces[ dm->firstBSPSurface + j ];

			/* set its verts */
			for ( k = 0; k < ds->numVerts; k++ )
			{
				f = ds->firstVert + k;
				VectorAdd( origin, bspDrawVerts[ f ].xyz, yDrawVerts[ f ].xyz );
			}
		}
	}
}



/*
   PointToPolygonFormFactor()
   calculates the area over a point/normal hemisphere a winding covers
   ydnar: fixme: there has to be a faster way to calculate this
   without the expensive per-vert sqrts and transcendental functions
   ydnar 2002-09-30: added -faster switch because only 19% deviance > 10%
   between this and the approximation
 */

#define ONE_OVER_2PI    0.159154942f    //% (1.0f / (2.0f * 3.141592657f))

float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w ){
	vec3_t triVector, triNormal;
	int i, j;
	vec3_t dirs[ MAX_POINTS_ON_WINDING ];
	float total;
	float dot, angle, facing;


	/* this is expensive */
	for ( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[ i ], point, dirs[ i ] );
		VectorFastNormalize( dirs[ i ], dirs[ i ] );
	}

	/* duplicate first vertex to avoid mod operation */
	VectorCopy( dirs[ 0 ], dirs[ i ] );

	/* calculcate relative area */
	total = 0.0f;
	for ( i = 0; i < w->numpoints; i++ )
	{
		/* get a triangle */
		j = i + 1;
		dot = DotProduct( dirs[ i ], dirs[ j ] );

		/* roundoff can cause slight creep, which gives an IND from acos */
		if ( dot > 1.0f ) {
			dot = 1.0f;
		}
		else if ( dot < -1.0f ) {
			dot = -1.0f;
		}

		/* get the angle */
		angle = acos( dot );

		CrossProduct( dirs[ i ], dirs[ j ], triVector );
		if ( VectorFastNormalize( triVector, triNormal ) < 0.0001f ) {
			continue;
		}

		facing = DotProduct( normal, triNormal );
		total += facing * angle;

		/* ydnar: this was throwing too many errors with radiosity + crappy maps. ignoring it. */
		if ( total > 6.3f || total < -6.3f ) {
			return 0.0f;
		}
	}

	/* now in the range of 0 to 1 over the entire incoming hemisphere */
	//%	total /= (2.0f * 3.141592657f);
	total *= ONE_OVER_2PI;
	return total;
}



/*
   LightContributionTosample()
   determines the amount of light reaching a sample (luxel or vertex) from a given light
 */

int LightContributionToSample( trace_t *trace ){
	light_t         *light;
	float angle;
	float add;
	float dist;
	float addDeluxe = 0.0f, addDeluxeBounceScale = 0.25f;
	bool angledDeluxe = true;
	float colorBrightness;
	bool doAddDeluxe = true;

	/* get light */
	light = trace->light;

	/* clear color */
	trace->forceSubsampling = 0.0f; /* to make sure */
	VectorClear( trace->color );
	VectorClear( trace->colorNoShadow );
	VectorClear( trace->directionContribution );

	colorBrightness = RGBTOGRAY( light->color ) * ( 1.0f / 255.0f );

	/* ydnar: early out */
	if ( !( light->flags & LIGHT_SURFACES ) || light->envelope <= 0.0f ) {
		return 0;
	}

	/* do some culling checks */
	if ( light->type != EMIT_SUN ) {
		/* MrE: if the light is behind the surface */
		if ( !trace->twoSided ) {
			if ( DotProduct( light->origin, trace->normal ) - DotProduct( trace->origin, trace->normal ) < 0.0f ) {
				return 0;
			}
		}

		/* ydnar: test pvs */
		if ( !ClusterVisible( trace->cluster, light->cluster ) ) {
			return 0;
		}
	}

	/* exact point to polygon form factor */
	if ( light->type == EMIT_AREA ) {
		float factor;
		float d;
		vec3_t pushedOrigin;

		/* project sample point into light plane */
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if ( d < 3.0f ) {
			/* sample point behind plane? */
			if ( !( light->flags & LIGHT_TWOSIDED ) && d < -1.0f ) {
				return 0;
			}

			/* sample plane coincident? */
			if ( d > -3.0f && DotProduct( trace->normal, light->normal ) > 0.9f ) {
				return 0;
			}
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emitter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			VectorMA( trace->origin, ( 8.0f - d ), light->normal, pushedOrigin );
		}
		else{
			VectorCopy( trace->origin, pushedOrigin );
		}

		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* ptpff approximation */
		if ( faster ) {
			/* angle attenuation */
			angle = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && angle < 0 ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* attenuate */
			angle *= -DotProduct( light->normal, trace->direction );
			if ( angle == 0.0f ) {
				return 0;
			}
			else if ( angle < 0.0f &&
					  ( trace->twoSided || ( light->flags & LIGHT_TWOSIDED ) ) ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* clamp the distance to prevent super hot spots */
			dist = sqrt( dist * dist + light->extraDist * light->extraDist );
			if ( dist < 16.0f ) {
				dist = 16.0f;
			}

			add = light->photons / ( dist * dist ) * angle;

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = light->photons / ( dist * dist ) * angle;
				}
				else{
					addDeluxe = light->photons / ( dist * dist );
				}
			}
		}
		else
		{
			/* calculate the contribution */
			factor = PointToPolygonFormFactor( pushedOrigin, trace->normal, light->w );
			if ( factor == 0.0f ) {
				return 0;
			}
			else if ( factor < 0.0f ) {
				/* twosided lighting */
				if ( trace->twoSided || ( light->flags & LIGHT_TWOSIDED ) ) {
					factor = -factor;

					/* push light origin to other side of the plane */
					VectorMA( light->origin, -2.0f, light->normal, trace->end );
					dist = SetupTrace( trace );
					if ( dist >= light->envelope ) {
						return 0;
					}

					/* no deluxemap contribution from "other side" light */
					doAddDeluxe = false;
				}
				else{
					return 0;
				}
			}

			/* also don't deluxe if the direction is on the wrong side */
			if ( DotProduct( trace->normal, trace->direction ) < 0 ) {
				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* ydnar: moved to here */
			add = factor * light->add;

			if ( deluxemap ) {
				addDeluxe = add;
			}
		}
	}

	/* point/spot lights */
	else if ( light->type == EMIT_POINT || light->type == EMIT_SPOT ) {
		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* angle attenuation */
		if ( light->flags & LIGHT_ATTEN_ANGLE ) {
			/* standard Lambert attenuation */
			float dot = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					if ( dot > 1.0f ) {
						dot = 1.0f;
					}
					dot = ( dot * 0.5f ) + 0.5f;
					dot *= dot;
				}
				else{
					dot = 0;
				}
			}

			angle = dot;
		}
		else{
			angle = 1.0f;
		}

		if ( light->angleScale != 0.0f ) {
			angle /= light->angleScale;
			if ( angle > 1.0f ) {
				angle = 1.0f;
			}
		}

		/* attenuate */
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			add = angle * light->photons * linearScale - ( dist * light->fade );
			if ( add < 0.0f ) {
				add = 0.0f;
			}

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = angle * light->photons * linearScale - ( dist * light->fade );
				}
				else{
					addDeluxe = light->photons * linearScale - ( dist * light->fade );
				}

				if ( addDeluxe < 0.0f ) {
					addDeluxe = 0.0f;
				}
			}
		}
		else
		{
			add = ( light->photons / ( dist * dist ) ) * angle;
			if ( add < 0.0f ) {
				add = 0.0f;
			}

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = ( light->photons / ( dist * dist ) ) * angle;
				}
				else{
					addDeluxe = ( light->photons / ( dist * dist ) );
				}
			}

			if ( addDeluxe < 0.0f ) {
				addDeluxe = 0.0f;
			}
		}

		/* handle spotlights */
		if ( light->type == EMIT_SPOT ) {
			float distByNormal, radiusAtDist, sampleRadius;
			vec3_t pointAtDist, distToSample;

			/* do cone calculation */
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return 0;
			}
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );

			/* outside the cone */
			if ( sampleRadius >= radiusAtDist ) {
				return 0;
			}

			/* attenuate */
			if ( sampleRadius > ( radiusAtDist - 32.0f ) ) {
				add *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
				if ( add < 0.0f ) {
					add = 0.0f;
				}

				addDeluxe *= ( ( radiusAtDist - sampleRadius ) / 32.0f );

				if ( addDeluxe < 0.0f ) {
					addDeluxe = 0.0f;
				}
			}
		}
	}

	/* ydnar: sunlight */
	else if ( light->type == EMIT_SUN ) {
		/* get origin and direction */
		VectorAdd( trace->origin, light->origin, trace->end );
		dist = SetupTrace( trace );

		/* angle attenuation */
		if ( light->flags & LIGHT_ATTEN_ANGLE ) {
			/* standard Lambert attenuation */
			float dot = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					if ( dot > 1.0f ) {
						dot = 1.0f;
					}
					dot = ( dot * 0.5f ) + 0.5f;
					dot *= dot;
				}
				else{
					dot = 0;
				}
			}

			angle = dot;
		}
		else{
			angle = 1.0f;
		}

		/* attenuate */
		add = light->photons * angle;

		if ( deluxemap ) {
			if ( angledDeluxe ) {
				addDeluxe = light->photons * angle;
			}
			else{
				addDeluxe = light->photons;
			}

			if ( addDeluxe < 0.0f ) {
				addDeluxe = 0.0f;
			}
		}

		if ( add <= 0.0f ) {
			return 0;
		}

		/* VorteX: set noShadow color */
		VectorScale( light->color, add, trace->colorNoShadow );

		addDeluxe *= colorBrightness;

		if ( bouncing ) {
			addDeluxe *= addDeluxeBounceScale;
			if ( addDeluxe < 0.00390625f ) {
				addDeluxe = 0.00390625f;
			}
		}

		VectorScale( trace->direction, addDeluxe, trace->directionContribution );

		/* setup trace */
		trace->testAll = true;
		VectorScale( light->color, add, trace->color );

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			trace->forceSubsampling *= add;
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				VectorClear( trace->color );
				VectorClear( trace->directionContribution );

				return -1;
			}
		}

		/* return to sender */
		return 1;
	}
	else{
		Error( "Light of undefined type!" );
	}

	/* VorteX: set noShadow color */
	VectorScale( light->color, add, trace->colorNoShadow );

	/* ydnar: changed to a variable number */
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LIGHT_FAST_ACTUAL ) ) ) {
		return 0;
	}

	addDeluxe *= colorBrightness;

	/* hack land: scale down the radiosity contribution to light directionality.
	   Deluxemaps fusion many light directions into one. In a rtl process all lights
	   would contribute individually to the bump map, so several light sources together
	   would make it more directional (example: a yellow and red lights received from
	   opposing sides would light one side in red and the other in blue, adding
	   the effect of 2 directions applied. In the deluxemapping case, this 2 lights would
	   neutralize each other making it look like having no direction.
	   Same thing happens with radiosity. In deluxemapping case the radiosity contribution
	   is modifying the direction applied from directional lights, making it go closer and closer
	   to the surface normal the bigger is the amount of radiosity received.
	   So, for preserving the directional lights contributions, we scale down the radiosity
	   contribution. It's a hack, but there's a reason behind it */
	if ( bouncing ) {
		addDeluxe *= addDeluxeBounceScale;
		/* better NOT increase it beyond the original value
		   if( addDeluxe < 0.00390625f )
		    addDeluxe = 0.00390625f;
		 */
	}

	if ( doAddDeluxe ) {
		VectorScale( trace->direction, addDeluxe, trace->directionContribution );
	}

	/* setup trace */
	trace->testAll = false;
	VectorScale( light->color, add, trace->color );

	/* raytrace */
	TraceLine( trace );
	trace->forceSubsampling *= add;
	if ( trace->passSolid || trace->opaque ) {
		VectorClear( trace->color );
		VectorClear( trace->directionContribution );

		return -1;
	}

	/* return to sender */
	return 1;
}



/*
   LightingAtSample()
   determines the amount of light reaching a sample (luxel or vertex)
 */

void LightingAtSample( trace_t *trace, byte styles[ MAX_LIGHTMAPS ], vec3_t colors[ MAX_LIGHTMAPS ] ){
	int i, lightmapNum;


	/* clear colors */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		VectorClear( colors[ lightmapNum ] );

	/* ydnar: normalmap */
	if ( normalmap ) {
		colors[ 0 ][ 0 ] = ( trace->normal[ 0 ] + 1.0f ) * 127.5f;
		colors[ 0 ][ 1 ] = ( trace->normal[ 1 ] + 1.0f ) * 127.5f;
		colors[ 0 ][ 2 ] = ( trace->normal[ 2 ] + 1.0f ) * 127.5f;
		return;
	}

	/* ydnar: don't bounce ambient all the time */
	if ( !bouncing ) {
		VectorCopy( ambientColor, colors[ 0 ] );
	}

	/* ydnar: trace to all the list of lights pre-stored in tw */
	for ( i = 0; i < trace->numLights && trace->lights[ i ] != NULL; i++ )
	{
		/* set light */
		trace->light = trace->lights[ i ];

		/* style check */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			if ( styles[ lightmapNum ] == trace->light->style ||
				 styles[ lightmapNum ] == LS_NONE ) {
				break;
			}
		}

		/* max of MAX_LIGHTMAPS (4) styles allowed to hit a sample */
		if ( lightmapNum >= MAX_LIGHTMAPS ) {
			continue;
		}

		/* sample light */
		LightContributionToSample( trace );
		if ( trace->color[ 0 ] == 0.0f && trace->color[ 1 ] == 0.0f && trace->color[ 2 ] == 0.0f ) {
			continue;
		}

		/* handle negative light */
		if ( trace->light->flags & LIGHT_NEGATIVE ) {
			VectorScale( trace->color, -1.0f, trace->color );
		}

		/* set style */
		styles[ lightmapNum ] = trace->light->style;

		/* add it */
		VectorAdd( colors[ lightmapNum ], trace->color, colors[ lightmapNum ] );

		/* cheap mode */
		if ( cheap &&
			 colors[ 0 ][ 0 ] >= 255.0f &&
			 colors[ 0 ][ 1 ] >= 255.0f &&
			 colors[ 0 ][ 2 ] >= 255.0f ) {
			break;
		}
	}
}



/*
   LightContributionToPoint()
   for a given light, how much light/color reaches a given point in space (with no facing)
   note: this is similar to LightContributionToSample() but optimized for omnidirectional sampling
 */

bool LightContributionToPoint( trace_t *trace ){
	light_t     *light;
	float add, dist;


	/* get light */
	light = trace->light;

	/* clear color */
	VectorClear( trace->color );

	/* ydnar: early out */
	if ( !( light->flags & LIGHT_GRID ) || light->envelope <= 0.0f ) {
		return false;
	}

	/* is this a sun? */
	if ( light->type != EMIT_SUN ) {
		/* sun only? */
		if ( sunOnly ) {
			return false;
		}

		/* test pvs */
		if ( !ClusterVisible( trace->cluster, light->cluster ) ) {
			return false;
		}
	}

	/* ydnar: check origin against light's pvs envelope */
	if ( trace->origin[ 0 ] > light->maxs[ 0 ] || trace->origin[ 0 ] < light->mins[ 0 ] ||
		 trace->origin[ 1 ] > light->maxs[ 1 ] || trace->origin[ 1 ] < light->mins[ 1 ] ||
		 trace->origin[ 2 ] > light->maxs[ 2 ] || trace->origin[ 2 ] < light->mins[ 2 ] ) {
		gridBoundsCulled++;
		return false;
	}

	/* set light origin */
	if ( light->type == EMIT_SUN ) {
		VectorAdd( trace->origin, light->origin, trace->end );
	}
	else{
		VectorCopy( light->origin, trace->end );
	}

	/* set direction */
	dist = SetupTrace( trace );

	/* test envelope */
	if ( dist > light->envelope ) {
		gridEnvelopeCulled++;
		return false;
	}

	/* ptpff approximation */
	if ( light->type == EMIT_AREA && faster ) {
		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* attenuate */
		add = light->photons / ( dist * dist );
	}

	/* exact point to polygon form factor */
	else if ( light->type == EMIT_AREA ) {
		float factor, d;
		vec3_t pushedOrigin;


		/* see if the point is behind the light */
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if ( !( light->flags & LIGHT_TWOSIDED ) && d < -1.0f ) {
			return false;
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emiter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			VectorMA( trace->origin, ( 8.0f - d ), light->normal, pushedOrigin );
		}
		else{
			VectorCopy( trace->origin, pushedOrigin );
		}

		/* calculate the contribution (ydnar 2002-10-21: [bug 642] bad normal calc) */
		factor = PointToPolygonFormFactor( pushedOrigin, trace->direction, light->w );
		if ( factor == 0.0f ) {
			return false;
		}
		else if ( factor < 0.0f ) {
			if ( light->flags & LIGHT_TWOSIDED ) {
				factor = -factor;
			}
			else{
				return false;
			}
		}

		/* ydnar: moved to here */
		add = factor * light->add;
	}

	/* point/spot lights */
	else if ( light->type == EMIT_POINT || light->type == EMIT_SPOT ) {
		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* attenuate */
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			add = light->photons * linearScale - ( dist * light->fade );
			if ( add < 0.0f ) {
				add = 0.0f;
			}
		}
		else{
			add = light->photons / ( dist * dist );
		}

		/* handle spotlights */
		if ( light->type == EMIT_SPOT ) {
			float distByNormal, radiusAtDist, sampleRadius;
			vec3_t pointAtDist, distToSample;


			/* do cone calculation */
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return false;
			}
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );

			/* outside the cone */
			if ( sampleRadius >= radiusAtDist ) {
				return false;
			}

			/* attenuate */
			if ( sampleRadius > ( radiusAtDist - 32.0f ) ) {
				add *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
			}
		}
	}

	/* ydnar: sunlight */
	else if ( light->type == EMIT_SUN ) {
		/* attenuate */
		add = light->photons;
		if ( add <= 0.0f ) {
			return false;
		}

		/* setup trace */
		trace->testAll = true;
		VectorScale( light->color, add, trace->color );

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				VectorClear( trace->color );
				return false;
			}
		}

		/* return to sender */
		return true;
	}

	/* unknown light type */
	else{
		return false;
	}

	/* ydnar: changed to a variable number */
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LIGHT_FAST_ACTUAL ) ) ) {
		return false;
	}

	/* setup trace */
	trace->testAll = false;
	VectorScale( light->color, add, trace->color );

	/* trace */
	TraceLine( trace );
	if ( trace->passSolid ) {
		VectorClear( trace->color );
		return false;
	}

	/* we have a valid sample */
	return true;
}



/*
   TraceGrid()
   grid samples are for quickly determining the lighting
   of dynamically placed entities in the world
 */

#define MAX_CONTRIBUTIONS   32768

typedef struct
{
	vec3_t dir;
	vec3_t color;
	vec3_t ambient;
	int style;
}
contribution_t;

void TraceGrid( int num ){
	int i, j, x, y, z, mod, numCon, numStyles;
	float d, step;
	vec3_t baseOrigin, cheapColor, color, thisdir;
	rawGridPoint_t          *gp;
	bspGridPoint_t          *bgp;
	contribution_t contributions[ MAX_CONTRIBUTIONS ];
	trace_t trace;

	/* get grid points */
	gp = &rawGridPoints[ num ];
	bgp = &bspGridPoints[ num ];

	/* get grid origin */
	mod = num;
	z = mod / ( gridBounds[ 0 ] * gridBounds[ 1 ] );
	mod -= z * ( gridBounds[ 0 ] * gridBounds[ 1 ] );
	y = mod / gridBounds[ 0 ];
	mod -= y * gridBounds[ 0 ];
	x = mod;

	trace.origin[ 0 ] = gridMins[ 0 ] + x * gridSize[ 0 ];
	trace.origin[ 1 ] = gridMins[ 1 ] + y * gridSize[ 1 ];
	trace.origin[ 2 ] = gridMins[ 2 ] + z * gridSize[ 2 ];

	/* set inhibit sphere */
	if ( gridSize[ 0 ] > gridSize[ 1 ] && gridSize[ 0 ] > gridSize[ 2 ] ) {
		trace.inhibitRadius = gridSize[ 0 ] * 0.5f;
	}
	else if ( gridSize[ 1 ] > gridSize[ 0 ] && gridSize[ 1 ] > gridSize[ 2 ] ) {
		trace.inhibitRadius = gridSize[ 1 ] * 0.5f;
	}
	else{
		trace.inhibitRadius = gridSize[ 2 ] * 0.5f;
	}

	/* find point cluster */
	trace.cluster = ClusterForPointExt( trace.origin, GRID_EPSILON );
	if ( trace.cluster < 0 ) {
		/* try to nudge the origin around to find a valid point */
		VectorCopy( trace.origin, baseOrigin );
		for ( step = 0; ( step += 0.005 ) <= 1.0; )
		{
			VectorCopy( baseOrigin, trace.origin );
			trace.origin[ 0 ] += step * ( Random() - 0.5 ) * gridSize[0];
			trace.origin[ 1 ] += step * ( Random() - 0.5 ) * gridSize[1];
			trace.origin[ 2 ] += step * ( Random() - 0.5 ) * gridSize[2];

			/* ydnar: changed to find cluster num */
			trace.cluster = ClusterForPointExt( trace.origin, VERTEX_EPSILON );
			if ( trace.cluster >= 0 ) {
				break;
			}
		}

		/* can't find a valid point at all */
		if ( step > 1.0 ) {
			return;
		}
	}

	/* setup trace */
	trace.testOcclusion = !noTrace;
	trace.forceSunlight = false;
	trace.recvShadows = WORLDSPAWN_RECV_SHADOWS;
	trace.numSurfaces = 0;
	trace.surfaces = NULL;
	trace.numLights = 0;
	trace.lights = NULL;

	/* clear */
	numCon = 0;
	VectorClear( cheapColor );

	/* trace to all the lights, find the major light direction, and divide the
	   total light between that along the direction and the remaining in the ambient */
	for ( trace.light = lights; trace.light != NULL; trace.light = trace.light->next )
	{
		float addSize;


		/* sample light */
		if ( !LightContributionToPoint( &trace ) ) {
			continue;
		}

		/* handle negative light */
		if ( trace.light->flags & LIGHT_NEGATIVE ) {
			VectorScale( trace.color, -1.0f, trace.color );
		}

		/* add a contribution */
		VectorCopy( trace.color, contributions[ numCon ].color );
		VectorCopy( trace.direction, contributions[ numCon ].dir );
		VectorClear( contributions[ numCon ].ambient );
		contributions[ numCon ].style = trace.light->style;
		numCon++;

		/* push average direction around */
		addSize = VectorLength( trace.color );
		VectorMA( gp->dir, addSize, trace.direction, gp->dir );

		/* stop after a while */
		if ( numCon >= ( MAX_CONTRIBUTIONS - 1 ) ) {
			break;
		}

		/* ydnar: cheap mode */
		VectorAdd( cheapColor, trace.color, cheapColor );
		if ( cheapgrid && cheapColor[ 0 ] >= 255.0f && cheapColor[ 1 ] >= 255.0f && cheapColor[ 2 ] >= 255.0f ) {
			break;
		}
	}

	/////// Floodlighting for point //////////////////
	//do our floodlight ambient occlusion loop, and add a single contribution based on the brightest dir
	if ( floodlighty ) {
		int k;
		float addSize, f;
		vec3_t dir = { 0, 0, 1 };
		float ambientFrac = 0.25f;

		trace.testOcclusion = true;
		trace.forceSunlight = false;
		trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
		trace.testAll = true;

		for ( k = 0; k < 2; k++ )
		{
			if ( k == 0 ) { // upper hemisphere
				trace.normal[0] = 0;
				trace.normal[1] = 0;
				trace.normal[2] = 1;
			}
			else //lower hemisphere
			{
				trace.normal[0] = 0;
				trace.normal[1] = 0;
				trace.normal[2] = -1;
			}

			f = FloodLightForSample( &trace, floodlightDistance, floodlight_lowquality );

			/* add a fraction as pure ambient, half as top-down direction */
			contributions[ numCon ].color[0] = floodlightRGB[0] * floodlightIntensity * f * ( 1.0f - ambientFrac );
			contributions[ numCon ].color[1] = floodlightRGB[1] * floodlightIntensity * f * ( 1.0f - ambientFrac );
			contributions[ numCon ].color[2] = floodlightRGB[2] * floodlightIntensity * f * ( 1.0f - ambientFrac );

			contributions[ numCon ].ambient[0] = floodlightRGB[0] * floodlightIntensity * f * ambientFrac;
			contributions[ numCon ].ambient[1] = floodlightRGB[1] * floodlightIntensity * f * ambientFrac;
			contributions[ numCon ].ambient[2] = floodlightRGB[2] * floodlightIntensity * f * ambientFrac;

			contributions[ numCon ].dir[0] = dir[0];
			contributions[ numCon ].dir[1] = dir[1];
			contributions[ numCon ].dir[2] = dir[2];

			contributions[ numCon ].style = 0;

			/* push average direction around */
			addSize = VectorLength( contributions[ numCon ].color );
			VectorMA( gp->dir, addSize, dir, gp->dir );

			numCon++;
		}
	}
	/////////////////////

	/* normalize to get primary light direction */
	VectorNormalize( gp->dir, thisdir );

	/* now that we have identified the primary light direction,
	   go back and separate all the light into directed and ambient */

	numStyles = 1;
	for ( i = 0; i < numCon; i++ )
	{
		/* get relative directed strength */
		d = DotProduct( contributions[ i ].dir, thisdir );
		/* we map 1 to gridDirectionality, and 0 to gridAmbientDirectionality */
		d = gridAmbientDirectionality + d * ( gridDirectionality - gridAmbientDirectionality );
		if ( d < 0.0f ) {
			d = 0.0f;
		}

		/* find appropriate style */
		for ( j = 0; j < numStyles; j++ )
		{
			if ( gp->styles[ j ] == contributions[ i ].style ) {
				break;
			}
		}

		/* style not found? */
		if ( j >= numStyles ) {
			/* add a new style */
			if ( numStyles < MAX_LIGHTMAPS ) {
				gp->styles[ numStyles ] = contributions[ i ].style;
				bgp->styles[ numStyles ] = contributions[ i ].style;
				numStyles++;
				//%	Sys_Printf( "(%d, %d) ", num, contributions[ i ].style );
			}

			/* fallback */
			else{
				j = 0;
			}
		}

		/* add the directed color */
		VectorMA( gp->directed[ j ], d, contributions[ i ].color, gp->directed[ j ] );

		/* ambient light will be at 1/4 the value of directed light */
		/* (ydnar: nuke this in favor of more dramatic lighting?) */
		/* (PM: how about actually making it work? d=1 when it got here for single lights/sun :P */
//		d = 0.25f;
		/* (Hobbes: always setting it to .25 is hardly any better) */
		d = 0.25f * ( 1.0f - d );
		VectorMA( gp->ambient[ j ], d, contributions[ i ].color, gp->ambient[ j ] );

		VectorAdd( gp->ambient[ j ], contributions[ i ].ambient, gp->ambient[ j ] );

/*
 * div0:
 * the total light average = ambient value + 0.25 * sum of all directional values
 * we can also get the total light average as 0.25 * the sum of all contributions
 *
 * 0.25 * sum(contribution_i) == ambient + 0.25 * sum(d_i contribution_i)
 *
 * THIS YIELDS:
 * ambient == 0.25 * sum((1 - d_i) contribution_i)
 *
 * So, 0.25f * (1.0f - d) IS RIGHT. If you want to tune it, tune d BEFORE.
 */
	}


	/* store off sample */
	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
#if 0
		/* do some fudging to keep the ambient from being too low (2003-07-05: 0.25 -> 0.125) */
		if ( !bouncing ) {
			VectorMA( gp->ambient[ i ], 0.125f, gp->directed[ i ], gp->ambient[ i ] );
		}
#endif

		/* set minimum light and copy off to bytes */
		VectorCopy( gp->ambient[ i ], color );
		for ( j = 0; j < 3; j++ )
			if ( color[ j ] < minGridLight[ j ] ) {
				color[ j ] = minGridLight[ j ];
			}

		/* vortex: apply gridscale and gridambientscale here */
		ColorToBytes( color, bgp->ambient[ i ], gridScale * gridAmbientScale );
		ColorToBytes( gp->directed[ i ], bgp->directed[ i ], gridScale );
	}

	/* debug code */
	#if 0
	//%	Sys_FPrintf( SYS_VRB, "%10d %10d %10d ", &gp->ambient[ 0 ][ 0 ], &gp->ambient[ 0 ][ 1 ], &gp->ambient[ 0 ][ 2 ] );
	Sys_FPrintf( SYS_VRB, "%9d Amb: (%03.1f %03.1f %03.1f) Dir: (%03.1f %03.1f %03.1f)\n",
				 num,
				 gp->ambient[ 0 ][ 0 ], gp->ambient[ 0 ][ 1 ], gp->ambient[ 0 ][ 2 ],
				 gp->directed[ 0 ][ 0 ], gp->directed[ 0 ][ 1 ], gp->directed[ 0 ][ 2 ] );
	#endif

	/* store direction */
	NormalToLatLong( thisdir, bgp->latLong );
}



/*
   SetupGrid()
   calculates the size of the lightgrid and allocates memory
 */

void SetupGrid( void ){
	int i, j;
	vec3_t maxs, oldGridSize;
	char temp[ 64 ];


	/* don't do this if not grid lighting */
	if ( noGridLighting ) {
		return;
	}

	/* ydnar: set grid size */
	ENT_READKV( &gridSize, &entities[ 0 ], "gridsize" );

	/* quantize it */
	VectorCopy( gridSize, oldGridSize );
	for ( i = 0; i < 3; i++ )
		gridSize[ i ] = gridSize[ i ] >= 8.0f ? floor( gridSize[ i ] ) : 8.0f;

	/* ydnar: increase gridSize until grid count is smaller than max allowed */
	numRawGridPoints = MAX_MAP_LIGHTGRID + 1;
	j = 0;
	while ( numRawGridPoints > MAX_MAP_LIGHTGRID )
	{
		/* get world bounds */
		for ( i = 0; i < 3; i++ )
		{
			gridMins[ i ] = gridSize[ i ] * ceil( bspModels[ 0 ].mins[ i ] / gridSize[ i ] );
			maxs[ i ] = gridSize[ i ] * floor( bspModels[ 0 ].maxs[ i ] / gridSize[ i ] );
			gridBounds[ i ] = ( maxs[ i ] - gridMins[ i ] ) / gridSize[ i ] + 1;
		}

		/* set grid size */
		numRawGridPoints = gridBounds[ 0 ] * gridBounds[ 1 ] * gridBounds[ 2 ];

		/* increase grid size a bit */
		if ( numRawGridPoints > MAX_MAP_LIGHTGRID ) {
			gridSize[ j++ % 3 ] += 16.0f;
		}
	}

	/* print it */
	Sys_Printf( "Grid size = { %1.0f, %1.0f, %1.0f }\n", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );

	/* different? */
	if ( !VectorCompare( gridSize, oldGridSize ) ) {
		sprintf( temp, "%.0f %.0f %.0f", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
		SetKeyValue( &entities[ 0 ], "gridsize", (const char*) temp );
		Sys_FPrintf( SYS_VRB, "Storing adjusted grid size\n" );
	}

	/* 2nd variable. fixme: is this silly? */
	numBSPGridPoints = numRawGridPoints;

	/* allocate lightgrid */
	rawGridPoints = safe_calloc( numRawGridPoints * sizeof( *rawGridPoints ) );

	free( bspGridPoints );
	bspGridPoints = safe_calloc( numBSPGridPoints * sizeof( *bspGridPoints ) );

	/* clear lightgrid */
	for ( i = 0; i < numRawGridPoints; i++ )
	{
		VectorCopy( ambientColor, rawGridPoints[ i ].ambient[ j ] );
		rawGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		bspGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		for ( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			rawGridPoints[ i ].styles[ j ] = LS_NONE;
			bspGridPoints[ i ].styles[ j ] = LS_NONE;
		}
	}

	/* note it */
	Sys_Printf( "%9d grid points\n", numRawGridPoints );
}



/*
   LightWorld()
   does what it says...
 */

void LightWorld( bool fastAllocate ){
	vec3_t color;
	float f;
	int b, bt;
	bool minVertex, minGrid;


	/* ydnar: smooth normals */
	if ( shade ) {
		Sys_Printf( "--- SmoothNormals ---\n" );
		SmoothNormals();
	}

	/* determine the number of grid points */
	Sys_Printf( "--- SetupGrid ---\n" );
	SetupGrid();

	/* find the optional minimum lighting values */
	GetVectorForKey( &entities[ 0 ], "_color", color );
	if ( colorsRGB ) {
		color[0] = Image_LinearFloatFromsRGBFloat( color[0] );
		color[1] = Image_LinearFloatFromsRGBFloat( color[1] );
		color[2] = Image_LinearFloatFromsRGBFloat( color[2] );
	}
	if ( VectorLength( color ) == 0.0f ) {
		VectorSet( color, 1.0, 1.0, 1.0 );
	}

	/* ambient */
	f = FloatForKey( &entities[ 0 ], "_ambient", "ambient" );
	VectorScale( color, f, ambientColor );

	/* minvertexlight */
	if ( ( minVertex = ENT_READKV( &f, &entities[ 0 ], "_minvertexlight" ) ) ) {
		VectorScale( color, f, minVertexLight );
	}

	/* mingridlight */
	if ( ( minGrid = ENT_READKV( &f, &entities[ 0 ], "_mingridlight" ) ) ) {
		VectorScale( color, f, minGridLight );
	}

	/* minlight */
	if ( ENT_READKV( &f, &entities[ 0 ], "_minlight" ) ) {
		VectorScale( color, f, minLight );
		if ( !minVertex )
			VectorScale( color, f, minVertexLight );
		if ( !minGrid )
			VectorScale( color, f, minGridLight );
	}

	/* maxlight */
	if ( ENT_READKV( &f, &entities[ 0 ], "_maxlight" ) ) {
		maxLight = f > 255? 255 : f < 0? 0 : f;
	}

	/* create world lights */
	Sys_FPrintf( SYS_VRB, "--- CreateLights ---\n" );
	CreateEntityLights();
	CreateSurfaceLights();
	Sys_Printf( "%9d point lights\n", numPointLights );
	Sys_Printf( "%9d spotlights\n", numSpotLights );
	Sys_Printf( "%9d diffuse (area) lights\n", numDiffuseLights );
	Sys_Printf( "%9d sun/sky lights\n", numSunLights );

	/* calculate lightgrid */
	if ( !noGridLighting ) {
		/* ydnar: set up light envelopes */
		SetupEnvelopes( true, fastgrid );

		Sys_Printf( "--- TraceGrid ---\n" );
		inGrid = true;
		RunThreadsOnIndividual( numRawGridPoints, true, TraceGrid );
		inGrid = false;
		Sys_Printf( "%d x %d x %d = %d grid\n",
					gridBounds[ 0 ], gridBounds[ 1 ], gridBounds[ 2 ], numBSPGridPoints );

		/* ydnar: emit statistics on light culling */
		Sys_FPrintf( SYS_VRB, "%9d grid points envelope culled\n", gridEnvelopeCulled );
		Sys_FPrintf( SYS_VRB, "%9d grid points bounds culled\n", gridBoundsCulled );
	}

	/* slight optimization to remove a sqrt */
	subdivideThreshold *= subdivideThreshold;

	/* map the world luxels */
	Sys_Printf( "--- MapRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, true, MapRawLightmap );
	Sys_Printf( "%9d luxels\n", numLuxels );
	Sys_Printf( "%9d luxels mapped\n", numLuxelsMapped );
	Sys_Printf( "%9d luxels occluded\n", numLuxelsOccluded );

	/* dirty them up */
	if ( dirty ) {
		Sys_Printf( "--- DirtyRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, true, DirtyRawLightmap );
	}

	/* floodlight pass */
	FloodlightRawLightmaps();

	/* ydnar: set up light envelopes */
	SetupEnvelopes( false, fast );

	/* light up my world */
	lightsPlaneCulled = 0;
	lightsEnvelopeCulled = 0;
	lightsBoundsCulled = 0;
	lightsClusterCulled = 0;

	Sys_Printf( "--- IlluminateRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, true, IlluminateRawLightmap );
	Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );

	StitchSurfaceLightmaps();

	Sys_Printf( "--- IlluminateVertexes ---\n" );
	RunThreadsOnIndividual( numBSPDrawSurfaces, true, IlluminateVertexes );
	Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

	/* ydnar: emit statistics on light culling */
	Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );

	/* radiosity */
	b = 1;
	bt = bounce;
	while ( bounce > 0 )
	{
		/* store off the bsp between bounces */
		StoreSurfaceLightmaps( fastAllocate );
		UnparseEntities();
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );

		/* note it */
		Sys_Printf( "\n--- Radiosity (bounce %d of %d) ---\n", b, bt );

		/* flag bouncing */
		bouncing = true;
		VectorClear( ambientColor );
		floodlighty = false;

		/* generate diffuse lights */
		RadFreeLights();
		RadCreateDiffuseLights();

		/* setup light envelopes */
		SetupEnvelopes( false, fastbounce );
		if ( numLights == 0 ) {
			Sys_Printf( "No diffuse light to calculate, ending radiosity.\n" );
			break;
		}

		/* add to lightgrid */
		if ( bouncegrid ) {
			gridEnvelopeCulled = 0;
			gridBoundsCulled = 0;

			Sys_Printf( "--- BounceGrid ---\n" );
			inGrid = true;
			RunThreadsOnIndividual( numRawGridPoints, true, TraceGrid );
			inGrid = false;
			Sys_FPrintf( SYS_VRB, "%9d grid points envelope culled\n", gridEnvelopeCulled );
			Sys_FPrintf( SYS_VRB, "%9d grid points bounds culled\n", gridBoundsCulled );
		}

		/* light up my world */
		lightsPlaneCulled = 0;
		lightsEnvelopeCulled = 0;
		lightsBoundsCulled = 0;
		lightsClusterCulled = 0;

		Sys_Printf( "--- IlluminateRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, true, IlluminateRawLightmap );
		Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );
		Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

		StitchSurfaceLightmaps();

		Sys_Printf( "--- IlluminateVertexes ---\n" );
		RunThreadsOnIndividual( numBSPDrawSurfaces, true, IlluminateVertexes );
		Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

		/* ydnar: emit statistics on light culling */
		Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );

		/* interate */
		bounce--;
		b++;
	}
}



/*
   LightMain()
   main routine for light processing
 */

int LightMain( int argc, char **argv ){
	int i;
	float f;
	int lightmapMergeSize = 0;
	bool lightSamplesInsist = false;
	bool fastAllocate = true;


	/* note it */
	Sys_Printf( "--- Light ---\n" );
	Sys_Printf( "--- ProcessGameSpecific ---\n" );

	/* set standard game flags */
	wolfLight = game->wolfLight;
	if ( wolfLight ) {
		Sys_Printf( " lightning model: wolf\n" );
	}
	else{
		Sys_Printf( " lightning model: quake3\n" );
	}

	lmCustomSize = game->lightmapSize;
	Sys_Printf( " lightmap size: %d x %d pixels\n", lmCustomSize, lmCustomSize );

	lightmapGamma = game->lightmapGamma;
	Sys_Printf( " lightning gamma: %f\n", lightmapGamma );

	lightmapsRGB = game->lightmapsRGB;
	if ( lightmapsRGB ) {
		Sys_Printf( " lightmap colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " lightmap colorspace: linear\n" );
	}

	texturesRGB = game->texturesRGB;
	if ( texturesRGB ) {
		Sys_Printf( " texture colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " texture colorspace: linear\n" );
	}

	colorsRGB = game->colorsRGB;
	if ( colorsRGB ) {
		Sys_Printf( " _color colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " _color colorspace: linear\n" );
	}

	lightmapCompensate = game->lightmapCompensate;
	Sys_Printf( " lightning compensation: %f\n", lightmapCompensate );

	lightmapExposure = game->lightmapExposure;
	Sys_Printf( " lightning exposure: %f\n", lightmapExposure );

	gridScale = game->gridScale;
	Sys_Printf( " lightgrid scale: %f\n", gridScale );

	gridAmbientScale = game->gridAmbientScale;
	Sys_Printf( " lightgrid ambient scale: %f\n", gridAmbientScale );

	lightAngleHL = game->lightAngleHL;
	if ( lightAngleHL ) {
		Sys_Printf( " half lambert light angle attenuation enabled \n" );
	}

	noStyles = game->noStyles;
	if ( noStyles ) {
		Sys_Printf( " shader lightstyles hack: disabled\n" );
	}
	else{
		Sys_Printf( " shader lightstyles hack: enabled\n" );
	}

	patchShadows = game->patchShadows;
	if ( patchShadows ) {
		Sys_Printf( " patch shadows: enabled\n" );
	}
	else{
		Sys_Printf( " patch shadows: disabled\n" );
	}

	deluxemap = game->deluxeMap;
	deluxemode = game->deluxeMode;
	if ( deluxemap ) {
		if ( deluxemode ) {
			Sys_Printf( " deluxemapping: enabled with tangentspace deluxemaps\n" );
		}
		else{
			Sys_Printf( " deluxemapping: enabled with modelspace deluxemaps\n" );
		}
	}
	else{
		Sys_Printf( " deluxemapping: disabled\n" );
	}

	Sys_Printf( "--- ProcessCommandLine ---\n" );

	/* process commandline arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* lightsource scaling */
		if ( strEqual( argv[ i ], "-point" ) || strEqual( argv[ i ], "-pointscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-spherical" ) || strEqual( argv[ i ], "-sphericalscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-spot" ) || strEqual( argv[ i ], "-spotscale" ) ) {
			f = atof( argv[ i + 1 ] );
			spotScale *= f;
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-area" ) || strEqual( argv[ i ], "-areascale" ) ) {
			f = atof( argv[ i + 1 ] );
			areaScale *= f;
			Sys_Printf( "Area (shader) light scaled by %f to %f\n", f, areaScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-sky" ) || strEqual( argv[ i ], "-skyscale" ) ) {
			f = atof( argv[ i + 1 ] );
			skyScale *= f;
			Sys_Printf( "Sky/sun light scaled by %f to %f\n", f, skyScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-vertexscale" ) ) {
			f = atof( argv[ i + 1 ] );
			vertexglobalscale *= f;
			Sys_Printf( "Vertexlight scaled by %f to %f\n", f, vertexglobalscale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-backsplash" ) && i < ( argc - 3 ) ) {
			f = atof( argv[ i + 1 ] );
			g_backsplashFractionScale = f;
			Sys_Printf( "Area lights backsplash fraction scaled by %f\n", f, g_backsplashFractionScale );
			f = atof( argv[ i + 2 ] );
			if ( f >= -900.0f ){
				g_backsplashDistance = f;
				Sys_Printf( "Area lights backsplash distance set globally to %f\n", f, g_backsplashDistance );
			}
			i+=2;
		}

		else if ( strEqual( argv[ i ], "-nolm" ) ) {
			nolm = true;
			Sys_Printf( "No lightmaps yo\n" );
		}

		else if ( strEqual( argv[ i ], "-bouncecolorratio" ) ) {
			f = atof( argv[ i + 1 ] );
			bounceColorRatio *= f;
			if ( bounceColorRatio > 1 ) {
				bounceColorRatio = 1;
			}
			if ( bounceColorRatio < 0 ) {
				bounceColorRatio = 0;
			}
			Sys_Printf( "Bounce color ratio set to %f\n", bounceColorRatio );
			i++;
		}

		else if ( strEqual( argv[ i ], "-bouncescale" ) ) {
			f = atof( argv[ i + 1 ] );
			bounceScale *= f;
			Sys_Printf( "Bounce (radiosity) light scaled by %f to %f\n", f, bounceScale );
			i++;
		}

		else if ( strEqual( argv[ i ], "-scale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			areaScale *= f;
			skyScale *= f;
			bounceScale *= f;
			Sys_Printf( "All light scaled by %f\n", f );
			i++;
		}

		else if ( strEqual( argv[ i ], "-gridscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid lightning scaled by %f\n", f );
			gridScale *= f;
			i++;
		}

		else if ( strEqual( argv[ i ], "-gridambientscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid ambient lightning scaled by %f\n", f );
			gridAmbientScale *= f;
			i++;
		}

		else if ( strEqual( argv[ i ], "-griddirectionality" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f > 1 ) {
				f = 1;
			}
			if ( f < gridAmbientDirectionality ) {
				gridAmbientDirectionality = f;
			}
			Sys_Printf( "Grid directionality is %f\n", f );
			gridDirectionality = f;
			i++;
		}

		else if ( strEqual( argv[ i ], "-gridambientdirectionality" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f < -1 ) {
				f = -1;
			}
			if ( f > gridDirectionality ) {
				gridDirectionality = f;
			}
			Sys_Printf( "Grid ambient directionality is %f\n", f );
			gridAmbientDirectionality = f;
			i++;
		}

		else if ( strEqual( argv[ i ], "-gamma" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapGamma = f;
			Sys_Printf( "Lighting gamma set to %f\n", lightmapGamma );
			i++;
		}

		else if ( strEqual( argv[ i ], "-sRGBlight" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
		}

		else if ( strEqual( argv[ i ], "-nosRGBlight" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
		}

		else if ( strEqual( argv[ i ], "-sRGBtex" ) ) {
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
		}

		else if ( strEqual( argv[ i ], "-nosRGBtex" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
		}

		else if ( strEqual( argv[ i ], "-sRGBcolor" ) ) {
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( strEqual( argv[ i ], "-nosRGBcolor" ) ) {
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( strEqual( argv[ i ], "-sRGB" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( strEqual( argv[ i ], "-nosRGB" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( strEqual( argv[ i ], "-exposure" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapExposure = f;
			Sys_Printf( "Lighting exposure set to %f\n", lightmapExposure );
			i++;
		}

		else if ( strEqual( argv[ i ], "-compensate" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f <= 0.0f ) {
				f = 1.0f;
			}
			lightmapCompensate = f;
			Sys_Printf( "Lighting compensation set to 1/%f\n", lightmapCompensate );
			i++;
		}

		/* Lightmaps brightness */
		else if( strEqual( argv[ i ], "-brightness" ) ){
			lightmapBrightness = atof( argv[ i + 1 ] );
			Sys_Printf( "Scaling lightmaps brightness by %f\n", lightmapBrightness );
			i++;
		}

		/* Lighting contrast */
		else if( strEqual( argv[ i ], "-contrast" ) ){
			f = atof( argv[ i + 1 ] );
			lightmapContrast = f > 255? 255 : f < -255? -255 : f;
			Sys_Printf( "Lighting contrast set to %f\n", lightmapContrast );
			i++;
			/* change to factor in range of 0 to 129.5 */
			lightmapContrast = ( 259 * ( lightmapContrast + 255 ) ) / ( 255 * ( 259 - lightmapContrast ) );
		}

		/* ydnar switches */
		else if ( strEqual( argv[ i ], "-bounce" ) ) {
			bounce = atoi( argv[ i + 1 ] );
			if ( bounce < 0 ) {
				bounce = 0;
			}
			else if ( bounce > 0 ) {
				Sys_Printf( "Radiosity enabled with %d bounce(s)\n", bounce );
			}
			i++;
		}

		else if ( strEqual( argv[ i ], "-supersample" ) || strEqual( argv[ i ], "-super" ) ) {
			superSample = atoi( argv[ i + 1 ] );
			if ( superSample < 1 ) {
				superSample = 1;
			}
			else if ( superSample > 1 ) {
				Sys_Printf( "Ordered-grid supersampling enabled with %d sample(s) per lightmap texel\n", ( superSample * superSample ) );
			}
			i++;
		}

		else if ( strEqual( argv[ i ], "-randomsamples" ) ) {
			lightRandomSamples = true;
			Sys_Printf( "Random sampling enabled\n", lightRandomSamples );
		}

		else if ( strEqual( argv[ i ], "-samples" ) ) {
			lightSamplesInsist = ( *argv[i + 1] == '+' );
			lightSamples = atoi( argv[ i + 1 ] );
			if ( lightSamples < 1 ) {
				lightSamples = 1;
			}
			else if ( lightSamples > 1 ) {
				Sys_Printf( "Adaptive supersampling enabled with %d sample(s) per lightmap texel\n", lightSamples );
			}
			i++;
		}

		else if ( strEqual( argv[ i ], "-samplessearchboxsize" ) ) {
			lightSamplesSearchBoxSize = atoi( argv[ i + 1 ] );
//			lightSamplesSearchBoxSize = MAX( MIN( lightSamplesSearchBoxSize, 4 ), 1 );
			lightSamplesSearchBoxSize = lightSamplesSearchBoxSize < 1 ? 1
										: lightSamplesSearchBoxSize > 4 ? 4 /* more makes no sense */
										: lightSamplesSearchBoxSize;
			if ( lightSamplesSearchBoxSize != 1 )
				Sys_Printf( "Adaptive supersampling uses %f times the normal search box size\n", lightSamplesSearchBoxSize );
			i++;
		}

		else if ( strEqual( argv[ i ], "-filter" ) ) {
			filter = true;
			Sys_Printf( "Lightmap filtering enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-dark" ) ) {
			dark = true;
			Sys_Printf( "Dark lightmap seams enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-shadeangle" ) ) {
			shadeAngleDegrees = atof( argv[ i + 1 ] );
			if ( shadeAngleDegrees < 0.0f ) {
				shadeAngleDegrees = 0.0f;
			}
			else if ( shadeAngleDegrees > 0.0f ) {
				shade = true;
				Sys_Printf( "Phong shading enabled with a breaking angle of %f degrees\n", shadeAngleDegrees );
			}
			i++;
		}

		else if ( strEqual( argv[ i ], "-thresh" ) ) {
			subdivideThreshold = atof( argv[ i + 1 ] );
			if ( subdivideThreshold < 0 ) {
				subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;
			}
			else{
				Sys_Printf( "Subdivision threshold set at %.3f\n", subdivideThreshold );
			}
			i++;
		}

		else if ( strEqual( argv[ i ], "-approx" ) ) {
			approximateTolerance = atoi( argv[ i + 1 ] );
			if ( approximateTolerance < 0 ) {
				approximateTolerance = 0;
			}
			else if ( approximateTolerance > 0 ) {
				Sys_Printf( "Approximating lightmaps within a byte tolerance of %d\n", approximateTolerance );
			}
			i++;
		}
		else if ( strEqual( argv[ i ], "-deluxe" ) || strEqual( argv[ i ], "-deluxemap" ) ) {
			deluxemap = true;
			Sys_Printf( "Generating deluxemaps for average light direction\n" );
		}
		else if ( strEqual( argv[ i ], "-deluxemode" ) ) {
			deluxemode = atoi( argv[ i + 1 ] );
			if ( deluxemode == 0 || deluxemode > 1 || deluxemode < 0 ) {
				Sys_Printf( "Generating modelspace deluxemaps\n" );
				deluxemode = 0;
			}
			else{
				Sys_Printf( "Generating tangentspace deluxemaps\n" );
			}
			i++;
		}
		else if ( strEqual( argv[ i ], "-nodeluxe" ) || strEqual( argv[ i ], "-nodeluxemap" ) ) {
			deluxemap = false;
			Sys_Printf( "Disabling generating of deluxemaps for average light direction\n" );
		}
		else if ( strEqual( argv[ i ], "-external" ) ) {
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		else if ( strEqual( argv[ i ], "-lightmapsize" )
				|| strEqual( argv[ i ], "-extlmhacksize" ) ) {
			lmCustomSize = atoi( argv[ i + 1 ] );

			/* must be a power of 2 and greater than 2 */
			if ( ( ( lmCustomSize - 1 ) & lmCustomSize ) || lmCustomSize < 2 ) {
				Sys_Warning( "Lightmap size must be a power of 2, greater or equal to 2 pixels.\n" );
				lmCustomSize = game->lightmapSize;
			}
			i++;
			Sys_Printf( "Default lightmap size set to %d x %d pixels\n", lmCustomSize, lmCustomSize );

			/* enable external lightmaps */
			if ( lmCustomSize != game->lightmapSize ) {
				/* -lightmapsize might just require -external for native external lms, but it has already been used in existing batches alone,
				so brand new switch here for external lms, referenced by shaders hack/behavior */
				externalLightmaps = !strEqual( argv[ i - 1 ], "-extlmhacksize" );
				Sys_Printf( "Storing all lightmaps externally\n" );
			}
		}

		else if ( strEqual( argv[ i ], "-rawlightmapsizelimit" ) ) {
			lmLimitSize = atoi( argv[ i + 1 ] );

			i++;
			Sys_Printf( "Raw lightmap size limit set to %d x %d pixels\n", lmLimitSize, lmLimitSize );
		}

		else if ( strEqual( argv[ i ], "-lightmapdir" ) ) {
			lmCustomDir = argv[i + 1];
			i++;
			Sys_Printf( "Lightmap directory set to %s\n", lmCustomDir );
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		/* ydnar: add this to suppress warnings */
		else if ( strEqual( argv[ i ],  "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}

		else if ( strEqual( argv[ i ], "-wolf" ) ) {
			/* -game should already be set */
			wolfLight = true;
			Sys_Printf( "Enabling Wolf lighting model (linear default)\n" );
		}

		else if ( strEqual( argv[ i ], "-q3" ) ) {
			/* -game should already be set */
			wolfLight = false;
			Sys_Printf( "Enabling Quake 3 lighting model (nonlinear default)\n" );
		}

		else if ( strEqual( argv[ i ], "-extradist" ) ) {
			extraDist = atof( argv[ i + 1 ] );
			if ( extraDist < 0 ) {
				extraDist = 0;
			}
			i++;
			Sys_Printf( "Default extra radius set to %f units\n", extraDist );
		}

		else if ( strEqual( argv[ i ], "-sunonly" ) ) {
			sunOnly = true;
			Sys_Printf( "Only computing sunlight\n" );
		}

		else if ( strEqual( argv[ i ], "-bounceonly" ) ) {
			bounceOnly = true;
			Sys_Printf( "Storing bounced light (radiosity) only\n" );
		}

		else if ( strEqual( argv[ i ], "-nocollapse" ) ) {
			noCollapse = true;
			Sys_Printf( "Identical lightmap collapsing disabled\n" );
		}

		else if ( strEqual( argv[ i ], "-nolightmapsearch" ) ) {
			lightmapSearchBlockSize = 1;
			Sys_Printf( "No lightmap searching - all lightmaps will be sequential\n" );
		}

		else if ( strEqual( argv[ i ], "-lightmapsearchpower" ) ) {
			lightmapMergeSize = ( game->lightmapSize << atoi( argv[i + 1] ) );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - optimize for lightmap merge power %d (size %d)\n", atoi( argv[i] ), lightmapMergeSize );
		}

		else if ( strEqual( argv[ i ], "-lightmapsearchblocksize" ) ) {
			lightmapSearchBlockSize = atoi( argv[i + 1] );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - block size set to %d\n", lightmapSearchBlockSize );
		}

		else if ( strEqual( argv[ i ], "-shade" ) ) {
			shade = true;
			Sys_Printf( "Phong shading enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-bouncegrid" ) ) {
			bouncegrid = true;
			if ( bounce > 0 ) {
				Sys_Printf( "Grid lighting with radiosity enabled\n" );
			}
		}

		else if ( strEqual( argv[ i ], "-smooth" ) ) {
			lightSamples = EXTRA_SCALE;
			Sys_Printf( "The -smooth argument is deprecated, use \"-samples 2\" instead\n" );
		}

		else if ( strEqual( argv[ i ], "-nofastpoint" ) ) {
			fastpoint = false;
			Sys_Printf( "Automatic fast mode for point lights disabled\n" );
		}

		else if ( strEqual( argv[ i ], "-fast" ) ) {
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Fast mode enabled for all area lights\n" );
		}

		else if ( strEqual( argv[ i ], "-faster" ) ) {
			faster = true;
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Faster mode enabled\n" );
		}

//		else if ( strEqual( argv[ i ], "-fastallocate" ) ) {
//			fastAllocate = true;
//			Sys_Printf( "Fast allocation mode enabled\n" );
//		}
		else if ( strEqual( argv[ i ], "-slowallocate" ) ) {
			fastAllocate = false;
			Sys_Printf( "Slow allocation mode enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-fastgrid" ) ) {
			fastgrid = true;
			Sys_Printf( "Fast grid lighting enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-fastbounce" ) ) {
			fastbounce = true;
			Sys_Printf( "Fast bounce mode enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-cheap" ) ) {
			cheap = true;
			cheapgrid = true;
			Sys_Printf( "Cheap mode enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-cheapgrid" ) ) {
			cheapgrid = true;
			Sys_Printf( "Cheap grid mode enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-normalmap" ) ) {
			normalmap = true;
			Sys_Printf( "Storing normal map instead of lightmap\n" );
		}

		else if ( strEqual( argv[ i ], "-trisoup" ) ) {
			trisoup = true;
			Sys_Printf( "Converting brush faces to triangle soup\n" );
		}

		else if ( strEqual( argv[ i ], "-debug" ) ) {
			debug = true;
			Sys_Printf( "Lightmap debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugsurfaces" ) || strEqual( argv[ i ], "-debugsurface" ) ) {
			debugSurfaces = true;
			Sys_Printf( "Lightmap surface debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugunused" ) ) {
			debugUnused = true;
			Sys_Printf( "Unused luxel debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugaxis" ) ) {
			debugAxis = true;
			Sys_Printf( "Lightmap axis debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugcluster" ) ) {
			debugCluster = true;
			Sys_Printf( "Luxel cluster debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugorigin" ) ) {
			debugOrigin = true;
			Sys_Printf( "Luxel origin debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-debugdeluxe" ) ) {
			deluxemap = true;
			debugDeluxemap = true;
			Sys_Printf( "Deluxemap debugging enabled\n" );
		}

		else if ( strEqual( argv[ i ], "-export" ) ) {
			exportLightmaps = true;
			Sys_Printf( "Exporting lightmaps\n" );
		}

		else if ( strEqual( argv[ i ], "-notrace" ) ) {
			noTrace = true;
			Sys_Printf( "Shadow occlusion disabled\n" );
		}
		else if ( strEqual( argv[ i ], "-patchshadows" ) ) {
			patchShadows = true;
			Sys_Printf( "Patch shadow casting enabled\n" );
		}
		else if ( strEqual( argv[ i ], "-extra" ) ) {
			superSample = EXTRA_SCALE;      /* ydnar */
			Sys_Printf( "The -extra argument is deprecated, use \"-super 2\" instead\n" );
		}
		else if ( strEqual( argv[ i ], "-extrawide" ) ) {
			superSample = EXTRAWIDE_SCALE;  /* ydnar */
			filter = true;                  /* ydnar */
			Sys_Printf( "The -extrawide argument is deprecated, use \"-filter [-super 2]\" instead\n" );
		}
		else if ( strEqual( argv[ i ], "-samplesize" ) ) {
			sampleSize = atoi( argv[ i + 1 ] );
			if ( sampleSize < 1 ) {
				sampleSize = 1;
			}
			i++;
			Sys_Printf( "Default lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		else if ( strEqual( argv[ i ], "-minsamplesize" ) ) {
			minSampleSize = atoi( argv[ i + 1 ] );
			if ( minSampleSize < 1 ) {
				minSampleSize = 1;
			}
			i++;
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		else if ( strEqual( argv[ i ],  "-samplescale" ) ) {
			sampleScale = atoi( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Lightmaps sample scale set to %d\n", sampleScale );
		}
		else if ( strEqual( argv[ i ],  "-debugsamplesize" ) ) {
			debugSampleSize = 1;
			Sys_Printf( "debugging Lightmaps SampleSize\n" );
		}
		else if ( strEqual( argv[ i ], "-novertex" ) ) {
			noVertexLighting = 1;
			if ( ( atof( argv[ i + 1 ] ) != 0 ) && ( atof( argv[ i + 1 ] )) < 1 ) {
				noVertexLighting = ( atof( argv[ i + 1 ] ) );
				i++;
				Sys_Printf( "Setting vertex lighting globally to %f\n", noVertexLighting );
			}
			else{
				Sys_Printf( "Disabling vertex lighting\n" );
			}
		}
		else if ( strEqual( argv[ i ], "-nogrid" ) ) {
			noGridLighting = true;
			Sys_Printf( "Disabling grid lighting\n" );
		}
		else if ( strEqual( argv[ i ], "-border" ) ) {
			lightmapBorder = true;
			Sys_Printf( "Adding debug border to lightmaps\n" );
		}
		else if ( strEqual( argv[ i ], "-nosurf" ) ) {
			noSurfaces = true;
			Sys_Printf( "Not tracing against surfaces\n" );
		}
		else if ( strEqual( argv[ i ], "-dump" ) ) {
			dump = true;
			Sys_Printf( "Dumping radiosity lights into numbered prefabs\n" );
		}
		else if ( strEqual( argv[ i ], "-lomem" ) ) {
			loMem = true;
			Sys_Printf( "Enabling low-memory (potentially slower) lighting mode\n" );
		}
		else if ( strEqual( argv[ i ], "-lightanglehl" ) ) {
			if ( ( atoi( argv[ i + 1 ] ) != 0 ) != lightAngleHL ) {
				lightAngleHL = ( atoi( argv[ i + 1 ] ) != 0 );
				if ( lightAngleHL ) {
					Sys_Printf( "Enabling half lambert light angle attenuation\n" );
				}
				else{
					Sys_Printf( "Disabling half lambert light angle attenuation\n" );
				}
				i++;
			}
		}
		else if ( strEqual( argv[ i ], "-nostyle" ) || strEqual( argv[ i ], "-nostyles" ) ) {
			noStyles = true;
			Sys_Printf( "Disabling lightstyles\n" );
		}
		else if ( strEqual( argv[ i ], "-style" ) || strEqual( argv[ i ], "-styles" ) ) {
			noStyles = false;
			Sys_Printf( "Enabling lightstyles\n" );
		}
		else if ( strEqual( argv[ i ], "-cpma" ) ) {
			cpmaHack = true;
			Sys_Printf( "Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
		}
		else if ( strEqual( argv[ i ], "-floodlight" ) ) {
			floodlighty = true;
			Sys_Printf( "FloodLighting enabled\n" );
		}
		else if ( strEqual( argv[ i ], "-debugnormals" ) ) {
			debugnormals = true;
			Sys_Printf( "DebugNormals enabled\n" );
		}
		else if ( strEqual( argv[ i ], "-lowquality" ) ) {
			floodlight_lowquality = true;
			Sys_Printf( "Low Quality FloodLighting enabled\n" );
		}

		/* r7: dirtmapping */
		else if ( strEqual( argv[ i ], "-dirty" ) ) {
			dirty = true;
			Sys_Printf( "Dirtmapping enabled\n" );
		}
		else if ( strEqual( argv[ i ], "-dirtdebug" ) || strEqual( argv[ i ], "-debugdirt" ) ) {
			dirtDebug = true;
			Sys_Printf( "Dirtmap debugging enabled\n" );
		}
		else if ( strEqual( argv[ i ], "-dirtmode" ) ) {
			dirtMode = atoi( argv[ i + 1 ] );
			if ( dirtMode != 0 && dirtMode != 1 ) {
				dirtMode = 0;
			}
			if ( dirtMode == 1 ) {
				Sys_Printf( "Enabling randomized dirtmapping\n" );
			}
			else{
				Sys_Printf( "Enabling ordered dirtmapping\n" );
			}
			i++;
		}
		else if ( strEqual( argv[ i ], "-dirtdepth" ) ) {
			dirtDepth = atof( argv[ i + 1 ] );
			if ( dirtDepth <= 0.0f ) {
				dirtDepth = 128.0f;
			}
			Sys_Printf( "Dirtmapping depth set to %.1f\n", dirtDepth );
			i++;
		}
		else if ( strEqual( argv[ i ], "-dirtscale" ) ) {
			dirtScale = atof( argv[ i + 1 ] );
			if ( dirtScale <= 0.0f ) {
				dirtScale = 1.0f;
			}
			Sys_Printf( "Dirtmapping scale set to %.1f\n", dirtScale );
			i++;
		}
		else if ( strEqual( argv[ i ], "-dirtgain" ) ) {
			dirtGain = atof( argv[ i + 1 ] );
			if ( dirtGain <= 0.0f ) {
				dirtGain = 1.0f;
			}
			Sys_Printf( "Dirtmapping gain set to %.1f\n", dirtGain );
			i++;
		}
		else if ( strEqual( argv[ i ], "-trianglecheck" ) ) {
			lightmapTriangleCheck = true;
		}
		else if ( strEqual( argv[ i ], "-extravisnudge" ) ) {
			lightmapExtraVisClusterNudge = true;
		}
		else if ( strEqual( argv[ i ], "-fill" ) ) {
			lightmapFill = true;
			Sys_Printf( "Filling lightmap colors from surrounding pixels to improve JPEG compression\n" );
		}
		else if ( strEqual( argv[ i ], "-fillpink" ) ) {
			lightmapPink = true;
		}
		/* unhandled args */
		else
		{
			Sys_Warning( "Unknown argument \"%s\"\n", argv[ i ] );
		}

	}

	/* fix up falloff tolerance for sRGB */
	if ( lightmapsRGB ) {
		falloffTolerance = Image_LinearFloatFromsRGBFloat( falloffTolerance * ( 1.0 / 255.0 ) ) * 255.0;
	}

	/* fix up samples count */
	if ( lightRandomSamples ) {
		if ( !lightSamplesInsist ) {
			/* approximately match -samples in quality */
			switch ( lightSamples )
			{
			/* somewhat okay */
			case 1:
			case 2:
				lightSamples = 16;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			/* good */
			case 3:
				lightSamples = 64;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			/* perfect */
			case 4:
				lightSamples = 256;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			default: break;
			}
		}
	}

	/* fix up lightmap search power */
	if ( lightmapMergeSize ) {
		lightmapSearchBlockSize = ( lightmapMergeSize / lmCustomSize ) * ( lightmapMergeSize / lmCustomSize );
		if ( lightmapSearchBlockSize < 1 ) {
			lightmapSearchBlockSize = 1;
		}

		Sys_Printf( "Restricted lightmap searching enabled - block size adjusted to %d\n", lightmapSearchBlockSize );
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	path_set_extension( source, ".bsp" );

	strcpy( name, ExpandArg( argv[ i ] ) );
	if ( !striEqual( path_get_filename_base_end( name ), ".reg" ) ) { /* not .reg */
		path_set_extension( name, ".map" );
	}

	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );

	/* ydnar: handle shaders */
	BeginMapShaderFile( source );
	LoadShaderInfo();

	/* note loading */
	Sys_Printf( "Loading %s\n", source );

	/* ydnar: load surface file */
	LoadSurfaceExtraFile( source );

	/* load bsp file */
	LoadBSPFile( source );

	/* parse bsp entities */
	ParseEntities();

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* load map file */
	if ( !BoolForKey( &entities[ 0 ], "_keepLights" ) ) {
		LoadMapFile( name, true, false );
	}

	/* set the entity/model origins and init yDrawVerts */
	SetEntityOrigins();

	/* ydnar: set up optimization */
	SetupBrushes();
	SetupDirt();
	SetupFloodLight();
	SetupSurfaceLightmaps();

	/* initialize the surface facet tracing */
	SetupTraceNodes();

	/* light the world */
	LightWorld( fastAllocate );

	/* ydnar: store off lightmaps */
	StoreSurfaceLightmaps( fastAllocate );

	/* write out the bsp */
	UnparseEntities();
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* ydnar: export lightmaps */
	if ( exportLightmaps && !externalLightmaps ) {
		ExportLightmaps();
	}

	/* return to sender */
	return 0;
}
