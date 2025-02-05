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
#include "bspfile_rbsp.h"
#include <set>



/*
   CreateSunLight() - ydnar
   this creates a sun light
 */

static void CreateSunLight( sun_t& sun ){
	/* fixup */
	value_maximize( sun.numSamples, 1 );

	/* set photons */
	const float photons = sun.photons / sun.numSamples;

	/* create the right number of suns */
	for ( int i = 0; i < sun.numSamples; ++i )
	{
		/* calculate sun direction */
		Vector3 direction;
		if ( i == 0 ) {
			direction = sun.direction;
		}
		else
		{
			/*
			    sun.direction[ 0 ] = cos( angle ) * cos( elevation );
			    sun.direction[ 1 ] = sin( angle ) * cos( elevation );
			    sun.direction[ 2 ] = sin( elevation );

			    xz_dist   = sqrt( x*x + z*z )
			    latitude  = atan2( xz_dist, y ) * RADIANS
			    longitude = atan2( x,       z ) * RADIANS
			 */

			const double d = sqrt( sun.direction[ 0 ] * sun.direction[ 0 ] + sun.direction[ 1 ] * sun.direction[ 1 ] );
			double angle = atan2( sun.direction[ 1 ], sun.direction[ 0 ] );
			double elevation = atan2( sun.direction[ 2 ], d );

			/* jitter the angles (loop to keep random sample within sun.deviance steridians) */
			float da, de;
			do
			{
				da = ( Random() * 2.0f - 1.0f ) * sun.deviance;
				de = ( Random() * 2.0f - 1.0f ) * sun.deviance;
			}
			while ( ( da * da + de * de ) > ( sun.deviance * sun.deviance ) );
			angle += da;
			elevation += de;

			/* debug code */
			//%	Sys_Printf( "%d: Angle: %3.4lf Elevation: %3.3lf\n", sun.numSamples, radians_to_degrees( angle ), radians_to_degrees( elevation ) );

			/* create new vector */
			direction = vector3_for_spherical( angle, elevation );
		}

		/* create a light */
		numSunLights++;
		light_t& light = lights.emplace_front();

		/* initialize the light */
		light.flags = LightFlags::DefaultSun;
		light.type = ELightType::Sun;
		light.fade = 1.0f;
		light.falloffTolerance = falloffTolerance;
		light.filterRadius = sun.filterRadius / sun.numSamples;
		light.style = noStyles ? LS_NORMAL : sun.style;

		/* set the light's position out to infinity */
		light.origin = direction * ( MAX_WORLD_COORD * 8.0f );    /* MAX_WORLD_COORD * 2.0f */

		/* set the facing to be the inverse of the sun direction */
		light.normal = -direction;
		light.dist = vector3_dot( light.origin, light.normal );

		/* set color and photons */
		light.color = sun.color;
		light.photons = photons * skyScale;
	}
}



class SkyProbes
{
	struct SkyProbe
	{
		Vector3 color;
		Vector3 normal;
	};
	std::vector<SkyProbe> m_probes;
public:
	SkyProbes() = default;
	SkyProbes( const String64& skyParmsImageBase ){
		if( !skyParmsImageBase.empty() ){
			std::vector<const image_t*> images;
			for( const auto suffix : { "_lf", "_rt", "_ft", "_bk", "_up", "_dn" } )
			{
				if( nullptr == images.emplace_back( ImageLoad( StringStream<64>( skyParmsImageBase, suffix ) ) ) ){
					Sys_Warning( "Couldn't find image %s\n", StringStream<64>( skyParmsImageBase, suffix ).c_str() );
					return;
				}
			}

			const size_t res = 64;
			m_probes.reserve( res * res * 6 );

	/* Q3 skybox (top view)
		   ^Y
		   |
		   bk
		lf    rt ->X
		   ft
	*/
			// postrotate everything from _rt (+x)
			const std::array<Matrix4, 6> transforms{ matrix4_scale_for_vec3( Vector3( -1, -1, 1 ) ),
													g_matrix4_identity,
													matrix4_rotation_for_sincos_z( -1, 0 ),
													matrix4_rotation_for_sincos_z( 1, 0 ),
													matrix4_rotation_for_sincos_y( -1, 0 ),
													matrix4_rotation_for_sincos_y( 1, 0 ) };

	/* img
			0-----> width / U
			|he
			|ig
			|ht
			V
	*/
			for( size_t i = 0; i < 6; ++i )
			{
				for( size_t u = 0; u < res; ++u )
				{
					for( size_t v = 0; v < res; ++v )
					{
						const Vector3 normal = vector3_normalised( Vector3( 1, 1 - u * 2.f / res, 1 - v * 2.f / res ) );
						Vector3 color( 0 );
						{
							const image_t& img = *images[i];
							const size_t w = img.width * u / res;
							const size_t w2 = std::max( w + 1, img.width * ( u + 1 ) / res );
							const size_t h = img.height * v / res;
							const size_t h2 = std::max( h + 1, img.height * ( v + 1 ) / res );
							for( size_t iw = w; iw < w2; ++iw )
							{
								for( size_t ih = h; ih < h2; ++ih )
								{
									color += vector3_from_array( img.at( iw, ih ) );
								}
							}
							color /= ( ( w2 - w ) * ( h2 - h ) );
						}
						color *= vector3_dot( normal, g_vector3_axis_x );
						m_probes.push_back( SkyProbe{ color / 255, matrix4_transformed_direction( transforms[i], normal ) } );
					}
				}
			}
		}
	}
	Vector3 sampleColour( const Vector3& direction, const Vector3& limitDirection ) const {
		Vector3 color( 0 );
		float weightSum = 0;
		for( const auto& probe : m_probes )
		{
			const double dot = vector3_dot( probe.normal, direction );
			if( dot > 0 && vector3_dot( probe.normal, limitDirection ) >= 0 ){
				color += probe.color * dot;
				weightSum += dot;
			}
		}
		return weightSum != 0? color / weightSum : color;
	}
	operator bool() const {
		return !m_probes.empty();
	}
};




/*
   CreateSkyLights() - ydnar
   simulates sky light with multiple suns
 */

static void CreateSkyLights( const skylight_t& skylight, const Vector3& color, float filterRadius, int style, const String64& skyParmsImageBase ){
	/* dummy check */
	if ( skylight.value <= 0.0f || skylight.iterations < 2 || skylight.horizon_min > skylight.horizon_max ) {
		return;
	}

	const SkyProbes probes = skylight.sample_color? SkyProbes( skyParmsImageBase ) : SkyProbes();
	const Vector3 probesLimitDirection = skylight.horizon_min >= 0? g_vector3_axis_z : skylight.horizon_max <= 0? -g_vector3_axis_z : Vector3( 0 );

	/* basic sun setup */
	sun_t sun;
	std::vector<sun_t> suns;
	sun.color = color;
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;

	/* setup */
	const int doBot = ( skylight.horizon_min == -90 );
	const int doTop = ( skylight.horizon_max == 90 );
	const int angleSteps = ( skylight.iterations - 1 ) * 4;
	const float eleStep = 90.0f / skylight.iterations;
	const float elevationStep = degrees_to_radians( eleStep );  /* skip elevation 0 */
	const float angleStep = degrees_to_radians( 360.0f / angleSteps );
	// const int elevationSteps = skylight.iterations - 1;
	const float eleMin = doBot? -90 + eleStep * 1.5 : skylight.horizon_min + eleStep * 0.5;
	const float eleMax = doTop? 90 - eleStep * 1.5 : skylight.horizon_max - eleStep * 0.5;
	const int elevationSteps = float_to_integer( 1 + std::max( 0.f, ( eleMax - eleMin ) / eleStep ) );

	/* calc individual sun brightness */
	const int numSuns = angleSteps * elevationSteps + doBot + doTop;
	sun.photons = skylight.value / numSuns * std::max( .25, ( skylight.horizon_max - skylight.horizon_min ) / 90.0 );
	suns.reserve( numSuns );

	/* iterate elevation */
	float elevation = degrees_to_radians( std::min( eleMin, float( skylight.horizon_max ) ) );
	float angle = 0.0f;
	for ( int i = 0; i < elevationSteps; ++i )
	{
		/* iterate angle */
		for ( int j = 0; j < angleSteps; ++j )
		{
			/* create sun */
			sun.direction = vector3_for_spherical( angle, elevation );
			if( probes )
				sun.color = probes.sampleColour( sun.direction, probesLimitDirection );
			suns.push_back( sun );

			/* move */
			angle += angleStep;
		}

		/* move */
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}

	/* create vertical suns */
	if( doBot ){
		sun.direction = -g_vector3_axis_z;
		if( probes )
			sun.color = probes.sampleColour( sun.direction, probesLimitDirection );
		suns.push_back( sun );
	}
	if( doTop ){
		sun.direction = g_vector3_axis_z;
		if( probes )
			sun.color = probes.sampleColour( sun.direction, probesLimitDirection );
		suns.push_back( sun );
	}

	/* normalize colours */
	if( probes ){
		float max = 0;
		for( const auto& sun : suns )
		{
			value_maximize( max, sun.color[0] );
			value_maximize( max, sun.color[1] );
			value_maximize( max, sun.color[2] );
		}
		if( max != 0 )
			for( auto& sun : suns )
				sun.color /= max;
	}

	std::for_each( suns.begin(), suns.end(), CreateSunLight );
}



/*
   CreateEntityLights()
   creates lights from light entities
 */

static void CreateEntityLights(){
	/* go through entity list and find lights */
	for ( std::size_t i = 0; i < entities.size(); ++i )
	{
		/* get entity */
		const entity_t& e = entities[ i ];
		/* ydnar: check for lightJunior */
		bool junior;
		if ( e.classname_is( "lightJunior" ) ) {
			junior = true;
		}
		else if ( e.classname_prefixed( "light" ) ) {
			junior = false;
		}
		else{
			continue;
		}

		/* lights with target names (and therefore styles in RBSP) are only parsed from BSP */
		if ( !strEmpty( e.valueForKey( "targetname" ) ) && g_game->load == LoadRBSPFile && i >= numBSPEntities ) {
			continue;
		}

		/* create a light */
		numPointLights++;
		light_t& light = lights.emplace_front();

		/* handle spawnflags */
		const int spawnflags = e.intForKey( "spawnflags" );

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
		light.flags = flags;

		/* ydnar: set fade key (from wolf) */
		light.fade = 1.0f;
		if ( light.flags & LightFlags::AttenLinear ) {
			light.fade = e.floatForKey( "fade" );
			if ( light.fade == 0.0f ) {
				light.fade = 1.0f;
			}
		}

		/* ydnar: set angle scaling (from vlight) */
		light.angleScale = e.floatForKey( "_anglescale" );
		if ( light.angleScale != 0.0f ) {
			light.flags |= LightFlags::AttenAngle;
		}

		/* set origin */
		light.origin = e.vectorForKey( "origin" );
		e.read_keyvalue( light.style, "_style", "style" );
		if ( !style_is_valid( light.style ) ) {
			Error( "Invalid lightstyle (%d) on entity %zu", light.style, i );
		}

		/* set light intensity */
		float intensity = 300.f;
		e.read_keyvalue( intensity, "_light", "light" );
		if ( intensity == 0.0f ) {
			intensity = 300.0f;
		}

		{	/* ydnar: set light scale (sof2) */
			float scale;
			if( e.read_keyvalue( scale, "scale" ) && scale != 0.f )
				intensity *= scale;
		}

		/* ydnar: get deviance and samples */
		const float deviance = std::max( 0.f, e.floatForKey( "_deviance", "_deviation", "_jitter" ) );
		const int numSamples = std::max( 1, e.intForKey( "_samples" ) );

		intensity /= numSamples;

		{	/* ydnar: get filter radius */
			light.filterRadius = std::max( 0.f, e.floatForKey( "_filterradius", "_filteradius", "_filter" ) );
		}

		/* set light color */
		if ( e.read_keyvalue( light.color, "_color" ) ) {
			if ( colorsRGB ) {
				light.color[0] = Image_LinearFloatFromsRGBFloat( light.color[0] );
				light.color[1] = Image_LinearFloatFromsRGBFloat( light.color[1] );
				light.color[2] = Image_LinearFloatFromsRGBFloat( light.color[2] );
			}
			if ( !( light.flags & LightFlags::Unnormalized ) ) {
				ColorNormalize( light.color );
			}
		}
		else{
			light.color.set( 1 );
		}


		if( !e.read_keyvalue( light.extraDist, "_extradist" ) )
			light.extraDist = extraDist;

		light.photons = intensity;

		light.type = ELightType::Point;

		/* set falloff threshold */
		light.falloffTolerance = falloffTolerance / numSamples;

		/* lights with a target will be spotlights */
		const char *target;
		if ( e.read_keyvalue( target, "target" ) ) {
			/* get target */
			const entity_t *e2 = FindTargetEntity( target );
			if ( e2 == NULL ) {
				Sys_Warning( "light at (%i %i %i) has missing target\n",
				             (int) light.origin[ 0 ], (int) light.origin[ 1 ], (int) light.origin[ 2 ] );
				light.photons *= pointScale;
			}
			else
			{
				/* not a point light */
				numPointLights--;
				numSpotLights++;

				/* make a spotlight */
				light.normal = e2->vectorForKey( "origin" ) - light.origin;
				float dist = VectorNormalize( light.normal );
				float radius = e.floatForKey( "radius" );
				if ( !radius ) {
					radius = 64;
				}
				if ( !dist ) {
					dist = 64;
				}
				light.radiusByDist = ( radius + 16 ) / dist;
				light.type = ELightType::Spot;

				/* ydnar: wolf mods: spotlights always use nonlinear + angle attenuation */
				light.flags &= ~LightFlags::AttenLinear;
				light.flags |= LightFlags::AttenAngle;
				light.fade = 1.0f;

				/* ydnar: is this a sun? */
				if ( e.boolForKey( "_sun" ) ) {
					/* not a spot light */
					numSpotLights--;

					/* make a sun */
					sun_t sun{};
					sun.direction = -light.normal;
					sun.color = light.color;
					sun.photons = intensity;
					sun.deviance = degrees_to_radians( deviance );
					sun.numSamples = numSamples;
					sun.style = noStyles ? LS_NORMAL : light.style;

					/* free original light before sun insertion */
					lights.pop_front();

					/* make a sun light */
					CreateSunLight( sun );

					/* skip the rest of this love story */
					continue;
				}
				else
				{
					light.photons *= spotScale;
				}
			}
		}
		else{
			light.photons *= pointScale;
		}

		/* jitter the light */
		for ( int j = 1; j < numSamples; j++ )
		{
			/* create a light */
			light_t& light2 = lights.emplace_front( light );

			/* add to counts */
			if ( light.type == ELightType::Spot ) {
				numSpotLights++;
			}
			else{
				numPointLights++;
			}

			/* jitter it */
			light2.origin[ 0 ] = light.origin[ 0 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2.origin[ 1 ] = light.origin[ 1 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2.origin[ 2 ] = light.origin[ 2 ] + ( Random() * 2.0f - 1.0f ) * deviance;
		}
	}
}



/*
   CreateSurfaceLights() - ydnar
   this hijacks the radiosity code to generate surface lights for first pass
 */

#define APPROX_BOUNCE   1.0f

static void CreateSurfaceLights(){
	clipWork_t cw;


	/* get sun shader suppressor */
	const bool nss = entities[ 0 ].boolForKey( "_noshadersun" );

	/* walk the list of surfaces */
	for ( size_t i = 0; i < bspDrawSurfaces.size(); i++ )
	{
		/* get surface and other bits */
		bspDrawSurface_t *ds = &bspDrawSurfaces[ i ];
		surfaceInfo_t *info = &surfaceInfos[ i ];
		const shaderInfo_t *si = info->si;

		/* sunlight? */
		if ( !si->suns.empty() && !nss ) {
			shaderInfo_t* si_ = const_cast<shaderInfo_t*>( si );   /* FIXME: hack! */
			Sys_FPrintf( SYS_VRB, "Sun: %s\n", si->shader.c_str() );
			std::for_each( si_->suns.begin(), si_->suns.end(), CreateSunLight );
			si_->suns.clear();   /* FIXME: hack! */
		}

		/* sky light? */
		if ( !si->skylights.empty() ) {
			Sys_FPrintf( SYS_VRB, "Sky: %s\n", si->shader.c_str() );
			for( const skylight_t& skylight : si->skylights )
				CreateSkyLights( skylight, si->color, si->lightFilterRadius, si->lightStyle, si->skyParmsImageBase );
			const_cast<shaderInfo_t*>( si )->skylights.clear();   /* FIXME: hack! */
		}

		/* try to early out */
		if ( si->value <= 0 ) {
			continue;
		}

		/* autosprite shaders become point lights */
		if ( si->autosprite ) {
			/* create a light */
			light_t& light = lights.emplace_front();

			/* set it up */
			light.flags = LightFlags::DefaultQ3A;
			light.type = ELightType::Point;
			light.photons = si->value * pointScale;
			light.fade = 1.0f;
			light.si = si;
			light.origin = info->minmax.origin();
			light.color = si->color;
			light.falloffTolerance = falloffTolerance;
			light.style = si->lightStyle;

			/* add to point light count and continue */
			numPointLights++;
			continue;
		}

		/* get subdivision amount */
		const float subdivide = si->lightSubdivide > 0? si->lightSubdivide : defaultLightSubdivide;

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

static void SetEntityOrigins(){
	/* ydnar: copy drawverts into private storage for nefarious purposes */
	yDrawVerts = bspDrawVerts;

	/* set the entity origins */
	for ( const auto& e : entities )
	{
		/* get entity and model */
		const char *key = e.valueForKey( "model" );
		if ( key[ 0 ] != '*' ) {
			continue;
		}
		const int modelnum = atoi( key + 1 );
		const bspModel_t& dm = bspModels[ modelnum ];

		/* get entity origin */
		Vector3 origin( 0 );
		if ( !e.read_keyvalue( origin, "origin" ) ) {
			continue;
		}

		/* set origin for all surfaces for this model */
		for ( int j = 0; j < dm.numBSPSurfaces; j++ )
		{
			/* get drawsurf */
			const bspDrawSurface_t& ds = bspDrawSurfaces[ dm.firstBSPSurface + j ];

			/* set its verts */
			for ( int k = 0; k < ds.numVerts; k++ )
			{
				yDrawVerts[ ds.firstVert + k ].xyz += origin;
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

float PointToPolygonFormFactor( const Vector3& point, const Vector3& normal, const winding_t& w ){
	Vector3 dirs[ MAX_POINTS_ON_WINDING ];
	double total = 0;


	/* this is expensive */
	for ( size_t i = 0; i < w.size(); ++i )
	{
		dirs[ i ] = w[ i ] - point;
		VectorFastNormalize( dirs[ i ] );
	}

	/* duplicate first vertex to avoid mod operation */
	dirs[ w.size() ] = dirs[ 0 ];

	/* calculcate relative area */
	for ( size_t i = 0; i < w.size(); ++i )
	{
		/* get a triangle */
		Vector3 triNormal = vector3_cross( dirs[ i ], dirs[ i + 1 ] );
		if ( VectorFastNormalize( triNormal ) < 0.0001f ) {
			continue;
		}

		/* get the angle */
		/* roundoff can cause slight creep, which gives an IND from acos, thus clamp */
		const double angle = acos( std::clamp( vector3_dot( dirs[ i ], dirs[ i + 1 ] ), -1.0, 1.0 ) );

		const double facing = vector3_dot( normal, triNormal );
		total += facing * angle;

		/* ydnar: this was throwing too many errors with radiosity + crappy maps. ignoring it. */
		if ( total > 6.3 || total < -6.3 ) {
			return 0.0f;
		}
	}

	/* now in the range of 0 to 1 over the entire incoming hemisphere */
	//%	total /= ( 2.0f * 3.141592657f );
	return total * c_inv_2pi;
}



/*
   LightContributionTosample()
   determines the amount of light reaching a sample (luxel or vertex) from a given light
 */

int LightContributionToSample( trace_t *trace ){
	float angle;
	float add;
	float dist;
	float addDeluxe = 0.0f, addDeluxeBounceScale = 0.25f;
	bool angledDeluxe = true;
	float colorBrightness;
	bool doAddDeluxe = true;

	/* get light */
	const light_t *light = trace->light;

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
			dist = std::max( 16.f, std::sqrt( dist * dist + light->extraDist * light->extraDist ) );

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
		dist = std::max( 16.f, std::sqrt( dist * dist + light->extraDist * light->extraDist ) );

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

static bool LightContributionToPoint( trace_t *trace ){
	float add, dist;

	/* get light */
	const light_t *light = trace->light;

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
		dist = std::max( 16.f, std::sqrt( dist * dist + light->extraDist * light->extraDist ) );

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
		dist = std::max( 16.f, std::sqrt( dist * dist + light->extraDist * light->extraDist ) );

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

static void TraceGrid( int num ){
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
	for ( const light_t& light : lights )
	{
		trace.light = &light;
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

static void SetupGrid(){
	/* don't do this if not grid lighting */
	if ( noGridLighting ) {
		return;
	}

	/* ydnar: set grid size */
	entities[ 0 ].read_keyvalue( gridSize, "gridsize" );

	/* quantize it */
	const Vector3 oldGridSize = gridSize;
	for ( int i = 0; i < 3; i++ )
		gridSize[ i ] = std::max( 8.f, std::floor( gridSize[ i ] ) );

	/* ydnar: increase gridSize until grid count is smaller than max allowed */
	size_t numGridPoints;
	for( int j = 0; ; )
	{
		/* get world bounds */
		for ( int i = 0; i < 3; i++ )
		{
			gridMins[ i ] = gridSize[ i ] * ceil( bspModels[ 0 ].minmax.mins[ i ] / gridSize[ i ] );
			const float max = gridSize[ i ] * floor( bspModels[ 0 ].minmax.maxs[ i ] / gridSize[ i ] );
			gridBounds[ i ] = ( max - gridMins[ i ] ) / gridSize[ i ] + 1;
		}

		const int64_t num = int64_t( gridBounds[ 0 ] ) * gridBounds[ 1 ] * gridBounds[ 2 ]; // int64_t prevents reachable int32_t overflow : cube( 131072 / 8 ) = 4398046511104

		/* increase grid size a bit */
		if ( num > MAX_MAP_LIGHTGRID ) {
			gridSize[ j++ % 3 ] += 16.0f;
		}
		else{
			/* set grid size */
			numGridPoints = num;
			break;
		}
	}

	/* print it */
	Sys_Printf( "Grid size = { %1.0f, %1.0f, %1.0f }\n", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );

	/* different? */
	if ( !VectorCompare( gridSize, oldGridSize ) ) {
		char temp[ 64 ];
		sprintf( temp, "%.0f %.0f %.0f", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
		entities[ 0 ].setKeyValue( "gridsize", temp );
		Sys_FPrintf( SYS_VRB, "Storing adjusted grid size\n" );
	}

	/* allocate and clear lightgrid */
	{
		static_assert( MAX_LIGHTMAPS == 4 );
		rawGridPoints = decltype( rawGridPoints )( numGridPoints, rawGridPoint_t{
			{ ambientColor, ambientColor, ambientColor, ambientColor },
			{ g_vector3_identity, g_vector3_identity, g_vector3_identity, g_vector3_identity },
			g_vector3_identity,
			{ LS_NORMAL, LS_NONE, LS_NONE, LS_NONE } } );
		bspGridPoints = decltype( bspGridPoints )( numGridPoints, bspGridPoint_t{
			{ Vector3b( 0 ), Vector3b( 0 ), Vector3b( 0 ), Vector3b( 0 ) },
			{ Vector3b( 0 ), Vector3b( 0 ), Vector3b( 0 ), Vector3b( 0 ) },
			{ LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			{ 0, 0 } } );
	}

	/* note it */
	Sys_Printf( "%9zu grid points\n", rawGridPoints.size() );
}



/*
	Handles writing optional hacks after -light and each -bounce pass
*/

static void WriteBSPFileAfterLight( const char *bspFileName ){
	const auto lmPathStart = String64( "maps/", mapName, '/' );
	std::set<int> lmIds;
	// find external lm ids, if any
	// stupidly search in shader text, ( numExtLightmaps > 0 ) check wont work when e.g. deluxemaps
	// also would excessively include lightstyles using $lightmap reference
	if( !externalLightmaps ){ // unless native ext lms: e.g. in ET with lightstyles hack preloading lm imgs breaks r_mapOverbrightBits
		for ( const shaderInfo_t& si : Span( shaderInfo, numShaderInfo ) )
		{
			if ( si.custom && !strEmptyOrNull( si.shaderText ) ) {
				const char *txt = si.shaderText;
				while( ( txt = strstr( txt, lmPathStart ) ) ){
					txt += strlen( lmPathStart );
					int lmindex;
					int okcount = 0;
					if( sscanf( txt, EXTERNAL_LIGHTMAP "%n", &lmindex, &okcount ) && okcount == strlen( EXTERNAL_LIGHTMAP ) ){
						lmIds.insert( lmindex );
					}
				}
			}
		}
	}

	if( !lmIds.empty() || trisoup ){
		std::vector<bspModel_t> bakModels;
		std::vector<int> bakLeafSurfaces;
		std::vector<bspBrushSide_t> bakBrushSides;
		std::vector<bspDrawSurface_t> bakDrawSurfaces;

		if( !lmIds.empty() ){
			{ // write nomipmaps shaders
				/* dummy check */
				ENSURE( !mapShaderFile.empty() );

				/* open shader file */
				FILE *file = fopen( mapShaderFile.c_str(), "at" ); // append to existing file
				if ( file == NULL ) {
					Sys_Warning( "Unable to open map shader file %s for writing\n", mapShaderFile.c_str() );
					return;
				}

				/* print header */
				fprintf( file, "\n// Shaders to force nomipmaps flag on external lightmap images:\n\n" );

				/* devise les shaders: max 8 stages, max 8 maps in animmap */
				size_t shaderId = 0;
				for( auto it = lmIds.cbegin(); it != lmIds.cend(); ){
					fprintf( file, "%s\n{\n\tnomipmaps\n", String64( lmPathStart, "nomipmaps", shaderId++ ).c_str() );
					for( size_t i = 0; i < 8 && it != lmIds.cend(); ++i ){
						fprintf( file, "\t{\n\t\tanimmap .0042" );
						for( size_t j = 0; j < 8 && it != lmIds.cend(); ++j, ++it ){
							fprintf( file, " %s" EXTERNAL_LIGHTMAP, lmPathStart.c_str(), *it );
						}
						fprintf( file, "\n\t}\n" );
					}
					fprintf( file, "}\n\n" );
				}

				/* close the shader */
				fflush( file );
				fclose( file );
			}

			const size_t numNomipSurfs = ( lmIds.size() + 63 ) / 64;
			// prepend nomip surfs to the list so that they are read by engine first
			for( size_t su = 0; su < numNomipSurfs; ++su ){
				auto& out = bakDrawSurfaces.emplace_back();

				out.surfaceType = MST_FLARE;
				out.shaderNum = EmitShader( String64( lmPathStart, "nomipmaps", su ), nullptr, nullptr );
				out.fogNum = -1;

				for ( int i = 0; i < MAX_LIGHTMAPS; i++ )
				{
					out.lightmapNum[ i ] = -3;
					out.lightmapStyles[ i ] = LS_NONE;
					out.vertexStyles[ i ] = LS_NONE;
				}
			}

			bakDrawSurfaces.insert( bakDrawSurfaces.cend(), bspDrawSurfaces.cbegin(), bspDrawSurfaces.cend() );
			// backup original bsp lumps
			bakDrawSurfaces.swap( bspDrawSurfaces );

			bakModels = bspModels;
			bakModels.swap( bspModels );
			bakLeafSurfaces = bspLeafSurfaces;
			bakLeafSurfaces.swap( bspLeafSurfaces );
			bakBrushSides = bspBrushSides;
			bakBrushSides.swap( bspBrushSides );
			// repair indices
			for( auto&& index : bspLeafSurfaces )
				index += numNomipSurfs;
			for( auto&& side : bspBrushSides )
				side.surfaceNum += numNomipSurfs;
			for( auto&& model : bspModels )
				model.firstBSPSurface += numNomipSurfs;
		}

		/* ydnar: optional force-to-trisoup */
		if( trisoup ){
			if( bakDrawSurfaces.empty() ){ // backup if not yet
				bakDrawSurfaces = bspDrawSurfaces;
				bakDrawSurfaces.swap( bspDrawSurfaces );
			}
			for( auto& ds : bspDrawSurfaces ){
				if( ds.surfaceType == MST_PLANAR ){
					ds.surfaceType = MST_TRIANGLE_SOUP;
					ds.lightmapNum[ 0 ] = -3;
				}
			}
		}

		WriteBSPFile( bspFileName );
		// restore original bsp lumps
		bakDrawSurfaces.swap( bspDrawSurfaces );
		if( !bakModels.empty() || !bakLeafSurfaces.empty() || !bakBrushSides.empty() ){
			bakModels.swap( bspModels );
			bakLeafSurfaces.swap( bspLeafSurfaces );
			bakBrushSides.swap( bspBrushSides );
		}
	}
	else{ // no hacks to apply 😵
		WriteBSPFile( bspFileName );
	}
}



/*
   LightWorld()
   does what it says...
 */

static void LightWorld( bool fastAllocate, bool bounceStore ){
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
		RunThreadsOnIndividual( rawGridPoints.size(), true, TraceGrid );
		inGrid = false;
		Sys_Printf( "%d x %d x %d = %zu grid\n",
		            gridBounds[ 0 ], gridBounds[ 1 ], gridBounds[ 2 ], bspGridPoints.size() );

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
	RunThreadsOnIndividual( bspDrawSurfaces.size(), true, IlluminateVertexes );
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
		StoreSurfaceLightmaps( fastAllocate, bounceStore );
		if( bounceStore ){
			UnparseEntities();
			WriteBSPFileAfterLight( source );
		}

		/* note it */
		Sys_Printf( "\n--- Radiosity (bounce %d of %d) ---\n", b, bt );

		/* flag bouncing */
		bouncing = true;
		ambientColor.set( 0 );
		floodlighty = false;

		/* delete any existing lights, freeing up memory for the next bounce */
		lights.clear();
		/* generate diffuse lights */
		RadCreateDiffuseLights();

		/* setup light envelopes */
		SetupEnvelopes( false, fastbounce );
		if ( lights.empty() ) {
			Sys_Printf( "No diffuse light to calculate, ending radiosity.\n" );
			if( bounceStore ){ // already stored, just quit
				return;
			}
			break; // break to StoreSurfaceLightmaps
		}

		/* add to lightgrid */
		if ( bouncegrid ) {
			gridEnvelopeCulled = 0;
			gridBoundsCulled = 0;

			Sys_Printf( "--- BounceGrid ---\n" );
			inGrid = true;
			RunThreadsOnIndividual( rawGridPoints.size(), true, TraceGrid );
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
		RunThreadsOnIndividual( bspDrawSurfaces.size(), true, IlluminateVertexes );
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

	/* ydnar: store off lightmaps */
	StoreSurfaceLightmaps( fastAllocate, true );

	/* write out the bsp */
	UnparseEntities();
	WriteBSPFileAfterLight( source );
}



/*
   LightMain()
   main routine for light processing
 */

int LightMain( Args& args ){
	float f;
	int lightmapMergeSize = 0;
	bool lightSamplesInsist = false;
	bool fastAllocate = true;
	bool bounceStore = true;


	/* note it */
	Sys_Printf( "--- Light ---\n" );
	Sys_Printf( "--- ProcessGameSpecific ---\n" );

	/* set standard game flags */
	wolfLight = g_game->wolfLight;
	if ( wolfLight ) {
		Sys_Printf( " lightning model: wolf\n" );
	}
	else{
		Sys_Printf( " lightning model: quake3\n" );
	}

	lmCustomSizeW = lmCustomSizeH = g_game->lightmapSize;
	Sys_Printf( " lightmap size: %d x %d pixels\n", lmCustomSizeW, lmCustomSizeH );

	lightmapGamma = g_game->lightmapGamma;
	Sys_Printf( " lightning gamma: %f\n", lightmapGamma );

	lightmapsRGB = g_game->lightmapsRGB;
	if ( lightmapsRGB ) {
		Sys_Printf( " lightmap colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " lightmap colorspace: linear\n" );
	}

	texturesRGB = g_game->texturesRGB;
	if ( texturesRGB ) {
		Sys_Printf( " texture colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " texture colorspace: linear\n" );
	}

	colorsRGB = g_game->colorsRGB;
	if ( colorsRGB ) {
		Sys_Printf( " _color colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " _color colorspace: linear\n" );
	}

	lightmapCompensate = g_game->lightmapCompensate;
	Sys_Printf( " lightning compensation: %f\n", lightmapCompensate );

	lightmapExposure = g_game->lightmapExposure;
	Sys_Printf( " lightning exposure: %f\n", lightmapExposure );

	gridScale = g_game->gridScale;
	Sys_Printf( " lightgrid scale: %f\n", gridScale );

	gridAmbientScale = g_game->gridAmbientScale;
	Sys_Printf( " lightgrid ambient scale: %f\n", gridAmbientScale );

	lightAngleHL = g_game->lightAngleHL;
	if ( lightAngleHL ) {
		Sys_Printf( " half lambert light angle attenuation enabled \n" );
	}

	noStyles = g_game->noStyles;
	if ( noStyles ) {
		Sys_Printf( " shader lightstyles hack: disabled\n" );
	}
	else{
		Sys_Printf( " shader lightstyles hack: enabled\n" );
	}

	patchShadows = g_game->patchShadows;
	if ( patchShadows ) {
		Sys_Printf( " patch shadows: enabled\n" );
	}
	else{
		Sys_Printf( " patch shadows: disabled\n" );
	}

	deluxemap = g_game->deluxeMap;
	deluxemode = g_game->deluxeMode;
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
	const char *fileName = args.takeBack();
	const auto argsToInject = args.getVector();
	{
		/* lightsource scaling */
		while ( args.takeArg( "-point", "-pointscale" ) ) {
			f = atof( args.takeNext() );
			pointScale *= f;
			spotScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
		}

		while ( args.takeArg( "-spherical", "-sphericalscale" ) ) {
			f = atof( args.takeNext() );
			pointScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
		}

		while ( args.takeArg( "-spot", "-spotscale" ) ) {
			f = atof( args.takeNext() );
			spotScale *= f;
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
		}

		while ( args.takeArg( "-area", "-areascale" ) ) {
			f = atof( args.takeNext() );
			areaScale *= f;
			Sys_Printf( "Area (shader) light scaled by %f to %f\n", f, areaScale );
		}

		while ( args.takeArg( "-sky", "-skyscale" ) ) {
			f = atof( args.takeNext() );
			skyScale *= f;
			Sys_Printf( "Sky/sun light scaled by %f to %f\n", f, skyScale );
		}

		while ( args.takeArg( "-vertexscale" ) ) {
			f = atof( args.takeNext() );
			vertexglobalscale *= f;
			Sys_Printf( "Vertexlight scaled by %f to %f\n", f, vertexglobalscale );
		}

		while ( args.takeArg( "-backsplash" ) ) {
			g_backsplashFractionScale = atof( args.takeNext() );
			Sys_Printf( "Area lights backsplash fraction scaled by %f\n", g_backsplashFractionScale );
			f = atof( args.takeNext() );
			if ( f >= -900.0f ){
				g_backsplashDistance = f;
				Sys_Printf( "Area lights backsplash distance set globally to %f\n", g_backsplashDistance );
			}
		}

		while ( args.takeArg( "-nolm" ) ) {
			noLightmaps = true;
			Sys_Printf( "No lightmaps yo\n" );
		}

		while ( args.takeArg( "-bouncecolorratio" ) ) {
			bounceColorRatio = std::clamp( atof( args.takeNext() ), 0.0, 1.0 );
			Sys_Printf( "Bounce color ratio set to %f\n", bounceColorRatio );
		}

		while ( args.takeArg( "-bouncescale" ) ) {
			f = atof( args.takeNext() );
			bounceScale *= f;
			Sys_Printf( "Bounce (radiosity) light scaled by %f to %f\n", f, bounceScale );
		}

		while ( args.takeArg( "-scale" ) ) {
			f = atof( args.takeNext() );
			pointScale *= f;
			spotScale *= f;
			areaScale *= f;
			skyScale *= f;
			Sys_Printf( "All light scaled by %f\n", f );
		}

		while ( args.takeArg( "-gridscale" ) ) {
			f = atof( args.takeNext() );
			Sys_Printf( "Grid lightning scaled by %f\n", f );
			gridScale *= f;
		}

		while ( args.takeArg( "-gridambientscale" ) ) {
			f = atof( args.takeNext() );
			Sys_Printf( "Grid ambient lightning scaled by %f\n", f );
			gridAmbientScale *= f;
		}

		while ( args.takeArg( "-griddirectionality" ) ) {
			gridDirectionality = std::min( 1.0, atof( args.takeNext() ) );
			value_minimize( gridAmbientDirectionality, gridDirectionality );
			Sys_Printf( "Grid directionality is %f\n", gridDirectionality );
		}

		while ( args.takeArg( "-gridambientdirectionality" ) ) {
			gridAmbientDirectionality = std::max( -1.0, atof( args.takeNext() ) );
			value_maximize( gridDirectionality, gridAmbientDirectionality );
			Sys_Printf( "Grid ambient directionality is %f\n", gridAmbientDirectionality );
		}

		while ( args.takeArg( "-gamma" ) ) {
			f = atof( args.takeNext() );
			lightmapGamma = f;
			Sys_Printf( "Lighting gamma set to %f\n", lightmapGamma );
		}

		while ( args.takeArg( "-sRGBlight" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
		}

		while ( args.takeArg( "-nosRGBlight" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
		}

		while ( args.takeArg( "-sRGBtex" ) ) {
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
		}

		while ( args.takeArg( "-nosRGBtex" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
		}

		while ( args.takeArg( "-sRGBcolor" ) ) {
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		while ( args.takeArg( "-nosRGBcolor" ) ) {
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		while ( args.takeArg( "-sRGB" ) ) {
			lightmapsRGB = true;
			Sys_Printf( "Lighting is in sRGB\n" );
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		while ( args.takeArg( "-nosRGB" ) ) {
			lightmapsRGB = false;
			Sys_Printf( "Lighting is linear\n" );
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}

		while ( args.takeArg( "-exposure" ) ) {
			f = atof( args.takeNext() );
			lightmapExposure = f;
			Sys_Printf( "Lighting exposure set to %f\n", lightmapExposure );
		}

		while ( args.takeArg( "-compensate" ) ) {
			f = atof( args.takeNext() );
			if ( f <= 0.0f ) {
				f = 1.0f;
			}
			lightmapCompensate = f;
			Sys_Printf( "Lighting compensation set to 1/%f\n", lightmapCompensate );
		}

		/* Lightmaps brightness */
		while ( args.takeArg( "-brightness" ) ){
			lightmapBrightness = atof( args.takeNext() );
			Sys_Printf( "Scaling lightmaps brightness by %f\n", lightmapBrightness );
		}

		/* Lighting contrast */
		while ( args.takeArg( "-contrast" ) ){
			lightmapContrast = std::clamp( atof( args.takeNext() ), -255.0, 255.0 );
			Sys_Printf( "Lighting contrast set to %f\n", lightmapContrast );
			/* change to factor in range of 0 to 129.5 */
			lightmapContrast = ( 259 * ( lightmapContrast + 255 ) ) / ( 255 * ( 259 - lightmapContrast ) );
		}

		/* Lighting saturation */
		while ( args.takeArg( "-saturation" ) ){
			g_lightmapSaturation = atof( args.takeNext() );
			Sys_Printf( "Lighting saturation set to %f\n", g_lightmapSaturation );
		}

		/* ydnar switches */
		while ( args.takeArg( "-bounce" ) ) {
			bounce = std::max( 0, atoi( args.takeNext() ) );
			if ( bounce > 0 ) {
				Sys_Printf( "Radiosity enabled with %d bounce(s)\n", bounce );
			}
		}

		while ( args.takeArg( "-supersample", "-super" ) ) {
			superSample = std::max( 1, atoi( args.takeNext() ) );
			if ( superSample > 1 ) {
				Sys_Printf( "Ordered-grid supersampling enabled with %d sample(s) per lightmap texel\n", ( superSample * superSample ) );
			}
		}

		while ( args.takeArg( "-randomsamples" ) ) {
			lightRandomSamples = true;
			Sys_Printf( "Random sampling enabled\n", lightRandomSamples );
		}

		while ( args.takeArg( "-samples" ) ) {
			const char *arg = args.takeNext();
			lightSamplesInsist = ( *arg == '+' );
			lightSamples = std::max( 1, atoi( arg ) );
			if ( lightSamples > 1 ) {
				Sys_Printf( "Adaptive supersampling enabled with %d sample(s) per lightmap texel\n", lightSamples );
			}
		}

		while ( args.takeArg( "-samplessearchboxsize" ) ) {
			lightSamplesSearchBoxSize = std::clamp( atoi( args.takeNext() ), 1, 4 ); /* more makes no sense */
			if ( lightSamplesSearchBoxSize != 1 )
				Sys_Printf( "Adaptive supersampling uses %f times the normal search box size\n", lightSamplesSearchBoxSize );
		}

		while ( args.takeArg( "-filter" ) ) {
			filter = true;
			Sys_Printf( "Lightmap filtering enabled\n" );
		}

		while ( args.takeArg( "-dark" ) ) {
			dark = true;
			Sys_Printf( "Dark lightmap seams enabled\n" );
		}

		while ( args.takeArg( "-shadeangle" ) ) {
			shadeAngleDegrees = std::max( 0.0, atof( args.takeNext() ) );
			if ( shadeAngleDegrees > 0.0f ) {
				shade = true;
				Sys_Printf( "Phong shading enabled with a breaking angle of %f degrees\n", shadeAngleDegrees );
			}
		}

		while ( args.takeArg( "-thresh" ) ) {
			subdivideThreshold = atof( args.takeNext() );
			if ( subdivideThreshold < 0 ) {
				subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;
			}
			else{
				Sys_Printf( "Subdivision threshold set at %.3f\n", subdivideThreshold );
			}
		}

		while ( args.takeArg( "-approx" ) ) {
			approximateTolerance = std::max( 0, atoi( args.takeNext() ) );
			if ( approximateTolerance > 0 ) {
				Sys_Printf( "Approximating lightmaps within a byte tolerance of %d\n", approximateTolerance );
			}
		}
		while ( args.takeArg( "-deluxe", "-deluxemap" ) ) {
			deluxemap = true;
			Sys_Printf( "Generating deluxemaps for average light direction\n" );
		}
		while ( args.takeArg( "-deluxemode" ) ) {
			deluxemode = atoi( args.takeNext() );
			if ( deluxemode != 1 ) {
				Sys_Printf( "Generating modelspace deluxemaps\n" );
				deluxemode = 0;
			}
			else{
				Sys_Printf( "Generating tangentspace deluxemaps\n" );
			}
		}
		while ( args.takeArg( "-nodeluxe", "-nodeluxemap" ) ) {
			deluxemap = false;
			Sys_Printf( "Disabling generating of deluxemaps for average light direction\n" );
		}
		while ( args.takeArg( "-external" ) ) {
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		bool extlmhack = false;
		while ( args.takeArg( "-lightmapsize" )
		       || ( extlmhack = args.takeArg( "-extlmhacksize" ) ) ) {

			lmCustomSizeW = lmCustomSizeH = atoi( args.takeNext() );
			if( args.nextAvailable() && 0 != atoi( args.next() ) ){ // optional second dimension
				lmCustomSizeH = atoi( args.takeNext() );
			}
			/* must be a power of 2 and greater than 2 */
			if ( ( ( lmCustomSizeW - 1 ) & lmCustomSizeW ) || lmCustomSizeW < 2 ||
			     ( ( lmCustomSizeH - 1 ) & lmCustomSizeH ) || lmCustomSizeH < 2 ) {
				Sys_Warning( "Lightmap size must be a power of 2, greater or equal to 2 pixels.\n" );
				lmCustomSizeW = lmCustomSizeH = g_game->lightmapSize;
			}
			Sys_Printf( "Default lightmap size set to %d x %d pixels\n", lmCustomSizeW, lmCustomSizeH );

			/* enable external lightmaps */
			if ( lmCustomSizeW != g_game->lightmapSize || lmCustomSizeH != g_game->lightmapSize ) {
				/* -lightmapsize might just require -external for native external lms, but it has already been used in existing batches alone,
				so brand new switch here for external lms, referenced by shaders hack/behavior */
				externalLightmaps = !extlmhack;
				Sys_Printf( "Storing all lightmaps externally\n" );
			}
		}

		while ( args.takeArg( "-rawlightmapsizelimit" ) ) {
			lmLimitSize = atoi( args.takeNext() );
			Sys_Printf( "Raw lightmap size limit set to %d x %d pixels\n", lmLimitSize, lmLimitSize );
		}

		while ( args.takeArg( "-lightmapdir" ) ) {
			lmCustomDir = args.takeNext();
			Sys_Printf( "Lightmap directory set to %s\n", lmCustomDir );
			externalLightmaps = true;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		/* ydnar: add this to suppress warnings */
		while ( args.takeArg( "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}

		while ( args.takeArg( "-wolf" ) ) {
			/* -game should already be set */
			wolfLight = true;
			Sys_Printf( "Enabling Wolf lighting model (linear default)\n" );
		}

		while ( args.takeArg( "-q3" ) ) {
			/* -game should already be set */
			wolfLight = false;
			Sys_Printf( "Enabling Quake 3 lighting model (nonlinear default)\n" );
		}

		while ( args.takeArg( "-extradist" ) ) {
			extraDist = std::max( 0.0, atof( args.takeNext() ) );
			Sys_Printf( "Default extra radius set to %f units\n", extraDist );
		}

		while ( args.takeArg( "-sunonly" ) ) {
			sunOnly = true;
			Sys_Printf( "Only computing sunlight\n" );
		}

		while ( args.takeArg( "-bounceonly" ) ) {
			bounceOnly = true;
			Sys_Printf( "Storing bounced light (radiosity) only\n" );
		}

		while ( args.takeArg( "-nobouncestore" ) ) {
			bounceStore = false;
			Sys_Printf( "Not storing BSP, lightmap and shader files between bounces\n" );
		}

		while ( args.takeArg( "-nocollapse" ) ) {
			noCollapse = true;
			Sys_Printf( "Identical lightmap collapsing disabled\n" );
		}

		while ( args.takeArg( "-nolightmapsearch" ) ) {
			lightmapSearchBlockSize = 1;
			Sys_Printf( "No lightmap searching - all lightmaps will be sequential\n" );
		}

		while ( args.takeArg( "-lightmapsearchpower" ) ) {
			const int power = atoi( args.takeNext() );
			lightmapMergeSize = ( g_game->lightmapSize << power );
			Sys_Printf( "Restricted lightmap searching enabled - optimize for lightmap merge power %d (size %d)\n", power, lightmapMergeSize );
		}

		while ( args.takeArg( "-lightmapsearchblocksize" ) ) {
			lightmapSearchBlockSize = atoi( args.takeNext() );
			Sys_Printf( "Restricted lightmap searching enabled - block size set to %d\n", lightmapSearchBlockSize );
		}

		while ( args.takeArg( "-shade" ) ) {
			shade = true;
			Sys_Printf( "Phong shading enabled\n" );
		}

		while ( args.takeArg( "-bouncegrid" ) ) {
			bouncegrid = true;
			if ( bounce > 0 ) {
				Sys_Printf( "Grid lighting with radiosity enabled\n" );
			}
		}

		while ( args.takeArg( "-nofastpoint" ) ) {
			fastpoint = false;
			Sys_Printf( "Automatic fast mode for point lights disabled\n" );
		}

		while ( args.takeArg( "-fast" ) ) {
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Fast mode enabled for all area lights\n" );
		}

		while ( args.takeArg( "-faster" ) ) {
			faster = true;
			fast = true;
			fastgrid = true;
			fastbounce = true;
			Sys_Printf( "Faster mode enabled\n" );
		}

		while ( args.takeArg( "-slowallocate" ) ) {
			fastAllocate = false;
			Sys_Printf( "Slow allocation mode enabled\n" );
		}

		while ( args.takeArg( "-fastgrid" ) ) {
			fastgrid = true;
			Sys_Printf( "Fast grid lighting enabled\n" );
		}

		while ( args.takeArg( "-fastbounce" ) ) {
			fastbounce = true;
			Sys_Printf( "Fast bounce mode enabled\n" );
		}

		while ( args.takeArg( "-cheap" ) ) {
			cheap = true;
			cheapgrid = true;
			Sys_Printf( "Cheap mode enabled\n" );
		}

		while ( args.takeArg( "-cheapgrid" ) ) {
			cheapgrid = true;
			Sys_Printf( "Cheap grid mode enabled\n" );
		}

		while ( args.takeArg( "-normalmap" ) ) {
			normalmap = true;
			Sys_Printf( "Storing normal map instead of lightmap\n" );
		}

		while ( args.takeArg( "-trisoup" ) ) {
			trisoup = true;
			Sys_Printf( "Converting brush faces to triangle soup\n" );
		}

		while ( args.takeArg( "-debug" ) ) {
			debug = true;
			Sys_Printf( "Lightmap debugging enabled\n" );
		}

		while ( args.takeArg( "-debugsurfaces", "-debugsurface" ) ) {
			debugSurfaces = true;
			Sys_Printf( "Lightmap surface debugging enabled\n" );
		}

		while ( args.takeArg( "-debugaxis" ) ) {
			debugAxis = true;
			Sys_Printf( "Lightmap axis debugging enabled\n" );
		}

		while ( args.takeArg( "-debugcluster" ) ) {
			debugCluster = true;
			Sys_Printf( "Luxel cluster debugging enabled\n" );
		}

		while ( args.takeArg( "-debugorigin" ) ) {
			debugOrigin = true;
			Sys_Printf( "Luxel origin debugging enabled\n" );
		}

		while ( args.takeArg( "-debugdeluxe" ) ) {
			deluxemap = true;
			debugDeluxemap = true;
			Sys_Printf( "Deluxemap debugging enabled\n" );
		}

		while ( args.takeArg( "-export" ) ) {
			exportLightmaps = true;
			Sys_Printf( "Exporting lightmaps\n" );
		}

		while ( args.takeArg( "-notrace" ) ) {
			noTrace = true;
			Sys_Printf( "Shadow occlusion disabled\n" );
		}
		while ( args.takeArg( "-patchshadows" ) ) {
			patchShadows = true;
			Sys_Printf( "Patch shadow casting enabled\n" );
		}
		while ( args.takeArg( "-samplesize" ) ) {
			sampleSize = std::max( 1, atoi( args.takeNext() ) );
			Sys_Printf( "Default lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		while ( args.takeArg( "-minsamplesize" ) ) {
			minSampleSize = std::max( 1, atoi( args.takeNext() ) );
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		while ( args.takeArg(  "-samplescale" ) ) {
			sampleScale = atoi( args.takeNext() );
			Sys_Printf( "Lightmaps sample scale set to %d\n", sampleScale );
		}
		while ( args.takeArg(  "-debugsamplesize" ) ) {
			debugSampleSize = 1;
			Sys_Printf( "debugging Lightmaps SampleSize\n" );
		}
		while ( args.takeArg( "-novertex" ) ) {
			if ( args.nextAvailable() && atof( args.next() ) != 0 ) { /* optional value to set */
				noVertexLighting = std::clamp( atof( args.takeNext() ), 0.0, 1.0 );
				Sys_Printf( "Setting vertex lighting globally to %f\n", noVertexLighting );
			}
			else{
				noVertexLighting = 1;
				Sys_Printf( "Disabling vertex lighting\n" );
			}
		}
		while ( args.takeArg( "-nogrid" ) ) {
			noGridLighting = true;
			Sys_Printf( "Disabling grid lighting\n" );
		}
		while ( args.takeArg( "-border" ) ) {
			lightmapBorder = true;
			Sys_Printf( "Adding debug border to lightmaps\n" );
		}
		while ( args.takeArg( "-nosurf" ) ) {
			noSurfaces = true;
			Sys_Printf( "Not tracing against surfaces\n" );
		}
		while ( args.takeArg( "-dump" ) ) {
			dump = true;
			Sys_Printf( "Dumping radiosity lights into numbered prefabs\n" );
		}
		while ( args.takeArg( "-lomem" ) ) {
			loMem = true;
			Sys_Printf( "Enabling low-memory (potentially slower) lighting mode\n" );
		}
		while ( args.takeArg( "-lightanglehl" ) ) {
			const bool enable = ( atoi( args.takeNext() ) != 0 );
			if ( enable != lightAngleHL ) {
				lightAngleHL = enable;
				if ( lightAngleHL ) {
					Sys_Printf( "Enabling half lambert light angle attenuation\n" );
				}
				else{
					Sys_Printf( "Disabling half lambert light angle attenuation\n" );
				}
			}
		}
		while ( args.takeArg( "-nostyle", "-nostyles" ) ) {
			noStyles = true;
			Sys_Printf( "Disabling lightstyles\n" );
		}
		while ( args.takeArg( "-style", "-styles" ) ) {
			noStyles = false;
			Sys_Printf( "Enabling lightstyles\n" );
		}
		while ( args.takeArg( "-cpma" ) ) {
			cpmaHack = true;
			Sys_Printf( "Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
		}
		while ( args.takeArg( "-floodlight" ) ) {
			floodlighty = true;
			Sys_Printf( "FloodLighting enabled\n" );
		}
		while ( args.takeArg( "-debugnormals" ) ) {
			debugnormals = true;
			Sys_Printf( "DebugNormals enabled\n" );
		}
		while ( args.takeArg( "-lowquality" ) ) {
			floodlight_lowquality = true;
			Sys_Printf( "Low Quality FloodLighting enabled\n" );
		}

		/* r7: dirtmapping */
		while ( args.takeArg( "-dirty" ) ) {
			dirty = true;
			Sys_Printf( "Dirtmapping enabled\n" );
		}
		while ( args.takeArg( "-dirtdebug", "-debugdirt" ) ) {
			dirtDebug = true;
			Sys_Printf( "Dirtmap debugging enabled\n" );
		}
		while ( args.takeArg( "-dirtmode" ) ) {
			dirtMode = atoi( args.takeNext() );
			if ( dirtMode != 0 && dirtMode != 1 ) {
				dirtMode = 0;
			}
			if ( dirtMode == 1 ) {
				Sys_Printf( "Enabling randomized dirtmapping\n" );
			}
			else{
				Sys_Printf( "Enabling ordered dirtmapping\n" );
			}
		}
		while ( args.takeArg( "-dirtdepth" ) ) {
			dirtDepth = atof( args.takeNext() );
			if ( dirtDepth <= 0.0f ) {
				dirtDepth = 128.0f;
			}
			Sys_Printf( "Dirtmapping depth set to %.1f\n", dirtDepth );
		}
		while ( args.takeArg( "-dirtscale" ) ) {
			dirtScale = atof( args.takeNext() );
			if ( dirtScale <= 0.0f ) {
				dirtScale = 1.0f;
			}
			Sys_Printf( "Dirtmapping scale set to %.1f\n", dirtScale );
		}
		while ( args.takeArg( "-dirtgain" ) ) {
			dirtGain = atof( args.takeNext() );
			if ( dirtGain <= 0.0f ) {
				dirtGain = 1.0f;
			}
			Sys_Printf( "Dirtmapping gain set to %.1f\n", dirtGain );
		}
		while ( args.takeArg( "-trianglecheck" ) ) {
			lightmapTriangleCheck = true;
		}
		while ( args.takeArg( "-extravisnudge" ) ) {
			lightmapExtraVisClusterNudge = true;
		}
		while ( args.takeArg( "-fill" ) ) {
			lightmapFill = true;
			Sys_Printf( "Filling lightmap colors from surrounding pixels to improve JPEG compression\n" );
		}
		while ( args.takeArg( "-fillpink" ) ) {
			lightmapPink = true;
		}
		/* unhandled args */
		while( !args.empty() )
		{
			Sys_Warning( "Unknown argument \"%s\"\n", args.takeFront() );
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
	strcpy( source, ExpandArg( fileName ) );
	path_set_extension( source, ".bsp" );

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
	InjectCommandLine( "-light", argsToInject );

	/* load map file */
	if ( !entities[ 0 ].boolForKey( "_keepLights" ) ) {
		char *mapFileName = ExpandArg( fileName );
		if ( !path_extension_is( fileName, "reg" ) ) /* not .reg */
			path_set_extension( mapFileName, ".map" );

		LoadMapFile( CopiedString( mapFileName ).c_str(), true, false );
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
	LightWorld( fastAllocate, bounceStore );

	/* ydnar: export lightmaps */
	if ( exportLightmaps && !externalLightmaps ) {
		ExportLightmaps();
	}

	/* return to sender */
	return 0;
}
