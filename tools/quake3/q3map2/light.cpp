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



/*
   CreateSunLight() - ydnar
   this creates a sun light
 */

static void CreateSunLight( sun_t *sun ){
	int i;
	float photons, d, angle, elevation, da, de;
	Vector3 direction;
	light_t     *light;


	/* dummy check */
	if ( sun == NULL ) {
		return;
	}

	/* fixup */
	value_maximize( sun->numSamples, 1 );

	/* set photons */
	photons = sun->photons / sun->numSamples;

	/* create the right number of suns */
	for ( i = 0; i < sun->numSamples; i++ )
	{
		/* calculate sun direction */
		if ( i == 0 ) {
			direction = sun->direction;
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
			//%	Sys_Printf( "%d: Angle: %3.4lf Elevation: %3.3lf\n", sun->numSamples, radians_to_degrees( angle ), radians_to_degrees( elevation ) );

			/* create new vector */
			direction = vector3_for_spherical( angle, elevation );
		}

		/* create a light */
		numSunLights++;
		light = safe_calloc( sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* initialize the light */
		light->flags = LightFlags::DefaultSun;
		light->type = ELightType::Sun;
		light->fade = 1.0f;
		light->falloffTolerance = falloffTolerance;
		light->filterRadius = sun->filterRadius / sun->numSamples;
		light->style = noStyles ? LS_NORMAL : sun->style;

		/* set the light's position out to infinity */
		light->origin = direction * ( MAX_WORLD_COORD * 8.0f );    /* MAX_WORLD_COORD * 2.0f */

		/* set the facing to be the inverse of the sun direction */
		light->normal = -direction;
		light->dist = vector3_dot( light->origin, light->normal );

		/* set color and photons */
		light->color = sun->color;
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

static void CreateSkyLights( const Vector3& color, float value, int iterations, float filterRadius, int style ){
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
	sun.color = color;
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;
	sun.next = NULL;

	/* setup */
	elevationSteps = iterations - 1;
	angleSteps = elevationSteps * 4;
	elevationStep = degrees_to_radians( 90.0f / iterations );  /* skip elevation 0 */
	angleStep = degrees_to_radians( 360.0f / angleSteps );

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
			sun.direction = vector3_for_spherical( angle, elevation );
			CreateSunLight( &sun );

			/* move */
			angle += angleStep;
		}

		/* move */
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}

	/* create vertical sun */
	sun.direction = g_vector3_axis_z;
	CreateSunLight( &sun );

	/* short circuit */
	return;
}



/*
   CreateEntityLights()
   creates lights from light entities
 */

void CreateEntityLights( void ){
	int j;
	light_t         *light, *light2;
	const entity_t  *e, *e2;


	/* go throught entity list and find lights */
	for ( std::size_t i = 0; i < entities.size(); ++i )
	{
		/* get entity */
		e = &entities[ i ];
		/* ydnar: check for lightJunior */
		bool junior;
		if ( e->classname_is( "lightJunior" ) ) {
			junior = true;
		}
		else if ( e->classname_prefixed( "light" ) ) {
			junior = false;
		}
		else{
			continue;
		}

		/* lights with target names (and therefore styles) are only parsed from BSP */
		if ( !strEmpty( e->valueForKey( "targetname" ) ) && i >= numBSPEntities ) {
			continue;
		}

		/* create a light */
		numPointLights++;
		light = safe_calloc( sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* handle spawnflags */
		const int spawnflags = e->intForKey( "spawnflags" );

		LightFlags flags;
		/* ydnar: quake 3+ light behavior */
		if ( !wolfLight ) {
			/* set default flags */
			flags = LightFlags::DefaultQ3A;

			/* linear attenuation? */
			if ( spawnflags & 1 ) {
				flags |= LightFlags::AttenLinear;
				flags &= ~LightFlags::AttenAngle;
			}

			/* no angle attenuate? */
			if ( spawnflags & 2 ) {
				flags &= ~LightFlags::AttenAngle;
			}
		}

		/* ydnar: wolf light behavior */
		else
		{
			/* set default flags */
			flags = LightFlags::DefaultWolf;

			/* inverse distance squared attenuation? */
			if ( spawnflags & 1 ) {
				flags &= ~LightFlags::AttenLinear;
				flags |= LightFlags::AttenAngle;
			}

			/* angle attenuate? */
			if ( spawnflags & 2 ) {
				flags |= LightFlags::AttenAngle;
			}
		}

		/* other flags (borrowed from wolf) */

		/* wolf dark light? */
		if ( ( spawnflags & 4 ) || ( spawnflags & 8 ) ) {
			flags |= LightFlags::Dark;
		}

		/* nogrid? */
		if ( spawnflags & 16 ) {
			flags &= ~LightFlags::Grid;
		}

		/* junior? */
		if ( junior ) {
			flags |= LightFlags::Grid;
			flags &= ~LightFlags::Surfaces;
		}

		/* vortex: unnormalized? */
		if ( spawnflags & 32 ) {
			flags |= LightFlags::Unnormalized;
		}

		/* vortex: distance atten? */
		if ( spawnflags & 64 ) {
			flags |= LightFlags::AttenDistance;
		}

		/* store the flags */
		light->flags = flags;

		/* ydnar: set fade key (from wolf) */
		light->fade = 1.0f;
		if ( light->flags & LightFlags::AttenLinear ) {
			light->fade = e->floatForKey( "fade" );
			if ( light->fade == 0.0f ) {
				light->fade = 1.0f;
			}
		}

		/* ydnar: set angle scaling (from vlight) */
		light->angleScale = e->floatForKey( "_anglescale" );
		if ( light->angleScale != 0.0f ) {
			light->flags |= LightFlags::AttenAngle;
		}

		/* set origin */
		light->origin = e->vectorForKey( "origin" );
		e->read_keyvalue( light->style, "_style", "style" );
		if ( light->style < LS_NORMAL || light->style >= LS_NONE ) {
			Error( "Invalid lightstyle (%d) on entity %zu", light->style, i );
		}

		/* set light intensity */
		float intensity = 300.f;
		e->read_keyvalue( intensity, "_light", "light" );
		if ( intensity == 0.0f ) {
			intensity = 300.0f;
		}

		{	/* ydnar: set light scale (sof2) */
			float scale;
			if( e->read_keyvalue( scale, "scale" ) && scale != 0.f )
				intensity *= scale;
		}

		/* ydnar: get deviance and samples */
		const float deviance = std::max( 0.f, e->floatForKey( "_deviance", "_deviation", "_jitter" ) );
		const int numSamples = std::max( 1, e->intForKey( "_samples" ) );

		intensity /= numSamples;

		{	/* ydnar: get filter radius */
			light->filterRadius = std::max( 0.f, e->floatForKey( "_filterradius", "_filteradius", "_filter" ) );
		}

		/* set light color */
		if ( e->read_keyvalue( light->color, "_color" ) ) {
			if ( colorsRGB ) {
				light->color[0] = Image_LinearFloatFromsRGBFloat( light->color[0] );
				light->color[1] = Image_LinearFloatFromsRGBFloat( light->color[1] );
				light->color[2] = Image_LinearFloatFromsRGBFloat( light->color[2] );
			}
			if ( !( light->flags & LightFlags::Unnormalized ) ) {
				ColorNormalize( light->color );
			}
		}
		else{
			light->color.set( 1 );
		}


		if( !e->read_keyvalue( light->extraDist, "_extradist" ) )
			light->extraDist = extraDist;

		light->photons = intensity;

		light->type = ELightType::Point;

		/* set falloff threshold */
		light->falloffTolerance = falloffTolerance / numSamples;

		/* lights with a target will be spotlights */
		const char *target;
		if ( e->read_keyvalue( target, "target" ) ) {
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
				light->normal = e2->vectorForKey( "origin" ) - light->origin;
				float dist = VectorNormalize( light->normal );
				float radius = e->floatForKey( "radius" );
				if ( !radius ) {
					radius = 64;
				}
				if ( !dist ) {
					dist = 64;
				}
				light->radiusByDist = ( radius + 16 ) / dist;
				light->type = ELightType::Spot;

				/* ydnar: wolf mods: spotlights always use nonlinear + angle attenuation */
				light->flags &= ~LightFlags::AttenLinear;
				light->flags |= LightFlags::AttenAngle;
				light->fade = 1.0f;

				/* ydnar: is this a sun? */
				if ( e->boolForKey( "_sun" ) ) {
					/* not a spot light */
					numSpotLights--;

					/* unlink this light */
					lights = light->next;

					/* make a sun */
					sun_t sun;
					sun.direction = -light->normal;
					sun.color = light->color;
					sun.photons = intensity;
					sun.deviance = degrees_to_radians( deviance );
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
			if ( light->type == ELightType::Spot ) {
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
	clipWork_t cw;


	/* get sun shader supressor */
	const bool nss = entities[ 0 ].boolForKey( "_noshadersun" );

	/* walk the list of surfaces */
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		/* get surface and other bits */
		ds = &bspDrawSurfaces[ i ];
		info = &surfaceInfos[ i ];
		si = info->si;

		/* sunlight? */
		if ( si->sun != NULL && !nss ) {
			Sys_FPrintf( SYS_VRB, "Sun: %s\n", si->shader.c_str() );
			CreateSunLight( si->sun );
			si->sun = NULL; /* FIXME: leak! */
		}

		/* sky light? */
		if ( si->skyLightValue > 0.0f ) {
			Sys_FPrintf( SYS_VRB, "Sky: %s\n", si->shader.c_str() );
			CreateSkyLights( si->color, si->skyLightValue, si->skyLightIterations, si->lightFilterRadius, si->lightStyle );
			si->skyLightValue = 0.0f;   /* FIXME: hack! */
		}

		/* try to early out */
		if ( si->value <= 0 ) {
			continue;
		}

		/* autosprite shaders become point lights */
		if ( si->autosprite ) {
			/* create a light */
			light = safe_calloc( sizeof( *light ) );
			light->next = lights;
			lights = light;

			/* set it up */
			light->flags = LightFlags::DefaultQ3A;
			light->type = ELightType::Point;
			light->photons = si->value * pointScale;
			light->fade = 1.0f;
			light->si = si;
			light->origin = info->minmax.origin();
			light->color = si->color;
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
	int j, k, f;
	const char          *key;
	int modelnum;
	bspModel_t          *dm;
	bspDrawSurface_t    *ds;


	/* ydnar: copy drawverts into private storage for nefarious purposes */
	yDrawVerts = safe_malloc( numBSPDrawVerts * sizeof( bspDrawVert_t ) );
	memcpy( yDrawVerts, bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVert_t ) );

	/* set the entity origins */
	for ( const auto& e : entities )
	{
		/* get entity and model */
		key = e.valueForKey( "model" );
		if ( key[ 0 ] != '*' ) {
			continue;
		}
		modelnum = atoi( key + 1 );
		dm = &bspModels[ modelnum ];

		/* get entity origin */
		Vector3 origin( 0 );
		if ( !e.read_keyvalue( origin, "origin" ) ) {
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
				yDrawVerts[ f ].xyz = origin + bspDrawVerts[ f ].xyz;
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

float PointToPolygonFormFactor( const Vector3& point, const Vector3& normal, const winding_t *w ){
	int i, j;
	Vector3 dirs[ MAX_POINTS_ON_WINDING ];
	float total;
	float angle, facing;


	/* this is expensive */
	for ( i = 0; i < w->numpoints; i++ )
	{
		dirs[ i ] = w->p[ i ] - point;
		VectorFastNormalize( dirs[ i ] );
	}

	/* duplicate first vertex to avoid mod operation */
	dirs[ i ] = dirs[ 0 ];

	/* calculcate relative area */
	total = 0.0f;
	for ( i = 0; i < w->numpoints; i++ )
	{
		/* get a triangle */
		j = i + 1;

		/* get the angle */
		/* roundoff can cause slight creep, which gives an IND from acos, thus clamp */
		angle = acos( std::clamp( vector3_dot( dirs[ i ], dirs[ j ] ), -1.0, 1.0 ) );

		Vector3 triNormal = vector3_cross( dirs[ i ], dirs[ j ] );
		if ( VectorFastNormalize( triNormal ) < 0.0001f ) {
			continue;
		}

		facing = vector3_dot( normal, triNormal );
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
	trace->color.set( 0 );
	trace->directionContribution.set( 0 );

	colorBrightness = RGBTOGRAY( light->color ) * ( 1.0f / 255.0f );

	/* ydnar: early out */
	if ( !( light->flags & LightFlags::Surfaces ) || light->envelope <= 0.0f ) {
		return 0;
	}

	/* do some culling checks */
	if ( light->type != ELightType::Sun ) {
		/* MrE: if the light is behind the surface */
		if ( !trace->twoSided ) {
			if ( vector3_dot( light->origin, trace->normal ) - vector3_dot( trace->origin, trace->normal ) < 0.0f ) {
				return 0;
			}
		}

		/* ydnar: test pvs */
		if ( !ClusterVisible( trace->cluster, light->cluster ) ) {
			return 0;
		}
	}

	/* exact point to polygon form factor */
	if ( light->type == ELightType::Area ) {
		float factor;
		float d;
		Vector3 pushedOrigin;

		/* project sample point into light plane */
		d = vector3_dot( trace->origin, light->normal ) - light->dist;
		if ( d < 3.0f ) {
			/* sample point behind plane? */
			if ( !( light->flags & LightFlags::Twosided ) && d < -1.0f ) {
				return 0;
			}

			/* sample plane coincident? */
			if ( d > -3.0f && vector3_dot( trace->normal, light->normal ) > 0.9f ) {
				return 0;
			}
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emitter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			pushedOrigin = trace->origin + light->normal * ( 8.0f - d );
		}
		else{
			pushedOrigin = trace->origin;
		}

		/* get direction and distance */
		trace->end = light->origin;
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* ptpff approximation */
		if ( faster ) {
			/* angle attenuation */
			angle = vector3_dot( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && angle < 0 ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* attenuate */
			angle *= -vector3_dot( light->normal, trace->direction );
			if ( angle == 0.0f ) {
				return 0;
			}
			else if ( angle < 0.0f &&
			          ( trace->twoSided || ( light->flags & LightFlags::Twosided ) ) ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* clamp the distance to prevent super hot spots */
			dist = std::max( 16.0, sqrt( dist * dist + light->extraDist * light->extraDist ) );

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
				if ( trace->twoSided || ( light->flags & LightFlags::Twosided ) ) {
					factor = -factor;

					/* push light origin to other side of the plane */
					trace->end = light->origin - light->normal * 2.f;
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
			if ( vector3_dot( trace->normal, trace->direction ) < 0 ) {
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
	else if ( light->type == ELightType::Point || light->type == ELightType::Spot ) {
		/* get direction and distance */
		trace->end = light->origin;
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* clamp the distance to prevent super hot spots */
		dist = std::max( 16.0, sqrt( dist * dist + light->extraDist * light->extraDist ) );

		/* angle attenuation */
		if ( light->flags & LightFlags::AttenAngle ) {
			/* standard Lambert attenuation */
			float dot = vector3_dot( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					value_minimize( dot, 1.0f );
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
			value_minimize( angle, 1.0f );
		}

		/* attenuate */
		if ( light->flags & LightFlags::AttenLinear ) {
			add = std::max( 0.f, angle * light->photons * linearScale - ( dist * light->fade ) );

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = angle * light->photons * linearScale - ( dist * light->fade );
				}
				else{
					addDeluxe = light->photons * linearScale - ( dist * light->fade );
				}

				value_maximize( addDeluxe, 0.0f );
			}
		}
		else
		{
			add = std::max( 0.f, ( light->photons / ( dist * dist ) ) * angle );

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = ( light->photons / ( dist * dist ) ) * angle;
				}
				else{
					addDeluxe = ( light->photons / ( dist * dist ) );
				}
			}

			value_maximize( addDeluxe, 0.0f );
		}

		/* handle spotlights */
		if ( light->type == ELightType::Spot ) {
			/* do cone calculation */
			const float distByNormal = -vector3_dot( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return 0;
			}
			const Vector3 pointAtDist = light->origin + light->normal * distByNormal;
			const float radiusAtDist = light->radiusByDist * distByNormal;
			const Vector3 distToSample = trace->origin - pointAtDist;
			const float sampleRadius = vector3_length( distToSample );

			/* outside the cone */
			if ( sampleRadius >= radiusAtDist ) {
				return 0;
			}

			/* attenuate */
			if ( sampleRadius > ( radiusAtDist - 32.0f ) ) {
				add *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
				value_maximize( add, 0.0f );

				addDeluxe *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
				value_maximize( addDeluxe, 0.0f );
			}
		}
	}

	/* ydnar: sunlight */
	else if ( light->type == ELightType::Sun ) {
		/* get origin and direction */
		trace->end = trace->origin + light->origin;
		dist = SetupTrace( trace );

		/* angle attenuation */
		if ( light->flags & LightFlags::AttenAngle ) {
			/* standard Lambert attenuation */
			float dot = vector3_dot( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = false;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					value_minimize( dot, 1.0f );
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

			value_maximize( addDeluxe, 0.0f );
		}

		if ( add <= 0.0f ) {
			return 0;
		}

		addDeluxe *= colorBrightness;

		if ( bouncing ) {
			addDeluxe *= addDeluxeBounceScale;
			value_maximize( addDeluxe, 0.00390625f );
		}

		trace->directionContribution = trace->direction * addDeluxe;

		/* setup trace */
		trace->testAll = true;
		trace->color = light->color * add;

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			trace->forceSubsampling *= add;
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				trace->color.set( 0 );
				trace->directionContribution.set( 0 );

				return -1;
			}
		}

		/* return to sender */
		return 1;
	}
	else{
		Error( "Light of undefined type!" );
	}

	/* ydnar: changed to a variable number */
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LightFlags::FastActual ) ) ) {
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
		trace->directionContribution = trace->direction * addDeluxe;
	}

	/* setup trace */
	trace->testAll = false;
	trace->color = light->color * add;

	/* raytrace */
	TraceLine( trace );
	trace->forceSubsampling *= add;
	if ( trace->passSolid || trace->opaque ) {
		trace->color.set( 0 );
		trace->directionContribution.set( 0 );

		return -1;
	}

	/* return to sender */
	return 1;
}



/*
   LightingAtSample()
   determines the amount of light reaching a sample (luxel or vertex)
 */

void LightingAtSample( trace_t *trace, byte styles[ MAX_LIGHTMAPS ], Vector3 (&colors)[ MAX_LIGHTMAPS ] ){
	int i, lightmapNum;


	/* clear colors */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		colors[ lightmapNum ].set( 0 );

	/* ydnar: normalmap */
	if ( normalmap ) {
		colors[ 0 ] = ( trace->normal + Vector3( 1 ) ) * 127.5f;
		return;
	}

	/* ydnar: don't bounce ambient all the time */
	if ( !bouncing ) {
		colors[ 0 ] = ambientColor;
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
		if ( trace->color == g_vector3_identity ) {
			continue;
		}

		/* handle negative light */
		if ( trace->light->flags & LightFlags::Negative ) {
			vector3_negate( trace->color );
		}

		/* set style */
		styles[ lightmapNum ] = trace->light->style;

		/* add it */
		colors[ lightmapNum ] += trace->color;

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
	trace->color.set( 0 );

	/* ydnar: early out */
	if ( !( light->flags & LightFlags::Grid ) || light->envelope <= 0.0f ) {
		return false;
	}

	/* is this a sun? */
	if ( light->type != ELightType::Sun ) {
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
	if ( !light->minmax.test( trace->origin ) ) {
		gridBoundsCulled++;
		return false;
	}

	/* set light origin */
	if ( light->type == ELightType::Sun ) {
		trace->end = trace->origin + light->origin;
	}
	else{
		trace->end = light->origin;
	}

	/* set direction */
	dist = SetupTrace( trace );

	/* test envelope */
	if ( dist > light->envelope ) {
		gridEnvelopeCulled++;
		return false;
	}

	/* ptpff approximation */
	if ( light->type == ELightType::Area && faster ) {
		/* clamp the distance to prevent super hot spots */
		dist = std::max( 16.0, sqrt( dist * dist + light->extraDist * light->extraDist ) );

		/* attenuate */
		add = light->photons / ( dist * dist );
	}

	/* exact point to polygon form factor */
	else if ( light->type == ELightType::Area ) {
		float factor, d;
		Vector3 pushedOrigin;


		/* see if the point is behind the light */
		d = vector3_dot( trace->origin, light->normal ) - light->dist;
		if ( !( light->flags & LightFlags::Twosided ) && d < -1.0f ) {
			return false;
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emiter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			pushedOrigin = trace->origin + light->normal * ( 8.0f - d );
		}
		else{
			pushedOrigin = trace->origin;
		}

		/* calculate the contribution (ydnar 2002-10-21: [bug 642] bad normal calc) */
		factor = PointToPolygonFormFactor( pushedOrigin, trace->direction, light->w );
		if ( factor == 0.0f ) {
			return false;
		}
		else if ( factor < 0.0f ) {
			if ( light->flags & LightFlags::Twosided ) {
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
	else if ( light->type == ELightType::Point || light->type == ELightType::Spot ) {
		/* clamp the distance to prevent super hot spots */
		dist = std::max( 16.0, sqrt( dist * dist + light->extraDist * light->extraDist ) );

		/* attenuate */
		if ( light->flags & LightFlags::AttenLinear ) {
			add = std::max( 0.f, light->photons * linearScale - ( dist * light->fade ) );
		}
		else{
			add = light->photons / ( dist * dist );
		}

		/* handle spotlights */
		if ( light->type == ELightType::Spot ) {
			/* do cone calculation */
			const float distByNormal = -vector3_dot( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return false;
			}
			const Vector3 pointAtDist = light->origin + light->normal * distByNormal;
			const float radiusAtDist = light->radiusByDist * distByNormal;
			const Vector3 distToSample = trace->origin - pointAtDist;
			const float sampleRadius = vector3_length( distToSample );

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
	else if ( light->type == ELightType::Sun ) {
		/* attenuate */
		add = light->photons;
		if ( add <= 0.0f ) {
			return false;
		}

		/* setup trace */
		trace->testAll = true;
		trace->color = light->color * add;

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				trace->color.set( 0 );
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
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LightFlags::FastActual ) ) ) {
		return false;
	}

	/* setup trace */
	trace->testAll = false;
	trace->color = light->color * add;

	/* trace */
	TraceLine( trace );
	if ( trace->passSolid ) {
		trace->color.set( 0 );
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

struct contribution_t
{
	Vector3 dir;
	Vector3 color;
	Vector3 ambient;
	int style;
};

void TraceGrid( int num ){
	int i, j, x, y, z, mod, numCon, numStyles;
	float d, step;
	Vector3 cheapColor, thisdir;
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

	trace.origin = gridMins + Vector3( x, y, z ) * gridSize;

	/* set inhibit sphere */
	trace.inhibitRadius = gridSize[ vector3_max_abs_component_index( gridSize ) ] * 0.5f;

	/* find point cluster */
	trace.cluster = ClusterForPointExt( trace.origin, GRID_EPSILON );
	if ( trace.cluster < 0 ) {
		/* try to nudge the origin around to find a valid point */
		const Vector3 baseOrigin( trace.origin );
		for ( step = 0; ( step += 0.005 ) <= 1.0; )
		{
			trace.origin = baseOrigin;
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
	cheapColor.set( 0 );

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
		if ( trace.light->flags & LightFlags::Negative ) {
			vector3_negate( trace.color );
		}

		/* add a contribution */
		contributions[ numCon ].color = trace.color;
		contributions[ numCon ].dir = trace.direction;
		contributions[ numCon ].ambient.set( 0 );
		contributions[ numCon ].style = trace.light->style;
		numCon++;

		/* push average direction around */
		addSize = vector3_length( trace.color );
		gp->dir += trace.direction * addSize;

		/* stop after a while */
		if ( numCon >= ( MAX_CONTRIBUTIONS - 1 ) ) {
			break;
		}

		/* ydnar: cheap mode */
		cheapColor += trace.color;
		if ( cheapgrid && cheapColor[ 0 ] >= 255.0f && cheapColor[ 1 ] >= 255.0f && cheapColor[ 2 ] >= 255.0f ) {
			break;
		}
	}

	/////// Floodlighting for point //////////////////
	//do our floodlight ambient occlusion loop, and add a single contribution based on the brightest dir
	if ( floodlighty ) {
		int k;
		float addSize, f;
		Vector3 dir = g_vector3_axis_z;
		float ambientFrac = 0.25f;

		trace.testOcclusion = true;
		trace.forceSunlight = false;
		trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
		trace.testAll = true;

		for ( k = 0; k < 2; k++ )
		{
			if ( k == 0 ) { // upper hemisphere
				trace.normal = g_vector3_axis_z;
			}
			else //lower hemisphere
			{
				trace.normal = -g_vector3_axis_z;
			}

			f = FloodLightForSample( &trace, floodlightDistance, floodlight_lowquality );

			/* add a fraction as pure ambient, half as top-down direction */
			contributions[ numCon ].color = floodlightRGB * ( floodlightIntensity * f * ( 1.0f - ambientFrac ) );

			contributions[ numCon ].ambient = floodlightRGB * ( floodlightIntensity * f * ambientFrac );

			contributions[ numCon ].dir = dir;

			contributions[ numCon ].style = 0;

			/* push average direction around */
			addSize = vector3_length( contributions[ numCon ].color );
			gp->dir += dir * addSize;

			numCon++;
		}
	}
	/////////////////////

	/* normalize to get primary light direction */
	thisdir = VectorNormalized( gp->dir );

	/* now that we have identified the primary light direction,
	   go back and separate all the light into directed and ambient */

	numStyles = 1;
	for ( i = 0; i < numCon; i++ )
	{
		/* get relative directed strength */
		d = vector3_dot( contributions[ i ].dir, thisdir );
		/* we map 1 to gridDirectionality, and 0 to gridAmbientDirectionality */
		d = std::max( 0.f, gridAmbientDirectionality + d * ( gridDirectionality - gridAmbientDirectionality ) );

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
		gp->directed[ j ] += contributions[ i ].color * d;

		/* ambient light will be at 1/4 the value of directed light */
		/* (ydnar: nuke this in favor of more dramatic lighting?) */
		/* (PM: how about actually making it work? d=1 when it got here for single lights/sun :P */
//		d = 0.25f;
		/* (Hobbes: always setting it to .25 is hardly any better) */
		d = 0.25f * ( 1.0f - d );
		gp->ambient[ j ] += contributions[ i ].color * d;

		gp->ambient[ j ] += contributions[ i ].ambient;

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
		Vector3 color = gp->ambient[ i ];
		for ( j = 0; j < 3; j++ )
			value_maximize( color[ j ], minGridLight[ j ] );

		/* vortex: apply gridscale and gridambientscale here */
		bgp->ambient[ i ] = ColorToBytes( color, gridScale * gridAmbientScale );
		bgp->directed[ i ] = ColorToBytes( gp->directed[ i ], gridScale );
		/*
		 * HACK: if there's a non-zero directed component, this
		 * lightgrid cell is useful. However, q3 skips grid
		 * cells with zero ambient. So let's force ambient to be
		 * nonzero unless directed is zero too.
		 */
		 if( bgp->ambient[i][0] + bgp->ambient[i][1] + bgp->ambient[i][2] == 0
		&& bgp->directed[i][0] + bgp->directed[i][1] + bgp->directed[i][2] != 0 )
			bgp->ambient[i].set( 1 );
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
	char temp[ 64 ];


	/* don't do this if not grid lighting */
	if ( noGridLighting ) {
		return;
	}

	/* ydnar: set grid size */
	entities[ 0 ].read_keyvalue( gridSize, "gridsize" );

	/* quantize it */
	const Vector3 oldGridSize = gridSize;
	for ( i = 0; i < 3; i++ )
		gridSize[ i ] = std::max( 8.0, floor( gridSize[ i ] ) );

	/* ydnar: increase gridSize until grid count is smaller than max allowed */
	numRawGridPoints = MAX_MAP_LIGHTGRID + 1;
	j = 0;
	while ( numRawGridPoints > MAX_MAP_LIGHTGRID )
	{
		/* get world bounds */
		for ( i = 0; i < 3; i++ )
		{
			gridMins[ i ] = gridSize[ i ] * ceil( bspModels[ 0 ].minmax.mins[ i ] / gridSize[ i ] );
			const float max = gridSize[ i ] * floor( bspModels[ 0 ].minmax.maxs[ i ] / gridSize[ i ] );
			gridBounds[ i ] = ( max - gridMins[ i ] ) / gridSize[ i ] + 1;
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
		entities[ 0 ].setKeyValue( "gridsize", (const char*) temp );
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
		for ( j = 0; j < MAX_LIGHTMAPS; j++ )
			rawGridPoints[ i ].ambient[ j ] = ambientColor;

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
	Vector3 color;
	float f;
	int b, bt;
	bool minVertex, minGrid;


	/* ydnar: smooth normals */
	if ( shade ) {
		Sys_Printf( "--- SmoothNormals ---\n" );
		SmoothNormals();
	}

	/* find the optional minimum lighting values */
	color = entities[ 0 ].vectorForKey( "_color" );
	if ( colorsRGB ) {
		color[0] = Image_LinearFloatFromsRGBFloat( color[0] );
		color[1] = Image_LinearFloatFromsRGBFloat( color[1] );
		color[2] = Image_LinearFloatFromsRGBFloat( color[2] );
	}
	if ( vector3_length( color ) == 0.0f ) {
		color.set( 1 );
	}

	/* ambient */
	f = entities[ 0 ].floatForKey( "_ambient", "ambient" );
	ambientColor = color * f;

	/* minvertexlight */
	if ( ( minVertex = entities[ 0 ].read_keyvalue( f, "_minvertexlight" ) ) ) {
		minVertexLight = color * f;
	}

	/* mingridlight */
	if ( ( minGrid = entities[ 0 ].read_keyvalue( f, "_mingridlight" ) ) ) {
		minGridLight = color * f;
	}

	/* minlight */
	if ( entities[ 0 ].read_keyvalue( f, "_minlight" ) ) {
		minLight = color * f;
		if ( !minVertex )
			minVertexLight = color * f;
		if ( !minGrid )
			minGridLight = color * f;
	}

	/* maxlight */
	if ( entities[ 0 ].read_keyvalue( f, "_maxlight" ) ) {
		maxLight = std::clamp( f, 0.f, 255.f );
	}

	/* determine the number of grid points */
	Sys_Printf( "--- SetupGrid ---\n" );
	SetupGrid(); // uses ambientColor read above

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
		ambientColor.set( 0 );
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

	lmCustomSizeW = lmCustomSizeH = game->lightmapSize;
	Sys_Printf( " lightmap size: %d x %d pixels\n", lmCustomSizeW, lmCustomSizeH );

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
		if ( striEqual( argv[ i ], "-point" ) || striEqual( argv[ i ], "-pointscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-spherical" ) || striEqual( argv[ i ], "-sphericalscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-spot" ) || striEqual( argv[ i ], "-spotscale" ) ) {
			f = atof( argv[ i + 1 ] );
			spotScale *= f;
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-area" ) || striEqual( argv[ i ], "-areascale" ) ) {
			f = atof( argv[ i + 1 ] );
			areaScale *= f;
			Sys_Printf( "Area (shader) light scaled by %f to %f\n", f, areaScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-sky" ) || striEqual( argv[ i ], "-skyscale" ) ) {
			f = atof( argv[ i + 1 ] );
			skyScale *= f;
			Sys_Printf( "Sky/sun light scaled by %f to %f\n", f, skyScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-vertexscale" ) ) {
			f = atof( argv[ i + 1 ] );
			vertexglobalscale *= f;
			Sys_Printf( "Vertexlight scaled by %f to %f\n", f, vertexglobalscale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-backsplash" ) && i < ( argc - 3 ) ) {
			g_backsplashFractionScale = atof( argv[ i + 1 ] );
			Sys_Printf( "Area lights backsplash fraction scaled by %f\n", g_backsplashFractionScale );
			f = atof( argv[ i + 2 ] );
			if ( f >= -900.0f ){
				g_backsplashDistance = f;
				Sys_Printf( "Area lights backsplash distance set globally to %f\n", g_backsplashDistance );
			}
			i+=2;
		}

		else if ( striEqual( argv[ i ], "-nolm" ) ) {
			nolm = true;
			Sys_Printf( "No lightmaps yo\n" );
		}

		else if ( striEqual( argv[ i ], "-bouncecolorratio" ) ) {
			bounceColorRatio = std::clamp( atof( argv[ i + 1 ] ), 0.0, 1.0 );
			Sys_Printf( "Bounce color ratio set to %f\n", bounceColorRatio );
			i++;
		}

		else if ( striEqual( argv[ i ], "-bouncescale" ) ) {
			f = atof( argv[ i + 1 ] );
			bounceScale *= f;
			Sys_Printf( "Bounce (radiosity) light scaled by %f to %f\n", f, bounceScale );
			i++;
		}

		else if ( striEqual( argv[ i ], "-scale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			areaScale *= f;
			skyScale *= f;
			bounceScale *= f;
			Sys_Printf( "All light scaled by %f\n", f );
			i++;
		}

		else if ( striEqual( argv[ i ], "-gridscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid lightning scaled by %f\n", f );
			gridScale *= f;
			i++;
		}

		else if ( striEqual( argv[ i ], "-gridambientscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid ambient lightning scaled by %f\n", f );
			gridAmbientScale *= f;
			i++;
		}

		else if ( striEqual( argv[ i ], "-griddirectionality" ) ) {
			gridDirectionality = std::min( 1.0, atof( argv[ i + 1 ] ) );
			value_minimize( gridAmbientDirectionality, gridDirectionality );
			Sys_Printf( "Grid directionality is %f\n", gridDirectionality );
			i++;
		}

		else if ( striEqual( argv[ i ], "-gridambientdirectionality" ) ) {
			gridAmbientDirectionality = std::max( -1.0, atof( argv[ i + 1 ] ) );
			value_maximize( gridDirectionality, gridAmbientDirectionality );
			Sys_Printf( "Grid ambient directionality is %f\n", gridAmbientDirectionality );
			i++;
		}

		else if ( striEqual( argv[ i ], "-gamma" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapGamma = f;
			Sys_Printf( "Lighting gamma set to %f\n", lightmapGamma );
			i++;
		}

		else if ( striEqual( argv[ i ], "-sRGBlight" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
		}

		else if ( striEqual( argv[ i ], "-nosRGBlight" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
		}

		else if ( striEqual( argv[ i ], "-sRGBtex" ) ) {
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
		}

		else if ( striEqual( argv[ i ], "-nosRGBtex" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
		}

		else if ( striEqual( argv[ i ], "-sRGBcolor" ) ) {
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( striEqual( argv[ i ], "-nosRGBcolor" ) ) {
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( striEqual( argv[ i ], "-sRGB" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( striEqual( argv[ i ], "-nosRGB" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( striEqual( argv[ i ], "-exposure" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapExposure = f;
			Sys_Printf( "Lighting exposure set to %f\n", lightmapExposure );
			i++;
		}

		else if ( striEqual( argv[ i ], "-compensate" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f <= 0.0f ) {
				f = 1.0f;
			}
			lightmapCompensate = f;
			Sys_Printf( "Lighting compensation set to 1/%f\n", lightmapCompensate );
			i++;
		}

		/* Lightmaps brightness */
		else if( striEqual( argv[ i ], "-brightness" ) ){
			lightmapBrightness = atof( argv[ i + 1 ] );
			Sys_Printf( "Scaling lightmaps brightness by %f\n", lightmapBrightness );
			i++;
		}

		/* Lighting contrast */
		else if( striEqual( argv[ i ], "-contrast" ) ){
			lightmapContrast = std::clamp( atof( argv[ i + 1 ] ), -255.0, 255.0 );
			Sys_Printf( "Lighting contrast set to %f\n", lightmapContrast );
			i++;
			/* change to factor in range of 0 to 129.5 */
			lightmapContrast = ( 259 * ( lightmapContrast + 255 ) ) / ( 255 * ( 259 - lightmapContrast ) );
		}

		/* Lighting saturation */
		else if( striEqual( argv[ i ], "-saturation" ) ){
			g_lightmapSaturation = atof( argv[ i + 1 ] );
			Sys_Printf( "Lighting saturation set to %f\n", g_lightmapSaturation );
			i++;
		}

		/* ydnar switches */
		else if ( striEqual( argv[ i ], "-bounce" ) ) {
			bounce = std::max( 0, atoi( argv[ i + 1 ] ) );
			if ( bounce > 0 ) {
				Sys_Printf( "Radiosity enabled with %d bounce(s)\n", bounce );
			}
			i++;
		}

		else if ( striEqual( argv[ i ], "-supersample" ) || striEqual( argv[ i ], "-super" ) ) {
			superSample = std::max( 1, atoi( argv[ i + 1 ] ) );
			if ( superSample > 1 ) {
				Sys_Printf( "Ordered-grid supersampling enabled with %d sample(s) per lightmap texel\n", ( superSample * superSample ) );
			}
			i++;
		}

		else if ( striEqual( argv[ i ], "-randomsamples" ) ) {
			lightRandomSamples = true;
			Sys_Printf( "Random sampling enabled\n", lightRandomSamples );
		}

		else if ( striEqual( argv[ i ], "-samples" ) ) {
			lightSamplesInsist = ( *argv[i + 1] == '+' );
			lightSamples = std::max( 1, atoi( argv[ i + 1 ] ) );
			if ( lightSamples > 1 ) {
				Sys_Printf( "Adaptive supersampling enabled with %d sample(s) per lightmap texel\n", lightSamples );
			}
			i++;
		}

		else if ( striEqual( argv[ i ], "-samplessearchboxsize" ) ) {
			lightSamplesSearchBoxSize = std::clamp( atoi( argv[ i + 1 ] ), 1, 4 ); /* more makes no sense */
			if ( lightSamplesSearchBoxSize != 1 )
				Sys_Printf( "Adaptive supersampling uses %f times the normal search box size\n", lightSamplesSearchBoxSize );
			i++;
		}

		else if ( striEqual( argv[ i ], "-filter" ) ) {
			filter = true;
			Sys_Printf( "Lightmap filtering enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-dark" ) ) {
			dark = true;
			Sys_Printf( "Dark lightmap seams enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-shadeangle" ) ) {
			shadeAngleDegrees = std::max( 0.0, atof( argv[ i + 1 ] ) );
			if ( shadeAngleDegrees > 0.0f ) {
				shade = true;
				Sys_Printf( "Phong shading enabled with a breaking angle of %f degrees\n", shadeAngleDegrees );
			}
			i++;
		}

		else if ( striEqual( argv[ i ], "-thresh" ) ) {
			subdivideThreshold = atof( argv[ i + 1 ] );
			if ( subdivideThreshold < 0 ) {
				subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;
			}
			else{
				Sys_Printf( "Subdivision threshold set at %.3f\n", subdivideThreshold );
			}
			i++;
		}

		else if ( striEqual( argv[ i ], "-approx" ) ) {
			approximateTolerance = std::max( 0, atoi( argv[ i + 1 ] ) );
			if ( approximateTolerance > 0 ) {
				Sys_Printf( "Approximating lightmaps within a byte tolerance of %d\n", approximateTolerance );
			}
			i++;
		}
		else if ( striEqual( argv[ i ], "-deluxe" ) || striEqual( argv[ i ], "-deluxemap" ) ) {
			deluxemap = true;
			Sys_Printf( "Generating deluxemaps for average light direction\n" );
		}
		else if ( striEqual( argv[ i ], "-deluxemode" ) ) {
			deluxemode = atoi( argv[ i + 1 ] );
			if ( deluxemode != 1 ) {
				Sys_Printf( "Generating modelspace deluxemaps\n" );
				deluxemode = 0;
			}
			else{
				Sys_Printf( "Generating tangentspace deluxemaps\n" );
			}
			i++;
		}
		else if ( striEqual( argv[ i ], "-nodeluxe" ) || striEqual( argv[ i ], "-nodeluxemap" ) ) {
			deluxemap = false;
			Sys_Printf( "Disabling generating of deluxemaps for average light direction\n" );
		}
		else if ( striEqual( argv[ i ], "-external" ) ) {
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		else if ( striEqual( argv[ i ], "-lightmapsize" )
		       || striEqual( argv[ i ], "-extlmhacksize" ) ) {
			const bool extlmhack = striEqual( argv[ i ], "-extlmhacksize" );

			lmCustomSizeW = lmCustomSizeH = atoi( argv[ i + 1 ] );
			if( i + 2 < argc - 1 && argv[ i + 2 ][0] != '-' && 0 != atoi( argv[ i + 2 ] ) ){
				lmCustomSizeH = atoi( argv[ i + 2 ] );
				i++;
			}
			i++;
			/* must be a power of 2 and greater than 2 */
			if ( ( ( lmCustomSizeW - 1 ) & lmCustomSizeW ) || lmCustomSizeW < 2 ||
			     ( ( lmCustomSizeH - 1 ) & lmCustomSizeH ) || lmCustomSizeH < 2 ) {
				Sys_Warning( "Lightmap size must be a power of 2, greater or equal to 2 pixels.\n" );
				lmCustomSizeW = lmCustomSizeH = game->lightmapSize;
			}
			Sys_Printf( "Default lightmap size set to %d x %d pixels\n", lmCustomSizeW, lmCustomSizeH );

			/* enable external lightmaps */
			if ( lmCustomSizeW != game->lightmapSize || lmCustomSizeH != game->lightmapSize ) {
				/* -lightmapsize might just require -external for native external lms, but it has already been used in existing batches alone,
				so brand new switch here for external lms, referenced by shaders hack/behavior */
				externalLightmaps = !extlmhack;
				Sys_Printf( "Storing all lightmaps externally\n" );
			}
		}

		else if ( striEqual( argv[ i ], "-rawlightmapsizelimit" ) ) {
			lmLimitSize = atoi( argv[ i + 1 ] );

			i++;
			Sys_Printf( "Raw lightmap size limit set to %d x %d pixels\n", lmLimitSize, lmLimitSize );
		}

		else if ( striEqual( argv[ i ], "-lightmapdir" ) ) {
			lmCustomDir = argv[i + 1];
			i++;
			Sys_Printf( "Lightmap directory set to %s\n", lmCustomDir );
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		/* ydnar: add this to suppress warnings */
		else if ( striEqual( argv[ i ],  "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}

		else if ( striEqual( argv[ i ], "-wolf" ) ) {
			/* -game should already be set */
			wolfLight = true;
			Sys_Printf( "Enabling Wolf lighting model (linear default)\n" );
		}

		else if ( striEqual( argv[ i ], "-q3" ) ) {
			/* -game should already be set */
			wolfLight = false;
			Sys_Printf( "Enabling Quake 3 lighting model (nonlinear default)\n" );
		}

		else if ( striEqual( argv[ i ], "-extradist" ) ) {
			extraDist = std::max( 0.0, atof( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Default extra radius set to %f units\n", extraDist );
		}

		else if ( striEqual( argv[ i ], "-sunonly" ) ) {
			sunOnly = true;
			Sys_Printf( "Only computing sunlight\n" );
		}

		else if ( striEqual( argv[ i ], "-bounceonly" ) ) {
			bounceOnly = true;
			Sys_Printf( "Storing bounced light (radiosity) only\n" );
		}

		else if ( striEqual( argv[ i ], "-nocollapse" ) ) {
			noCollapse = true;
			Sys_Printf( "Identical lightmap collapsing disabled\n" );
		}

		else if ( striEqual( argv[ i ], "-nolightmapsearch" ) ) {
			lightmapSearchBlockSize = 1;
			Sys_Printf( "No lightmap searching - all lightmaps will be sequential\n" );
		}

		else if ( striEqual( argv[ i ], "-lightmapsearchpower" ) ) {
			lightmapMergeSize = ( game->lightmapSize << atoi( argv[i + 1] ) );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - optimize for lightmap merge power %d (size %d)\n", atoi( argv[i] ), lightmapMergeSize );
		}

		else if ( striEqual( argv[ i ], "-lightmapsearchblocksize" ) ) {
			lightmapSearchBlockSize = atoi( argv[i + 1] );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - block size set to %d\n", lightmapSearchBlockSize );
		}

		else if ( striEqual( argv[ i ], "-shade" ) ) {
			shade = true;
			Sys_Printf( "Phong shading enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-bouncegrid" ) ) {
			bouncegrid = true;
			if ( bounce > 0 ) {
				Sys_Printf( "Grid lighting with radiosity enabled\n" );
			}
		}

		else if ( striEqual( argv[ i ], "-smooth" ) ) {
			lightSamples = EXTRA_SCALE;
			Sys_Printf( "The -smooth argument is deprecated, use \"-samples 2\" instead\n" );
		}

		else if ( striEqual( argv[ i ], "-nofastpoint" ) ) {
			fastpoint = false;
			Sys_Printf( "Automatic fast mode for point lights disabled\n" );
		}

		else if ( striEqual( argv[ i ], "-fast" ) ) {
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Fast mode enabled for all area lights\n" );
		}

		else if ( striEqual( argv[ i ], "-faster" ) ) {
			faster = true;
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Faster mode enabled\n" );
		}

//		else if ( striEqual( argv[ i ], "-fastallocate" ) ) {
//			fastAllocate = true;
//			Sys_Printf( "Fast allocation mode enabled\n" );
//		}
		else if ( striEqual( argv[ i ], "-slowallocate" ) ) {
			fastAllocate = false;
			Sys_Printf( "Slow allocation mode enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-fastgrid" ) ) {
			fastgrid = true;
			Sys_Printf( "Fast grid lighting enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-fastbounce" ) ) {
			fastbounce = true;
			Sys_Printf( "Fast bounce mode enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-cheap" ) ) {
			cheap = true;
			cheapgrid = true;
			Sys_Printf( "Cheap mode enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-cheapgrid" ) ) {
			cheapgrid = true;
			Sys_Printf( "Cheap grid mode enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-normalmap" ) ) {
			normalmap = true;
			Sys_Printf( "Storing normal map instead of lightmap\n" );
		}

		else if ( striEqual( argv[ i ], "-trisoup" ) ) {
			trisoup = true;
			Sys_Printf( "Converting brush faces to triangle soup\n" );
		}

		else if ( striEqual( argv[ i ], "-debug" ) ) {
			debug = true;
			Sys_Printf( "Lightmap debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-debugsurfaces" ) || striEqual( argv[ i ], "-debugsurface" ) ) {
			debugSurfaces = true;
			Sys_Printf( "Lightmap surface debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-debugaxis" ) ) {
			debugAxis = true;
			Sys_Printf( "Lightmap axis debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-debugcluster" ) ) {
			debugCluster = true;
			Sys_Printf( "Luxel cluster debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-debugorigin" ) ) {
			debugOrigin = true;
			Sys_Printf( "Luxel origin debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-debugdeluxe" ) ) {
			deluxemap = true;
			debugDeluxemap = true;
			Sys_Printf( "Deluxemap debugging enabled\n" );
		}

		else if ( striEqual( argv[ i ], "-export" ) ) {
			exportLightmaps = true;
			Sys_Printf( "Exporting lightmaps\n" );
		}

		else if ( striEqual( argv[ i ], "-notrace" ) ) {
			noTrace = true;
			Sys_Printf( "Shadow occlusion disabled\n" );
		}
		else if ( striEqual( argv[ i ], "-patchshadows" ) ) {
			patchShadows = true;
			Sys_Printf( "Patch shadow casting enabled\n" );
		}
		else if ( striEqual( argv[ i ], "-extra" ) ) {
			superSample = EXTRA_SCALE;      /* ydnar */
			Sys_Printf( "The -extra argument is deprecated, use \"-super 2\" instead\n" );
		}
		else if ( striEqual( argv[ i ], "-extrawide" ) ) {
			superSample = EXTRAWIDE_SCALE;  /* ydnar */
			filter = true;                  /* ydnar */
			Sys_Printf( "The -extrawide argument is deprecated, use \"-filter [-super 2]\" instead\n" );
		}
		else if ( striEqual( argv[ i ], "-samplesize" ) ) {
			sampleSize = std::max( 1, atoi( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Default lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		else if ( striEqual( argv[ i ], "-minsamplesize" ) ) {
			minSampleSize = std::max( 1, atoi( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		else if ( striEqual( argv[ i ],  "-samplescale" ) ) {
			sampleScale = atoi( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Lightmaps sample scale set to %d\n", sampleScale );
		}
		else if ( striEqual( argv[ i ],  "-debugsamplesize" ) ) {
			debugSampleSize = 1;
			Sys_Printf( "debugging Lightmaps SampleSize\n" );
		}
		else if ( striEqual( argv[ i ], "-novertex" ) ) {
			noVertexLighting = 1;
			f = atof( argv[ i + 1 ] );
			if ( f != 0 && f < 1 ) {
				noVertexLighting = f;
				i++;
				Sys_Printf( "Setting vertex lighting globally to %f\n", noVertexLighting );
			}
			else{
				Sys_Printf( "Disabling vertex lighting\n" );
			}
		}
		else if ( striEqual( argv[ i ], "-nogrid" ) ) {
			noGridLighting = true;
			Sys_Printf( "Disabling grid lighting\n" );
		}
		else if ( striEqual( argv[ i ], "-border" ) ) {
			lightmapBorder = true;
			Sys_Printf( "Adding debug border to lightmaps\n" );
		}
		else if ( striEqual( argv[ i ], "-nosurf" ) ) {
			noSurfaces = true;
			Sys_Printf( "Not tracing against surfaces\n" );
		}
		else if ( striEqual( argv[ i ], "-dump" ) ) {
			dump = true;
			Sys_Printf( "Dumping radiosity lights into numbered prefabs\n" );
		}
		else if ( striEqual( argv[ i ], "-lomem" ) ) {
			loMem = true;
			Sys_Printf( "Enabling low-memory (potentially slower) lighting mode\n" );
		}
		else if ( striEqual( argv[ i ], "-lightanglehl" ) ) {
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
		else if ( striEqual( argv[ i ], "-nostyle" ) || striEqual( argv[ i ], "-nostyles" ) ) {
			noStyles = true;
			Sys_Printf( "Disabling lightstyles\n" );
		}
		else if ( striEqual( argv[ i ], "-style" ) || striEqual( argv[ i ], "-styles" ) ) {
			noStyles = false;
			Sys_Printf( "Enabling lightstyles\n" );
		}
		else if ( striEqual( argv[ i ], "-cpma" ) ) {
			cpmaHack = true;
			Sys_Printf( "Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
		}
		else if ( striEqual( argv[ i ], "-floodlight" ) ) {
			floodlighty = true;
			Sys_Printf( "FloodLighting enabled\n" );
		}
		else if ( striEqual( argv[ i ], "-debugnormals" ) ) {
			debugnormals = true;
			Sys_Printf( "DebugNormals enabled\n" );
		}
		else if ( striEqual( argv[ i ], "-lowquality" ) ) {
			floodlight_lowquality = true;
			Sys_Printf( "Low Quality FloodLighting enabled\n" );
		}

		/* r7: dirtmapping */
		else if ( striEqual( argv[ i ], "-dirty" ) ) {
			dirty = true;
			Sys_Printf( "Dirtmapping enabled\n" );
		}
		else if ( striEqual( argv[ i ], "-dirtdebug" ) || striEqual( argv[ i ], "-debugdirt" ) ) {
			dirtDebug = true;
			Sys_Printf( "Dirtmap debugging enabled\n" );
		}
		else if ( striEqual( argv[ i ], "-dirtmode" ) ) {
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
		else if ( striEqual( argv[ i ], "-dirtdepth" ) ) {
			dirtDepth = atof( argv[ i + 1 ] );
			if ( dirtDepth <= 0.0f ) {
				dirtDepth = 128.0f;
			}
			Sys_Printf( "Dirtmapping depth set to %.1f\n", dirtDepth );
			i++;
		}
		else if ( striEqual( argv[ i ], "-dirtscale" ) ) {
			dirtScale = atof( argv[ i + 1 ] );
			if ( dirtScale <= 0.0f ) {
				dirtScale = 1.0f;
			}
			Sys_Printf( "Dirtmapping scale set to %.1f\n", dirtScale );
			i++;
		}
		else if ( striEqual( argv[ i ], "-dirtgain" ) ) {
			dirtGain = atof( argv[ i + 1 ] );
			if ( dirtGain <= 0.0f ) {
				dirtGain = 1.0f;
			}
			Sys_Printf( "Dirtmapping gain set to %.1f\n", dirtGain );
			i++;
		}
		else if ( striEqual( argv[ i ], "-trianglecheck" ) ) {
			lightmapTriangleCheck = true;
		}
		else if ( striEqual( argv[ i ], "-extravisnudge" ) ) {
			lightmapExtraVisClusterNudge = true;
		}
		else if ( striEqual( argv[ i ], "-fill" ) ) {
			lightmapFill = true;
			Sys_Printf( "Filling lightmap colors from surrounding pixels to improve JPEG compression\n" );
		}
		else if ( striEqual( argv[ i ], "-fillpink" ) ) {
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
		lightmapSearchBlockSize = std::max( 1, ( lightmapMergeSize / lmCustomSizeW ) * ( lightmapMergeSize / lmCustomSizeW ) ); //? should use min or max( lmCustomSizeW, lmCustomSizeH )? :thinking:

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
	if ( !entities[ 0 ].boolForKey( "_keepLights" ) ) {
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
