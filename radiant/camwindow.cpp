/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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
 */

//
// Camera Window
//
// Leonardo Zide (leo@lokigames.com)
//

#include "camwindow.h"

#include "debugging/debugging.h"

#include "iscenegraph.h"
#include "irender.h"
#include "igl.h"
#include "icamera.h"
#include "cullable.h"
#include "renderable.h"
#include "preferencesystem.h"

#include "signal/signal.h"
#include "container/array.h"
#include "scenelib.h"
#include "render.h"
#include "cmdlib.h"
#include "math/frustum.h"

#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/xorrectangle.h"
#include "gtkmisc.h"
#include "selection.h"
#include "mainframe.h"
#include "preferences.h"
#include "commands.h"
#include "xywindow.h"
#include "windowobservers.h"
#include "renderstate.h"

#include "timer.h"

Signal0 g_cameraMoved_callbacks;

void AddCameraMovedCallback( const SignalHandler& handler ){
	g_cameraMoved_callbacks.connectLast( handler );
}

void CameraMovedNotify(){
	g_cameraMoved_callbacks();
}


struct camwindow_globals_private_t
{
	int m_nMoveSpeed;
	int m_time_toMaxSpeed;
	int m_nScrollMoveSpeed;
	float m_strafeSpeed;
	float m_angleSpeed;
	bool m_bCamInverseMouse;
	bool m_bCamDiscrete;
	bool m_bCubicClipping;
	int m_strafeMode;
	bool m_bFaceWire;
	bool m_bFaceFill;
	int m_MSAA;
	bool m_bShowWorkzone;
	bool m_bShowSize;

	camwindow_globals_private_t() :
		m_nMoveSpeed( 500 ),
		m_time_toMaxSpeed( 200 ),
		m_nScrollMoveSpeed( 100 ),
		m_strafeSpeed( 1.f ),
		m_angleSpeed( 9.f ),
		m_bCamInverseMouse( false ),
		m_bCamDiscrete( true ),
		m_bCubicClipping( false ),
		m_strafeMode( 3 ),
		m_bFaceWire( true ),
		m_bFaceFill( true ),
		m_MSAA( 8 ),
		m_bShowWorkzone( true ),
		m_bShowSize( true ){
	}

};

camwindow_globals_private_t g_camwindow_globals_private;


const Matrix4 g_opengl2radiant(
	 0, 0,-1, 0,
	-1, 0, 0, 0,
	 0, 1, 0, 0,
	 0, 0, 0, 1
	);

const Matrix4 g_radiant2opengl(
	 0,-1, 0, 0,
	 0, 0, 1, 0,
	-1, 0, 0, 0,
	 0, 0, 0, 1
	);

struct camera_t;
void Camera_mouseMove( camera_t& camera, int x, int y, unsigned int state );

struct MotionDeltaValues {
	int x;
	int y;
	unsigned int state;
	MotionDeltaValues( int x_, int y_, unsigned int state_ ):
		x( x_ ), y( y_ ), state( state_ ) {
	}
};

enum camera_draw_mode
{
	cd_wire = 0,
	cd_solid = 1,
	cd_texture = 2,
	cd_texture_plus_wire = 3,
	cd_lighting = 4
};
const int camera_draw_mode_count = 5;

struct camera_t
{
	int width, height;

	bool timing;

	Vector3 origin;
	Vector3 angles;

	Vector3 color; // background

	Vector3 forward, right; // move matrix (TTimo: used to have up but it was not updated)
	Vector3 vup, vpn, vright; // view matrix (taken from the modelview matrix)

	Matrix4 projection;
	Matrix4 modelview;

	bool m_strafe; // true when in strafemode toggled by the ctrl-key
	bool m_strafe_forward; // true when in strafemode by ctrl-key and shift is pressed for forward strafing
	bool m_strafe_forward_invert; //silly option to invert forward strafing to support old fegs
	bool m_orbit = false;
	Vector3 m_orbit_center;
	Vector3 m_orbit_initial_pos;
	int m_orbit_offset = 0;
	int m_focus_offset = 0;

	unsigned int movementflags; // movement flags
	Timer m_keycontrol_timer;
	guint m_keymove_handler;
	float m_keymove_speed_current;


	static float fieldOfView;
	static const float near_z;

	DeferredMotionDelta m_mouseMove;

	static void motionDelta( int x, int y, unsigned int state, void* data ){
		Camera_mouseMove( *reinterpret_cast<camera_t*>( data ), x, y, state );
	}

	View* m_view;
	Callback m_update;

	Callback1<const MotionDeltaValues&> m_update_motion_freemove;

	static camera_draw_mode draw_mode;

	camera_t( View* view, const Callback& update, const Callback1<const MotionDeltaValues&>& update_motion_freemove )
		: width( 0 ),
		height( 0 ),
		timing( false ),
		origin( 0, 0, 0 ),
		angles( 0, 0, 0 ),
		color( 0, 0, 0 ),
		movementflags( 0 ),
		m_keymove_handler( 0 ),
		m_keymove_speed_current( 0.f ),
		m_mouseMove( motionDelta, this ),
		m_view( view ),
		m_update( update ),
		m_update_motion_freemove( update_motion_freemove ){
	}
};

float camera_t::fieldOfView = 100.0f;
const float camera_t::near_z = 1.f;
camera_draw_mode camera_t::draw_mode = cd_texture;

inline Matrix4 projection_for_camera( float near_z, float far_z, float fieldOfView, int width, int height ){
	float half_width = static_cast<float>( near_z * tan( degrees_to_radians( fieldOfView * 0.5 ) ) );
	const bool swap = height > width;
	if( swap )
		std::swap( width, height );
	float half_height = half_width * ( static_cast<float>( height ) / static_cast<float>( width ) );
	if( swap )
		std::swap( half_width, half_height );

	return matrix4_frustum(
			   -half_width,
			   half_width,
			   -half_height,
			   half_height,
			   near_z,
			   far_z
			   );
}

float Camera_getFarClipPlane( camera_t& camera ){
	return ( g_camwindow_globals_private.m_bCubicClipping ) ? pow( 2.0, ( g_camwindow_globals.m_nCubicScale + 7 ) / 2.0 ) : ( ( g_MaxWorldCoord - g_MinWorldCoord ) * sqrt( 3 ) );
}

void Camera_updateProjection( camera_t& camera ){
	float farClip = Camera_getFarClipPlane( camera );
	//~near_z = farClip / 4096.0f
	camera.projection = projection_for_camera( camera_t::near_z, farClip, camera_t::fieldOfView, camera.width, camera.height );

	camera.m_view->Construct( camera.projection, camera.modelview, camera.width, camera.height );
}

void Camera_updateVectors( camera_t& camera ){
	for ( int i = 0 ; i < 3 ; i++ )
	{
		camera.vright[i] = camera.modelview[( i << 2 ) + 0];
		camera.vup[i] = camera.modelview[( i << 2 ) + 1];
		camera.vpn[i] = camera.modelview[( i << 2 ) + 2];
	}
}

void Camera_updateModelview( camera_t& camera ){
	camera.modelview = g_matrix4_identity;

	// roll, pitch, yaw
	Vector3 radiant_eulerXYZ( 0, -camera.angles[CAMERA_PITCH], camera.angles[CAMERA_YAW] );

	matrix4_translate_by_vec3( camera.modelview, camera.origin );
	matrix4_rotate_by_euler_xyz_degrees( camera.modelview, radiant_eulerXYZ );
	matrix4_multiply_by_matrix4( camera.modelview, g_radiant2opengl );
	matrix4_affine_invert( camera.modelview );

	Camera_updateVectors( camera );

	camera.m_view->Construct( camera.projection, camera.modelview, camera.width, camera.height );
}


void Camera_Move_updateAxes( camera_t& camera ){
	double ya = degrees_to_radians( camera.angles[CAMERA_YAW] );

	// the movement matrix is kept 2d
	camera.forward[0] = static_cast<float>( cos( ya ) );
	camera.forward[1] = static_cast<float>( sin( ya ) );
	camera.forward[2] = 0;
	camera.right[0] = camera.forward[1];
	camera.right[1] = -camera.forward[0];
}

void Camera_Freemove_updateAxes( camera_t& camera ){
	camera.right = camera.vright;
	camera.forward = vector3_negated( camera.vpn );
}

const Vector3& Camera_getOrigin( camera_t& camera ){
	return camera.origin;
}

void Camera_setOrigin( camera_t& camera, const Vector3& origin ){
	camera.origin = origin;
	Camera_updateModelview( camera );
	camera.m_update();
	CameraMovedNotify();
}

const Vector3& Camera_getAngles( camera_t& camera ){
	return camera.angles;
}

void Camera_setAngles( camera_t& camera, const Vector3& angles ){
	camera.angles = angles;
	Camera_updateModelview( camera );
	camera.m_update();
	CameraMovedNotify();
}


void Camera_FreeMove( camera_t& camera, int dx, int dy ){
	// free strafe mode, toggled by the ctrl key with optional shift for forward movement
	if ( camera.m_strafe ) {
		const float speed = g_camwindow_globals_private.m_strafeSpeed;
		camera.origin -= camera.vright * speed * dx;
		if ( camera.m_strafe_forward )
			camera.origin += camera.vpn * speed * dy * ( camera.m_strafe_forward_invert ? 1 : -1 );
		else
			camera.origin += camera.vup * speed * dy;
	}
	else // free rotation
	{
		const float dtime = 0.0333333f;
		Vector3 angles0( camera.angles );

		if ( g_camwindow_globals_private.m_bCamInverseMouse ) {
			camera.angles[CAMERA_PITCH] -= dy * dtime * g_camwindow_globals_private.m_angleSpeed;
		}
		else{
			camera.angles[CAMERA_PITCH] += dy * dtime * g_camwindow_globals_private.m_angleSpeed;
		}

		camera.angles[CAMERA_YAW] += dx * dtime * g_camwindow_globals_private.m_angleSpeed;

		if ( camera.angles[CAMERA_PITCH] > 90 ) {
			camera.angles[CAMERA_PITCH] = 90;
		}
		else if ( camera.angles[CAMERA_PITCH] < -90 ) {
			camera.angles[CAMERA_PITCH] = -90;
		}

		if ( camera.angles[CAMERA_YAW] >= 360 ) {
			camera.angles[CAMERA_YAW] -= 360;
		}
		else if ( camera.angles[CAMERA_YAW] <= 0 ) {
			camera.angles[CAMERA_YAW] += 360;
		}

		if( camera.m_orbit ){
#if 0
			const Vector3 radangles( euler_degrees_to_radians( camera.angles ) );
			const Vector3 viewdir(	cos( radangles[1] ) * cos( radangles[0] ),
									sin( radangles[1] ) * cos( radangles[0] ),
									sin( radangles[0] )
									);
			const float len = vector3_length( camera.origin - camera.m_orbit_center );
			camera.origin = camera.m_orbit_center - viewdir * len;
#else
			Matrix4 rot0 = matrix4_rotation_for_euler_xyz_degrees( Vector3( 0, -angles0[CAMERA_PITCH], angles0[CAMERA_YAW] ) );
			matrix4_affine_invert( rot0 );
			Matrix4 rot1 = matrix4_rotation_for_euler_xyz_degrees( Vector3( 0, -camera.angles[CAMERA_PITCH], camera.angles[CAMERA_YAW] ) );
			matrix4_multiply_by_matrix4( rot1, rot0 );

			Matrix4 mat = matrix4_translation_for_vec3( camera.m_orbit_center );
			matrix4_multiply_by_matrix4( mat, rot1 );
			matrix4_translate_by_vec3( mat, -camera.m_orbit_center );

			camera.origin = matrix4_transformed_point( mat, camera.origin );
#endif
		}
	}

	Camera_updateModelview( camera );
	Camera_Freemove_updateAxes( camera );
}

#if 0
void Cam_MouseControl( camera_t& camera, int x, int y ){
//	int xl, xh;
//	int yl, yh;
	float xf, yf;

	xf = (float)( x - camera.width / 2 ) / ( camera.width / 2 );
	yf = (float)( y - camera.height / 2 ) / ( camera.height / 2 );

//	xl = camera.width / 3;
//	xh = xl * 2;
//	yl = camera.height / 3;
//	yh = yl * 2;

	xf *= 1.0f - fabsf( yf );
	if ( xf < 0 ) {
		xf += 0.1f;
		if ( xf > 0 ) {
			xf = 0;
		}
	}
	else
	{
		xf -= 0.1f;
		if ( xf < 0 ) {
			xf = 0;
		}
	}

	vector3_add( camera.origin, vector3_scaled( camera.forward, yf * 0.02f * g_camwindow_globals_private.m_nMoveSpeed ) );
	camera.angles[CAMERA_YAW] += xf * -0.1f * g_camwindow_globals_private.m_angleSpeed;

	Camera_updateModelview( camera );
}
#endif // 0

void Camera_mouseMove( camera_t& camera, int x, int y, unsigned int state ){
	//globalOutputStream() << "mousemove... ";
	Camera_FreeMove( camera, -x, -y );
	camera.m_update_motion_freemove( MotionDeltaValues( x, y, state ) );
	camera.m_update();
	CameraMovedNotify();
}

const unsigned int MOVE_NONE = 0;
const unsigned int MOVE_FORWARD = 1 << 0;
const unsigned int MOVE_BACK = 1 << 1;
const unsigned int MOVE_ROTRIGHT = 1 << 2;
const unsigned int MOVE_ROTLEFT = 1 << 3;
const unsigned int MOVE_STRAFERIGHT = 1 << 4;
const unsigned int MOVE_STRAFELEFT = 1 << 5;
const unsigned int MOVE_UP = 1 << 6;
const unsigned int MOVE_DOWN = 1 << 7;
const unsigned int MOVE_PITCHUP = 1 << 8;
const unsigned int MOVE_PITCHDOWN = 1 << 9;
const unsigned int MOVE_FOCUS = 1 << 10;
const unsigned int MOVE_ALL = MOVE_FORWARD | MOVE_BACK | MOVE_ROTRIGHT | MOVE_ROTLEFT | MOVE_STRAFERIGHT | MOVE_STRAFELEFT | MOVE_UP | MOVE_DOWN | MOVE_PITCHUP | MOVE_PITCHDOWN | MOVE_FOCUS;

Vector3 Camera_getFocusPos( camera_t& camera );

void Cam_KeyControl( camera_t& camera, float dtime ){
	// Update angles
	const float dangle = 25.f * dtime * g_camwindow_globals_private.m_angleSpeed;
	if ( camera.movementflags & MOVE_ROTLEFT ) {
		camera.angles[CAMERA_YAW] += dangle;
	}
	if ( camera.movementflags & MOVE_ROTRIGHT ) {
		camera.angles[CAMERA_YAW] -= dangle;
	}
	if ( camera.movementflags & MOVE_PITCHUP ) {
		camera.angles[CAMERA_PITCH] = std::min( camera.angles[CAMERA_PITCH] + dangle, 90.f );
	}
	if ( camera.movementflags & MOVE_PITCHDOWN ) {
		camera.angles[CAMERA_PITCH] = std::max( camera.angles[CAMERA_PITCH] - dangle, -90.f );
	}

	Camera_updateModelview( camera );
	Camera_Freemove_updateAxes( camera );

	if( g_camwindow_globals_private.m_time_toMaxSpeed == 0 ){
		camera.m_keymove_speed_current = g_camwindow_globals_private.m_nMoveSpeed;
	}
	else{ /* accelerate */
		camera.m_keymove_speed_current = std::min( camera.m_keymove_speed_current
													+ g_camwindow_globals_private.m_nMoveSpeed * dtime
													/ g_camwindow_globals_private.m_time_toMaxSpeed * static_cast<float>( msec_per_sec ),
													static_cast<float>( g_camwindow_globals_private.m_nMoveSpeed ) );
	}
	const float dpos = dtime * camera.m_keymove_speed_current;
	// Update position
	if ( camera.movementflags & MOVE_FORWARD ) {
		camera.origin += camera.forward * dpos;
	}
	if ( camera.movementflags & MOVE_BACK ) {
		camera.origin -= camera.forward * dpos;
	}
	if ( camera.movementflags & MOVE_STRAFERIGHT ) {
		camera.origin += camera.right * dpos;
	}
	if ( camera.movementflags & MOVE_STRAFELEFT ) {
		camera.origin -= camera.right * dpos;
	}
	if ( camera.movementflags & MOVE_UP ) {
		camera.origin += g_vector3_axis_z * dpos;
	}
	if ( camera.movementflags & MOVE_DOWN ) {
		camera.origin -= g_vector3_axis_z * dpos;
	}
	if ( camera.movementflags & MOVE_FOCUS ) {
		camera.origin = Camera_getFocusPos( camera );
	}

	Camera_updateModelview( camera );
}

void Camera_keyMove( camera_t& camera ){
	camera.m_mouseMove.flush();

	//globalOutputStream() << "keymove... ";
	float time_seconds = camera.m_keycontrol_timer.elapsed_msec() / static_cast<float>( msec_per_sec );
	if( time_seconds == 0 ) /* some reasonable move at the very start */
		time_seconds = 0.008f;
	camera.m_keycontrol_timer.start();

	Cam_KeyControl( camera, time_seconds );

	camera.m_update();
	CameraMovedNotify();
}

gboolean camera_keymove( gpointer data ){
	Camera_keyMove( *reinterpret_cast<camera_t*>( data ) );
	return TRUE;
}

void Camera_setMovementFlags( camera_t& camera, unsigned int mask ){
	if ( ( ~camera.movementflags & mask ) != 0 && camera.movementflags == 0 ) {
		camera.m_keymove_handler = g_idle_add( camera_keymove, &camera );
		camera.m_keycontrol_timer.start();
		camera.m_keymove_speed_current = 0;
	}
	camera.movementflags |= mask;
}
void Camera_clearMovementFlags( camera_t& camera, unsigned int mask ){
	if ( ( camera.movementflags & ~mask ) == 0 && camera.movementflags != 0 ) {
		g_source_remove( camera.m_keymove_handler );
		camera.m_keymove_handler = 0;
	}
	camera.movementflags &= ~mask;
}

void Camera_MoveForward_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_FORWARD );
}
void Camera_MoveForward_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_FORWARD );
}
void Camera_MoveBack_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_BACK );
}
void Camera_MoveBack_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_BACK );
}

void Camera_MoveLeft_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_STRAFELEFT );
}
void Camera_MoveLeft_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_STRAFELEFT );
}
void Camera_MoveRight_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_STRAFERIGHT );
}
void Camera_MoveRight_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_STRAFERIGHT );
}

void Camera_MoveUp_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_UP );
}
void Camera_MoveUp_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_UP );
}
void Camera_MoveDown_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_DOWN );
}
void Camera_MoveDown_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_DOWN );
}

void Camera_RotateLeft_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_ROTLEFT );
}
void Camera_RotateLeft_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_ROTLEFT );
}
void Camera_RotateRight_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_ROTRIGHT );
}
void Camera_RotateRight_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_ROTRIGHT );
}

void Camera_PitchUp_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_PITCHUP );
}
void Camera_PitchUp_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_PITCHUP );
}
void Camera_PitchDown_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_PITCHDOWN );
}
void Camera_PitchDown_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_PITCHDOWN );
}

void Camera_Focus_KeyDown( camera_t& camera ){
	Camera_setMovementFlags( camera, MOVE_FOCUS );
	camera.m_focus_offset = 0;
}
void Camera_Focus_KeyUp( camera_t& camera ){
	Camera_clearMovementFlags( camera, MOVE_FOCUS );
	camera.m_focus_offset = 0;
}

typedef ReferenceCaller<camera_t, &Camera_MoveForward_KeyDown> FreeMoveCameraMoveForwardKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveForward_KeyUp> FreeMoveCameraMoveForwardKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveBack_KeyDown> FreeMoveCameraMoveBackKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveBack_KeyUp> FreeMoveCameraMoveBackKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveLeft_KeyDown> FreeMoveCameraMoveLeftKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveLeft_KeyUp> FreeMoveCameraMoveLeftKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveRight_KeyDown> FreeMoveCameraMoveRightKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveRight_KeyUp> FreeMoveCameraMoveRightKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveUp_KeyDown> FreeMoveCameraMoveUpKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveUp_KeyUp> FreeMoveCameraMoveUpKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveDown_KeyDown> FreeMoveCameraMoveDownKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveDown_KeyUp> FreeMoveCameraMoveDownKeyUpCaller;

typedef ReferenceCaller<camera_t, &Camera_Focus_KeyDown> FreeMoveCameraFocusKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_Focus_KeyUp> FreeMoveCameraFocusKeyUpCaller;

#define SPEED_MOVE 32
#define SPEED_TURN 22.5
#define CAM_MIN_SPEED 50
#define CAM_MAX_SPEED 9999
#define CAM_SPEED_STEP 250

void Camera_MoveForward_Discrete( camera_t& camera ){
	Camera_Move_updateAxes( camera );
	Camera_setOrigin( camera, vector3_added( Camera_getOrigin( camera ), vector3_scaled( camera.forward, SPEED_MOVE ) ) );
}
void Camera_MoveBack_Discrete( camera_t& camera ){
	Camera_Move_updateAxes( camera );
	Camera_setOrigin( camera, vector3_added( Camera_getOrigin( camera ), vector3_scaled( camera.forward, -SPEED_MOVE ) ) );
}

void Camera_MoveUp_Discrete( camera_t& camera ){
	Vector3 origin( Camera_getOrigin( camera ) );
	origin[2] += SPEED_MOVE;
	Camera_setOrigin( camera, origin );
}
void Camera_MoveDown_Discrete( camera_t& camera ){
	Vector3 origin( Camera_getOrigin( camera ) );
	origin[2] -= SPEED_MOVE;
	Camera_setOrigin( camera, origin );
}

void Camera_MoveLeft_Discrete( camera_t& camera ){
	Camera_Move_updateAxes( camera );
	Camera_setOrigin( camera, vector3_added( Camera_getOrigin( camera ), vector3_scaled( camera.right, -SPEED_MOVE ) ) );
}
void Camera_MoveRight_Discrete( camera_t& camera ){
	Camera_Move_updateAxes( camera );
	Camera_setOrigin( camera, vector3_added( Camera_getOrigin( camera ), vector3_scaled( camera.right, SPEED_MOVE ) ) );
}

void Camera_RotateLeft_Discrete( camera_t& camera ){
	Vector3 angles( Camera_getAngles( camera ) );
	angles[CAMERA_YAW] += SPEED_TURN;
	Camera_setAngles( camera, angles );
}
void Camera_RotateRight_Discrete( camera_t& camera ){
	Vector3 angles( Camera_getAngles( camera ) );
	angles[CAMERA_YAW] -= SPEED_TURN;
	Camera_setAngles( camera, angles );
}

void Camera_PitchUp_Discrete( camera_t& camera ){
	Vector3 angles( Camera_getAngles( camera ) );
	angles[CAMERA_PITCH] += SPEED_TURN;
	if ( angles[CAMERA_PITCH] > 90 ) {
		angles[CAMERA_PITCH] = 90;
	}
	Camera_setAngles( camera, angles );
}
void Camera_PitchDown_Discrete( camera_t& camera ){
	Vector3 angles( Camera_getAngles( camera ) );
	angles[CAMERA_PITCH] -= SPEED_TURN;
	if ( angles[CAMERA_PITCH] < -90 ) {
		angles[CAMERA_PITCH] = -90;
	}
	Camera_setAngles( camera, angles );
}


class RadiantCameraView : public CameraView
{
camera_t& m_camera;
View* m_view;
Callback m_update;
public:
RadiantCameraView( camera_t& camera, View* view, const Callback& update ) : m_camera( camera ), m_view( view ), m_update( update ){
}
void update(){
	m_view->Construct( m_camera.projection, m_camera.modelview, m_camera.width, m_camera.height );
	m_update();
}
void setModelview( const Matrix4& modelview ){
	m_camera.modelview = modelview;
	matrix4_multiply_by_matrix4( m_camera.modelview, g_radiant2opengl );
	matrix4_affine_invert( m_camera.modelview );
	Camera_updateVectors( m_camera );
	update();
}
void setFieldOfView( float fieldOfView ){
	float farClip = Camera_getFarClipPlane( m_camera );
	m_camera.projection = projection_for_camera( camera_t::near_z, farClip, fieldOfView, m_camera.width, m_camera.height );
	update();
}
};


void Camera_motionDelta( int x, int y, unsigned int state, void* data ){
	camera_t* cam = reinterpret_cast<camera_t*>( data );

	cam->m_mouseMove.motion_delta( x, y, state );

	cam->m_orbit = ( state & GDK_MOD1_MASK ) && ( state & GDK_BUTTON3_MASK );
	if( cam->m_orbit ){
		cam->m_strafe = false;
		return;
	}

	cam->m_strafe_forward_invert = false;

	switch ( g_camwindow_globals_private.m_strafeMode )
	{
	case 0:
		cam->m_strafe = false;
		break;
	case 1:
		cam->m_strafe = ( state & GDK_CONTROL_MASK ) || ( state & GDK_BUTTON3_MASK );
		cam->m_strafe_forward = false;
		break;
	case 2:
		cam->m_strafe = ( state & GDK_CONTROL_MASK ) || ( state & GDK_BUTTON3_MASK );
		cam->m_strafe_forward = true;
		break;
	case 4:
		cam->m_strafe_forward_invert = true; // fall through
	default: /* 3 & 4 */
		cam->m_strafe = ( state & GDK_CONTROL_MASK ) || ( state & GDK_BUTTON3_MASK ) || ( state & GDK_SHIFT_MASK );
		cam->m_strafe_forward = ( state & GDK_SHIFT_MASK ) != 0;
		break;
	}

	if( ( state & GDK_BUTTON1_MASK ) != 0 && g_camwindow_globals_private.m_strafeMode != 0 ){
		cam->m_strafe = true;
		cam->m_strafe_forward = false;
	}
}

#include "stream/stringstream.h"

class CamDrawSize
{
	Vector3 _extents;
	RenderTextLabel m_labels[3];
public:
	CamDrawSize() : _extents( -99.9f, -99.9f, -99.9f ){
	}
	void render( Renderer& renderer, Shader* shader, const View& view ){
		const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
		if( bounds.extents.x() != 0 || bounds.extents.y() != 0 || bounds.extents.z() != 0 ){
			renderer.SetState( shader, Renderer::eFullMaterials );

			Vector4 points[3] = { Vector4( bounds.origin - g_vector3_axes[1] * bounds.extents - g_vector3_axes[2] * bounds.extents, 1 ),
									Vector4( bounds.origin - g_vector3_axes[0] * bounds.extents - g_vector3_axes[2] * bounds.extents, 1 ),
									Vector4( bounds.origin - g_vector3_axes[0] * bounds.extents - g_vector3_axes[1] * bounds.extents, 1 ),
									};
			for( std::size_t i = 0; i < 3; ++i ){
				matrix4_transform_vector4( view.GetViewMatrix(), points[i] );
				points[i].x() /= points[i].w();
				points[i].y() /= points[i].w();
//				points[i].z() /= points[i].w();
				matrix4_transform_vector4( view.GetViewport(), points[i] );
			}

			for( std::size_t i = 0; i < 3; ++i ){
				if( points[i].w() > 0.005f ){
					updateTex( i, bounds.extents[i] );
					m_labels[i].screenPos.x() = points[i].x();
					m_labels[i].screenPos.y() = points[i].y();
					renderer.addRenderable( m_labels[i], g_matrix4_identity );
				}
			}
		}
	}
private:
	const Vector3 getColor( const std::size_t i ) const {
		switch ( i )
		{
		case 0:
			return g_xywindow_globals.AxisColorX;
		case 1:
			return g_xywindow_globals.AxisColorY;
		default:
			return Vector3( g_xywindow_globals.AxisColorZ.x(), 0.7f, g_xywindow_globals.AxisColorZ.z() ); //hack to make default blue visible better
		}
	}
	void updateTex( const std::size_t i, const float extent ){
		if( extent != _extents[i] ){
			_extents[i] = extent;
			m_labels[i].texFree();
			StringOutputStream stream( 16 );
			stream << ( extent * 2 );
			m_labels[i].texAlloc( stream.c_str(), getColor( i ) );
		}
	}
};

#include "grid.h"
class RenderableCamWorkzone : public OpenGLRenderable
{
	mutable Array<Vector3> m_verticesarr[3];
	mutable Array<GLboolean> m_edgearr[3];
	mutable Array<Colour4b> m_colorarr0[3];
	mutable Array<Colour4b> m_colorarr1[3];
public:
void render( RenderStateFlags state ) const {
	glEnableClientState( GL_EDGE_FLAG_ARRAY );

	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();

	for( std::size_t i = 0; i < 3; ++i ){
		const std::size_t i2 = ( i + 1 ) % 3;
		const std::size_t i3 = ( i + 2 ) % 3;
//		const Vector3 normal = g_vector3_axes[i];
		const float offset = 1024;
		std::vector<Vector3> points;
		points.reserve( 4 );
		points.push_back( bounds.origin + g_vector3_axes[i2] * bounds.extents + g_vector3_axes[i3] * bounds.extents );
		if( bounds.extents[i2] != 0 ){
			points.push_back( bounds.origin - g_vector3_axes[i2] * bounds.extents + g_vector3_axes[i3] * bounds.extents );
		}
		if( bounds.extents[i3] != 0 ){
			points.push_back( bounds.origin + g_vector3_axes[i2] * bounds.extents - g_vector3_axes[i3] * bounds.extents );
			if( bounds.extents[i2] != 0 ){
				points.push_back( bounds.origin - g_vector3_axes[i2] * bounds.extents - g_vector3_axes[i3] * bounds.extents );
			}
		}

		const float grid = GetGridSize();
		const std::size_t approx_count = ( std::max( 0.f, bounds.extents[i] ) + offset ) * 4 / grid + 8;

		Array<Vector3>& verticesarr( m_verticesarr[i] );
		Array<GLboolean>& edgearr( m_edgearr[i] );
		Array<Colour4b>& colorarr0( m_colorarr0[i] );
		Array<Colour4b>& colorarr1( m_colorarr1[i] );
		if( verticesarr.size() < approx_count ){
			verticesarr.resize( approx_count );
			edgearr.resize( approx_count );
			colorarr0.resize( approx_count );
			colorarr1.resize( approx_count );
		}

		float coord = float_snapped( bounds.origin[i] - std::max( 0.f, bounds.extents[i] ) - offset, grid );
//		const float coord_end = float_snapped( bounds.origin[i] + std::max( 0.f, bounds.extents[i] ) + offset, grid ) + 0.1f;
		const bool start0 = float_snapped( coord, grid * 2 ) == coord;
		std::size_t count = 0;

		for( ; count < approx_count - 4; count += 4 ){
			verticesarr[count][i] =
			verticesarr[count + 1][i] = coord;
			const float alpha = std::max( 0.f, std::min( 1.f, ( offset + bounds.extents[i] - std::fabs( coord - bounds.origin[i] ) ) / offset ) );
			colorarr0[count] = colorarr0[count + 1] = Colour4b( 255, 0, 0, alpha * 255 );
			colorarr1[count] = colorarr1[count + 1] = Colour4b( 255, 255, 255, alpha * 255 );
			coord += grid;
			verticesarr[count + 2][i] =
			verticesarr[count + 3][i] = coord;
			const float alpha2 = std::max( 0.f, std::min( 1.f, ( offset + bounds.extents[i] - std::fabs( coord - bounds.origin[i] ) ) / offset ) );
			colorarr0[count + 2] = colorarr0[count + 3] = Colour4b( 255, 0, 0, alpha2 * 255 );
			colorarr1[count + 2] = colorarr1[count + 3] = Colour4b( 255, 255, 255, alpha2 * 255 );
			coord += grid;
			edgearr[count] =
			edgearr[count + 2] = GL_FALSE;
			edgearr[count + 1] =
			edgearr[count + 3] = GL_TRUE;
		}

		if( points.size() == 1 ){
			points.push_back( points[0] + g_vector3_axes[i2] * 8 );
			for( std::size_t k = 0; k < count; k += 4 ){
				edgearr[k + 1] = GL_FALSE;
			}
		}

		glVertexPointer( 3, GL_FLOAT, sizeof( Vector3 ), verticesarr.data()->data() );
		glEdgeFlagPointer( sizeof( GLboolean ), edgearr.data() );
		for( std::vector<Vector3>::const_iterator j = points.begin(); j != points.end(); ++++j ){
			const std::vector<Vector3>::const_iterator jj = j + 1;
			for( std::size_t k = 0; k < count; k += 4 ){
				verticesarr[k][i2] = ( *j )[i2];
				verticesarr[k][i3] = ( *j )[i3];
				verticesarr[k + 1][i2] = ( *jj )[i2];
				verticesarr[k + 1][i3] = ( *jj )[i3];
				verticesarr[k + 2][i2] = ( *jj )[i2];
				verticesarr[k + 2][i3] = ( *jj )[i3];
				verticesarr[k + 3][i2] = ( *j )[i2];
				verticesarr[k + 3][i3] = ( *j )[i3];
			}

			glPolygonOffset( -2, 2 );
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( Colour4b ), colorarr0.data() );
			glDrawArrays( GL_QUADS, start0? 0 : 2, GLsizei( count - ( start0? 4 : 2 ) ) );

			glPolygonOffset( 1, -1 );
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( Colour4b ), colorarr1.data() );
			glDrawArrays( GL_QUADS, start0? 2 : 0, GLsizei( count - ( start0? 2 : 4 ) ) );
			glPolygonOffset( -1, 1 ); // restore default
		}
	}

	glDisableClientState( GL_EDGE_FLAG_ARRAY );
}

void render( Renderer& renderer, Shader* shader ) const {
	renderer.SetState( shader, Renderer::eFullMaterials );
	renderer.addRenderable( *this, g_matrix4_identity );
}
};





class CamWnd
{
View m_view;
camera_t m_Camera;
RadiantCameraView m_cameraview;
#if 0
int m_PositionDragCursorX;
int m_PositionDragCursorY;
#endif

guint m_freemove_handle_focusout;

static Shader* m_state_select0;
static Shader* m_state_select1;
static Shader* m_state_wire;
static Shader* m_state_facewire;
static Shader* m_state_workzone;
static Shader* m_state_text;

FreezePointer m_freezePointer;

CamDrawSize m_draw_size;
RenderableCamWorkzone m_draw_workzone;

public:
FBO* m_fbo;
FBO* fbo_get(){
	return m_fbo = m_fbo? m_fbo : GlobalOpenGL().support_ARB_framebuffer_object? new FBO : new FBO_fallback;
}
GtkWidget* m_gl_widget;
GtkWindow* m_parent;

SelectionSystemWindowObserver* m_window_observer;
XORRectangle m_XORRectangle;

DeferredDraw m_deferredDraw;
DeferredMotion m_deferred_motion;

guint m_selection_button_press_handler;
guint m_selection_button_release_handler;
guint m_selection_motion_handler;

guint m_freelook_button_press_handler;
guint m_freelook_button_release_handler;

guint m_sizeHandler;
guint m_exposeHandler;

Timer m_render_time;

CamWnd();
~CamWnd();

bool m_drawing;
void queue_draw(){
	//ASSERT_MESSAGE(!m_drawing, "CamWnd::queue_draw(): called while draw is already in progress");
	if ( m_drawing ) {
		return;
	}
	//globalOutputStream() << "queue... ";
	m_deferredDraw.draw();
}
void draw();

static void captureStates(){
	m_state_text = GlobalShaderCache().capture( "$TEXT" );
	m_state_workzone = GlobalShaderCache().capture( "$CAM_WORKZONE" );
	m_state_facewire = GlobalShaderCache().capture( "$CAM_FACEWIRE" );
	m_state_wire = GlobalShaderCache().capture( "$CAM_WIRE" );
	m_state_select0 = GlobalShaderCache().capture( "$CAM_OVERLAY" );
	m_state_select1 = GlobalShaderCache().capture( "$CAM_HIGHLIGHT" );
}
static void releaseStates(){
	GlobalShaderCache().release( "$CAM_HIGHLIGHT" );
	GlobalShaderCache().release( "$CAM_OVERLAY" );
	GlobalShaderCache().release( "$CAM_WIRE" );
	GlobalShaderCache().release( "$CAM_FACEWIRE" );
	GlobalShaderCache().release( "$CAM_WORKZONE" );
	GlobalShaderCache().release( "$TEXT" );
}

camera_t& getCamera(){
	return m_Camera;
}

void BenchMark();
void Cam_ChangeFloor( bool up );

void DisableFreeMove();
void EnableFreeMove();
bool m_bFreeMove;
bool m_bFreeMove_entering;

CameraView& getCameraView(){
	return m_cameraview;
}

Timer m_rightClickTimer;
float m_rightClickMove;

void selection_motion_freemove( const MotionDeltaValues& delta );

private:
void Cam_Draw();
};

typedef MemberCaller<CamWnd, &CamWnd::queue_draw> CamWndQueueDraw;

Shader* CamWnd::m_state_select0 = 0;
Shader* CamWnd::m_state_select1 = 0;
Shader* CamWnd::m_state_wire = 0;
Shader* CamWnd::m_state_facewire = 0;
Shader* CamWnd::m_state_workzone = 0;
Shader* CamWnd::m_state_text = 0;

CamWnd* NewCamWnd(){
	return new CamWnd;
}
void DeleteCamWnd( CamWnd* camwnd ){
	delete camwnd;
}

void CamWnd_constructStatic(){
	CamWnd::captureStates();
}

void CamWnd_destroyStatic(){
	CamWnd::releaseStates();
}

void CamWnd_reconstructStatic(){
	CamWnd_destroyStatic();
	CamWnd_constructStatic();
}

static CamWnd* g_camwnd = 0;

void GlobalCamera_setCamWnd( CamWnd& camwnd ){
	g_camwnd = &camwnd;
}


GtkWidget* CamWnd_getWidget( CamWnd& camwnd ){
	return camwnd.m_gl_widget;
}

GtkWindow* CamWnd_getParent( CamWnd& camwnd ){
	return camwnd.m_parent;
}

ToggleShown g_camera_shown( true );

void CamWnd_Shown_Construct( GtkWindow* parent ){
	g_camera_shown.connect( GTK_WIDGET( parent ) );
}

void CamWnd_setParent( CamWnd& camwnd, GtkWindow* parent ){
	camwnd.m_parent = parent;
}

void CamWnd_Update( CamWnd& camwnd ){
	camwnd.queue_draw();
}



camwindow_globals_t g_camwindow_globals;

const Vector3& Camera_getOrigin( CamWnd& camwnd ){
	return Camera_getOrigin( camwnd.getCamera() );
}

void Camera_setOrigin( CamWnd& camwnd, const Vector3& origin ){
	Camera_setOrigin( camwnd.getCamera(), origin );
}

const Vector3& Camera_getAngles( CamWnd& camwnd ){
	return Camera_getAngles( camwnd.getCamera() );
}

void Camera_setAngles( CamWnd& camwnd, const Vector3& angles ){
	Camera_setAngles( camwnd.getCamera(), angles );
}

const Vector3& Camera_getViewVector( CamWnd& camwnd ){
	return camwnd.getCamera().vpn;
}


// =============================================================================
// CamWnd class

void context_menu_show(){
	if( g_pParentWnd->ActiveXY() ){
		g_pParentWnd->ActiveXY()->OnContextMenu();
		g_bCamEntityMenu = true;
	}
}

void context_menu(){
	//need this hack, otherwise button wont be released = global accels broken, until correct button pressed again...
	GdkEvent* event_ = gtk_get_current_event();
	if( event_ ){
		event_->type = GDK_BUTTON_RELEASE;
		gtk_main_do_event( event_ );
		gdk_event_free( event_ );
		context_menu_show();
	}
}

/* GDK_2BUTTON_PRESS doesn't always work in this case, so... */
bool context_menu_try( CamWnd* camwnd ){
	//globalOutputStream() << camwnd->m_rightClickTimer.elapsed_msec() << "\n";
	return camwnd->m_rightClickTimer.elapsed_msec() < 250;
	//doesn't work if cam redraw > 200msec (3x click works): gtk_widget_queue_draw proceeds after timer.start()
}

void camera_orbit_init( camera_t& cam, Vector2 xy ){
	xy.x() = ( ( 2.0f * xy.x() ) / cam.width ) - 1.0f; // window_to_normalised_device
	xy.y() = ( ( 2.0f * ( cam.height - 1 - xy.y() ) ) / cam.height ) - 1.0f;

	const Vector2 epsilon( 8.f / cam.width, 8.f / cam.height ); //device epsilon

	Scene_Intersect( *cam.m_view, xy.data(), epsilon.data(), cam.m_orbit_center );

	cam.m_orbit_initial_pos = cam.origin;
	cam.m_orbit_offset = 0;
	cam.m_orbit = true;
}

inline bool ORBIT_EVENT( GdkEventButton* event ){
	return event->button == 3 && modifiers_for_state( event->state ) == c_modifierAlt;
}
inline bool M2_EVENT( GdkEventButton* event ){
	return event->button == 3 && modifiers_for_state( event->state ) == c_modifierNone;
}

gboolean enable_freelook_button_press( GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd ){
	const bool m2    = M2_EVENT( event );
	const bool m2alt = ORBIT_EVENT( event );
	if ( ( m2 || m2alt ) && event->type == GDK_BUTTON_PRESS ) {
		camwnd->m_bFreeMove_entering = true;
		if( m2 && context_menu_try( camwnd ) ){
			context_menu();
		}
		else{
			if( m2alt )
				camera_orbit_init( camwnd->getCamera(), Vector2( event->x, event->y ) );
			camwnd->EnableFreeMove();
			camwnd->m_rightClickTimer.start();
			camwnd->m_rightClickMove = 0;
		}
		return TRUE;
	}
	return FALSE;
}

gboolean disable_freelook_button_press( GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd ){
	const bool m2    = M2_EVENT( event );
	const bool m2alt = ORBIT_EVENT( event );
	if ( ( m2 || m2alt ) && event->type == GDK_BUTTON_PRESS ) {
		camwnd->m_bFreeMove_entering = false;
		if( m2 && context_menu_try( camwnd ) ){
			camwnd->DisableFreeMove();
			context_menu();
		}
		else{
			if( m2alt )
				camera_orbit_init( camwnd->getCamera(), Vector2( event->x, event->y ) );
			camwnd->m_rightClickTimer.start();
			camwnd->m_rightClickMove = 0;
		}
		return TRUE;
	}
	return FALSE;
}

gboolean disable_freelook_button_release( GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd ){
	const bool m2    = M2_EVENT( event );
	const bool m2alt = ORBIT_EVENT( event );
	if ( ( m2 || m2alt ) && event->type == GDK_BUTTON_RELEASE ) {
		camwnd->getCamera().m_orbit = false;
		if( ( ( camwnd->m_rightClickTimer.elapsed_msec() < 300 && camwnd->m_rightClickMove < 56 ) == !camwnd->m_bFreeMove_entering ) ){
			camwnd->DisableFreeMove();
			return TRUE;
		}
	}
	return FALSE;
}

#if 0
gboolean mousecontrol_button_press( GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd ){
	if ( event->type == GDK_BUTTON_PRESS && event->button == 3 ) {
		Cam_MouseControl( camwnd->getCamera(), event->x, widget->allocation.height - 1 - event->y );
	}
	return FALSE;
}
#endif

void camwnd_update_xor_rectangle( CamWnd& self, rect_t area ){
	if ( GTK_WIDGET_VISIBLE( self.m_gl_widget ) ) {
		if ( glwidget_make_current( self.m_gl_widget ) != FALSE ) {
			if ( Map_Valid( g_map ) && ScreenUpdates_Enabled() ) {
				GlobalOpenGL_debugAssertNoErrors();

//				glDrawBuffer( GL_FRONT );
				self.fbo_get()->blit();

				self.m_XORRectangle.set( area, self.getCamera().width, self.getCamera().height );

//				glDrawBuffer( GL_BACK );

				GlobalOpenGL_debugAssertNoErrors();
//				glwidget_make_current( self.m_gl_widget );
				glwidget_swap_buffers( self.m_gl_widget );
			}
		}
	}
}


gboolean selection_button_press( GtkWidget* widget, GdkEventButton* event, WindowObserver* observer ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		gtk_widget_grab_focus( widget );
		if( !ORBIT_EVENT( event ) )
			observer->onMouseDown( WindowVector( event->x, event->y ), button_for_button( event->button ), modifiers_for_state( event->state ) );
	}
	return FALSE;
}

gboolean selection_button_release( GtkWidget* widget, GdkEventButton* event, WindowObserver* observer ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		observer->onMouseUp( WindowVector( event->x, event->y ), button_for_button( event->button ), modifiers_for_state( event->state ) );
	}
	return FALSE;
}

void selection_motion( gdouble x, gdouble y, guint state, void* data ){
	//globalOutputStream() << "motion... ";
	reinterpret_cast<WindowObserver*>( data )->onMouseMotion( WindowVector( x, y ), modifiers_for_state( state ) );
}

inline WindowVector windowvector_for_widget_centre( GtkWidget* widget ){
	return WindowVector( static_cast<float>( widget->allocation.width / 2 ), static_cast<float>( widget->allocation.height / 2 ) );
}

gboolean selection_button_press_freemove( GtkWidget* widget, GdkEventButton* event, WindowObserver* observer ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		if( !ORBIT_EVENT( event ) )
			observer->onMouseDown( windowvector_for_widget_centre( widget ), button_for_button( event->button ), modifiers_for_state( event->state ) );
	}
	return FALSE;
}

gboolean selection_button_release_freemove( GtkWidget* widget, GdkEventButton* event, WindowObserver* observer ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		observer->onMouseUp( windowvector_for_widget_centre( widget ), button_for_button( event->button ), modifiers_for_state( event->state ) );
	}
	return FALSE;
}

void CamWnd::selection_motion_freemove( const MotionDeltaValues& delta ){
	m_rightClickMove += sqrt( static_cast<double>( delta.x * delta.x + delta.y * delta.y ) );
	m_window_observer->incMouseMove( WindowVector( delta.x, delta.y ) );
	m_window_observer->onMouseMotion( windowvector_for_widget_centre( m_gl_widget ), modifiers_for_state( delta.state ) );
}
typedef MemberCaller1<CamWnd, const MotionDeltaValues&, &CamWnd::selection_motion_freemove> CamWnd_selection_motion_freemove;


void camera_orbit_scroll( camera_t& camera ){
	Vector3 viewvector = vector3_normalised( camera.m_orbit_center - Camera_getOrigin( camera ) );
	if( vector3_dot( viewvector, -camera.vpn ) < 0 )
		vector3_negate( viewvector );
	float offset = vector3_length( camera.m_orbit_center - camera.m_orbit_initial_pos );
	const int off = camera.m_orbit_offset;
	if( off < 0 || off > 16 ){
		offset -= offset * off / 8 * pow( 2.0f, static_cast<float>( off < 0 ? -off : off - 16 ) / 8.f );
	}
	else if( off == 8 ){
		offset = std::min( 8.f, offset / 16.f ); //prevent zero offset, resulting in NAN viewvector in the next scroll step
	}
	else{
		offset -= offset * off / 8;
	}
	Camera_setOrigin( camera, camera.m_orbit_center - viewvector * offset );
}

gboolean wheelmove_scroll( GtkWidget* widget, GdkEventScroll* event, CamWnd* camwnd ){
	//gtk_window_set_focus( camwnd->m_parent, camwnd->m_gl_widget );
	gtk_widget_grab_focus( camwnd->m_gl_widget );
	if( !gtk_window_is_active( camwnd->m_parent ) )
		gtk_window_present( camwnd->m_parent );

	camera_t& cam = camwnd->getCamera();

	if ( event->direction == GDK_SCROLL_UP ) {
		if ( cam.movementflags & MOVE_FOCUS ) {
			++cam.m_focus_offset;
			return FALSE;
		}
		else if( cam.m_orbit ){
			++cam.m_orbit_offset;
			camera_orbit_scroll( cam );
			return FALSE;
		}

		Camera_Freemove_updateAxes( cam );
		if( camwnd->m_bFreeMove || !g_camwindow_globals.m_bZoomInToPointer ){
			Camera_setOrigin( *camwnd, Camera_getOrigin( *camwnd ) + cam.forward * static_cast<float>( g_camwindow_globals_private.m_nScrollMoveSpeed ) );
		}
		else{
			//Matrix4 maa = matrix4_multiplied_by_matrix4( cam.projection, cam.modelview );
			Matrix4 maa = cam.m_view->GetViewMatrix();
			matrix4_affine_invert( maa );

			float x = static_cast<float>( event->x );
			float y = static_cast<float>( event->y );
			Vector3 normalized;

			normalized[0] = 2.0f * ( x ) / static_cast<float>( cam.width ) - 1.0f;
			normalized[1] = 2.0f * ( y )/ static_cast<float>( cam.height ) - 1.0f;
			normalized[1] *= -1.f;
			normalized[2] = 0.f;

			normalized *= ( camera_t::near_z * 2.f );
				//globalOutputStream() << normalized << " normalized    ";
			matrix4_transform_point( maa, normalized );
				//globalOutputStream() << normalized << "\n";
			Vector3 norm = vector3_normalised( normalized - Camera_getOrigin( *camwnd ) );
				//globalOutputStream() << normalized - Camera_getOrigin( *camwnd ) << "  normalized - Camera_getOrigin( *camwnd )\n";
				//globalOutputStream() << norm << "  norm\n";
			Camera_setOrigin( *camwnd, Camera_getOrigin( *camwnd ) + norm * static_cast<float>( g_camwindow_globals_private.m_nScrollMoveSpeed ) );
		}
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		if ( cam.movementflags & MOVE_FOCUS ) {
			--cam.m_focus_offset;
			return FALSE;
		}
		else if( cam.m_orbit ){
			--cam.m_orbit_offset;
			camera_orbit_scroll( cam );
			return FALSE;
		}

		Camera_Freemove_updateAxes( cam );
		Camera_setOrigin( *camwnd, Camera_getOrigin( *camwnd ) - cam.forward * static_cast<float>( g_camwindow_globals_private.m_nScrollMoveSpeed ) );
	}

	return FALSE;
}

gboolean camera_size_allocate( GtkWidget* widget, GtkAllocation* allocation, CamWnd* camwnd ){
	camwnd->fbo_get()->reset( allocation->width, allocation->height, g_camwindow_globals_private.m_MSAA, true );
	camwnd->getCamera().width = allocation->width;
	camwnd->getCamera().height = allocation->height;
	Camera_updateProjection( camwnd->getCamera() );
	camwnd->m_window_observer->onSizeChanged( camwnd->getCamera().width, camwnd->getCamera().height );
	camwnd->queue_draw();
	return FALSE;
}

gboolean camera_expose( GtkWidget* widget, GdkEventExpose* event, gpointer data ){
	reinterpret_cast<CamWnd*>( data )->draw();
	return FALSE;
}

void KeyEvent_connect( const char* name ){
	const KeyEvent& keyEvent = GlobalKeyEvents_find( name );
	keydown_accelerators_add( keyEvent.m_accelerator, keyEvent.m_keyDown );
	keyup_accelerators_add( keyEvent.m_accelerator, keyEvent.m_keyUp );
}

void KeyEvent_disconnect( const char* name ){
	const KeyEvent& keyEvent = GlobalKeyEvents_find( name );
	keydown_accelerators_remove( keyEvent.m_accelerator );
	keyup_accelerators_remove( keyEvent.m_accelerator );
}

void CamWnd_registerCommands( CamWnd& camwnd ){
	GlobalKeyEvents_insert( "CameraForward", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveForward_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveForward_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraBack", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveBack_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveBack_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraLeft", accelerator_null(),
							ReferenceCaller<camera_t, Camera_RotateLeft_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_RotateLeft_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraRight", accelerator_null(),
							ReferenceCaller<camera_t, Camera_RotateRight_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_RotateRight_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraStrafeRight", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveRight_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveRight_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraStrafeLeft", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveLeft_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveLeft_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraUp", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveUp_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveUp_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraDown", accelerator_null(),
							ReferenceCaller<camera_t, Camera_MoveDown_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_MoveDown_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraAngleUp", accelerator_null(),
							ReferenceCaller<camera_t, Camera_PitchUp_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_PitchUp_KeyUp>( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraAngleDown", accelerator_null(),
							ReferenceCaller<camera_t, Camera_PitchDown_KeyDown>( camwnd.getCamera() ),
							ReferenceCaller<camera_t, Camera_PitchDown_KeyUp>( camwnd.getCamera() )
							);

	GlobalKeyEvents_insert( "CameraFreeMoveForward", accelerator_null(),
							FreeMoveCameraMoveForwardKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveForwardKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveBack", accelerator_null(),
							FreeMoveCameraMoveBackKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveBackKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveLeft", accelerator_null(),
							FreeMoveCameraMoveLeftKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveLeftKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveRight", accelerator_null(),
							FreeMoveCameraMoveRightKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveRightKeyUpCaller( camwnd.getCamera() )
							);

	GlobalKeyEvents_insert( "CameraFreeMoveForward2", accelerator_null(),
							FreeMoveCameraMoveForwardKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveForwardKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveBack2", accelerator_null(),
							FreeMoveCameraMoveBackKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveBackKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveLeft2", accelerator_null(),
							FreeMoveCameraMoveLeftKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveLeftKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveRight2", accelerator_null(),
							FreeMoveCameraMoveRightKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveRightKeyUpCaller( camwnd.getCamera() )
							);

	GlobalKeyEvents_insert( "CameraFreeMoveUp", accelerator_null(),
							FreeMoveCameraMoveUpKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveUpKeyUpCaller( camwnd.getCamera() )
							);
	GlobalKeyEvents_insert( "CameraFreeMoveDown", accelerator_null(),
							FreeMoveCameraMoveDownKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraMoveDownKeyUpCaller( camwnd.getCamera() )
							);

	GlobalKeyEvents_insert( "CameraFreeFocus", accelerator_null(),
							FreeMoveCameraFocusKeyDownCaller( camwnd.getCamera() ),
							FreeMoveCameraFocusKeyUpCaller( camwnd.getCamera() )
							);

	GlobalCommands_insert( "CameraForward", ReferenceCaller<camera_t, Camera_MoveForward_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraBack", ReferenceCaller<camera_t, Camera_MoveBack_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraLeft", ReferenceCaller<camera_t, Camera_RotateLeft_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraRight", ReferenceCaller<camera_t, Camera_RotateRight_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraStrafeRight", ReferenceCaller<camera_t, Camera_MoveRight_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraStrafeLeft", ReferenceCaller<camera_t, Camera_MoveLeft_Discrete>( camwnd.getCamera() ) );

	GlobalCommands_insert( "CameraUp", ReferenceCaller<camera_t, Camera_MoveUp_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraDown", ReferenceCaller<camera_t, Camera_MoveDown_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraAngleUp", ReferenceCaller<camera_t, Camera_PitchUp_Discrete>( camwnd.getCamera() ) );
	GlobalCommands_insert( "CameraAngleDown", ReferenceCaller<camera_t, Camera_PitchDown_Discrete>( camwnd.getCamera() ) );
}

void CamWnd_Move_Enable( CamWnd& camwnd ){
	KeyEvent_connect( "CameraForward" );
	KeyEvent_connect( "CameraBack" );
	KeyEvent_connect( "CameraLeft" );
	KeyEvent_connect( "CameraRight" );
	KeyEvent_connect( "CameraStrafeRight" );
	KeyEvent_connect( "CameraStrafeLeft" );
	KeyEvent_connect( "CameraUp" );
	KeyEvent_connect( "CameraDown" );
	KeyEvent_connect( "CameraAngleUp" );
	KeyEvent_connect( "CameraAngleDown" );
}

void CamWnd_Move_Disable( CamWnd& camwnd ){
	KeyEvent_disconnect( "CameraForward" );
	KeyEvent_disconnect( "CameraBack" );
	KeyEvent_disconnect( "CameraLeft" );
	KeyEvent_disconnect( "CameraRight" );
	KeyEvent_disconnect( "CameraStrafeRight" );
	KeyEvent_disconnect( "CameraStrafeLeft" );
	KeyEvent_disconnect( "CameraUp" );
	KeyEvent_disconnect( "CameraDown" );
	KeyEvent_disconnect( "CameraAngleUp" );
	KeyEvent_disconnect( "CameraAngleDown" );
}

void CamWnd_Move_Discrete_Enable( CamWnd& camwnd ){
	command_connect_accelerator( "CameraForward" );
	command_connect_accelerator( "CameraBack" );
	command_connect_accelerator( "CameraLeft" );
	command_connect_accelerator( "CameraRight" );
	command_connect_accelerator( "CameraStrafeRight" );
	command_connect_accelerator( "CameraStrafeLeft" );
	command_connect_accelerator( "CameraUp" );
	command_connect_accelerator( "CameraDown" );
	command_connect_accelerator( "CameraAngleUp" );
	command_connect_accelerator( "CameraAngleDown" );
}

void CamWnd_Move_Discrete_Disable( CamWnd& camwnd ){
	command_disconnect_accelerator( "CameraForward" );
	command_disconnect_accelerator( "CameraBack" );
	command_disconnect_accelerator( "CameraLeft" );
	command_disconnect_accelerator( "CameraRight" );
	command_disconnect_accelerator( "CameraStrafeRight" );
	command_disconnect_accelerator( "CameraStrafeLeft" );
	command_disconnect_accelerator( "CameraUp" );
	command_disconnect_accelerator( "CameraDown" );
	command_disconnect_accelerator( "CameraAngleUp" );
	command_disconnect_accelerator( "CameraAngleDown" );
}

void CamWnd_Move_Discrete_Import( CamWnd& camwnd, bool value ){
	if ( g_camwindow_globals_private.m_bCamDiscrete ) {
		CamWnd_Move_Discrete_Disable( camwnd );
	}
	else
	{
		CamWnd_Move_Disable( camwnd );
	}

	g_camwindow_globals_private.m_bCamDiscrete = value;

	if ( g_camwindow_globals_private.m_bCamDiscrete ) {
		CamWnd_Move_Discrete_Enable( camwnd );
	}
	else
	{
		CamWnd_Move_Enable( camwnd );
	}
}

void CamWnd_Move_Discrete_Import( bool value ){
	if ( g_camwnd != 0 ) {
		CamWnd_Move_Discrete_Import( *g_camwnd, value );
	}
	else
	{
		g_camwindow_globals_private.m_bCamDiscrete = value;
	}
}



void CamWnd_Add_Handlers_Move( CamWnd& camwnd ){
	camwnd.m_selection_button_press_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_press_event", G_CALLBACK( selection_button_press ), camwnd.m_window_observer );
	camwnd.m_selection_button_release_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_release_event", G_CALLBACK( selection_button_release ), camwnd.m_window_observer );
	camwnd.m_selection_motion_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "motion_notify_event", G_CALLBACK( DeferredMotion::gtk_motion ), &camwnd.m_deferred_motion );

	camwnd.m_freelook_button_press_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_press_event", G_CALLBACK( enable_freelook_button_press ), &camwnd );

	if ( g_camwindow_globals_private.m_bCamDiscrete ) {
		CamWnd_Move_Discrete_Enable( camwnd );
	}
	else
	{
		CamWnd_Move_Enable( camwnd );
	}
}

void CamWnd_Remove_Handlers_Move( CamWnd& camwnd ){
	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_selection_button_press_handler );
	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_selection_button_release_handler );
	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_selection_motion_handler );

	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_freelook_button_press_handler );

	if ( g_camwindow_globals_private.m_bCamDiscrete ) {
		CamWnd_Move_Discrete_Disable( camwnd );
	}
	else
	{
		CamWnd_Move_Disable( camwnd );
	}
}

void CamWnd_Add_Handlers_FreeMove( CamWnd& camwnd ){
	camwnd.m_selection_button_press_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_press_event", G_CALLBACK( selection_button_press_freemove ), camwnd.m_window_observer );
	camwnd.m_selection_button_release_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_release_event", G_CALLBACK( selection_button_release_freemove ), camwnd.m_window_observer );

	camwnd.m_freelook_button_press_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_press_event", G_CALLBACK( disable_freelook_button_press ), &camwnd );
	camwnd.m_freelook_button_release_handler = g_signal_connect( G_OBJECT( camwnd.m_gl_widget ), "button_release_event", G_CALLBACK( disable_freelook_button_release ), &camwnd );

	KeyEvent_connect( "CameraFreeMoveForward" );
	KeyEvent_connect( "CameraFreeMoveBack" );
	KeyEvent_connect( "CameraFreeMoveLeft" );
	KeyEvent_connect( "CameraFreeMoveRight" );

	KeyEvent_connect( "CameraFreeMoveForward2" );
	KeyEvent_connect( "CameraFreeMoveBack2" );
	KeyEvent_connect( "CameraFreeMoveLeft2" );
	KeyEvent_connect( "CameraFreeMoveRight2" );

	KeyEvent_connect( "CameraFreeMoveUp" );
	KeyEvent_connect( "CameraFreeMoveDown" );

	KeyEvent_connect( "CameraFreeFocus" );
}

void CamWnd_Remove_Handlers_FreeMove( CamWnd& camwnd ){
	KeyEvent_disconnect( "CameraFreeMoveForward" );
	KeyEvent_disconnect( "CameraFreeMoveBack" );
	KeyEvent_disconnect( "CameraFreeMoveLeft" );
	KeyEvent_disconnect( "CameraFreeMoveRight" );

	KeyEvent_disconnect( "CameraFreeMoveForward2" );
	KeyEvent_disconnect( "CameraFreeMoveBack2" );
	KeyEvent_disconnect( "CameraFreeMoveLeft2" );
	KeyEvent_disconnect( "CameraFreeMoveRight2" );

	KeyEvent_disconnect( "CameraFreeMoveUp" );
	KeyEvent_disconnect( "CameraFreeMoveDown" );

	KeyEvent_disconnect( "CameraFreeFocus" );

	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_selection_button_press_handler );
	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_selection_button_release_handler );

	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_freelook_button_press_handler );
	g_signal_handler_disconnect( G_OBJECT( camwnd.m_gl_widget ), camwnd.m_freelook_button_release_handler );
}

CamWnd::CamWnd() :
	m_view( true ),
	m_Camera( &m_view, CamWndQueueDraw( *this ), CamWnd_selection_motion_freemove( *this ) ),
	m_cameraview( m_Camera, &m_view, ReferenceCaller<CamWnd, CamWnd_Update>( *this ) ),
	m_fbo( 0 ),
	m_gl_widget( glwidget_new( TRUE ) ),
	m_window_observer( NewWindowObserver() ),
	m_deferredDraw( WidgetQueueDrawCaller( *m_gl_widget ) ),
	m_deferred_motion( selection_motion, m_window_observer ),
	m_selection_button_press_handler( 0 ),
	m_selection_button_release_handler( 0 ),
	m_selection_motion_handler( 0 ),
	m_freelook_button_press_handler( 0 ),
	m_freelook_button_release_handler( 0 ),
	m_drawing( false )
{
	m_bFreeMove = false;

	GlobalWindowObservers_add( m_window_observer );
	GlobalWindowObservers_connectWidget( m_gl_widget );

	m_window_observer->setRectangleDrawCallback( ReferenceCaller1<CamWnd, rect_t, camwnd_update_xor_rectangle>( *this ) );
	m_window_observer->setView( m_view );

	gtk_widget_ref( m_gl_widget );

	gtk_widget_set_events( m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );
	GTK_WIDGET_SET_FLAGS( m_gl_widget, GTK_CAN_FOCUS );

	m_sizeHandler = g_signal_connect( G_OBJECT( m_gl_widget ), "size_allocate", G_CALLBACK( camera_size_allocate ), this );
	m_exposeHandler = g_signal_connect( G_OBJECT( m_gl_widget ), "expose_event", G_CALLBACK( camera_expose ), this );

	Map_addValidCallback( g_map, DeferredDrawOnMapValidChangedCaller( m_deferredDraw ) );

	CamWnd_registerCommands( *this );

	CamWnd_Add_Handlers_Move( *this );

	g_signal_connect( G_OBJECT( m_gl_widget ), "scroll_event", G_CALLBACK( wheelmove_scroll ), this );

	AddSceneChangeCallback( ReferenceCaller<CamWnd, CamWnd_Update>( *this ) );

	PressedButtons_connect( g_pressedButtons, m_gl_widget );
}

CamWnd::~CamWnd(){
	if ( m_bFreeMove ) {
		DisableFreeMove();
	}

	CamWnd_Remove_Handlers_Move( *this );

	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_sizeHandler );
	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_exposeHandler );

	gtk_widget_unref( m_gl_widget );

	delete m_fbo;

	m_window_observer->release();
}

class FloorHeightWalker : public scene::Graph::Walker
{
Vector3 m_current;
public:
mutable float m_bestUp;
mutable float m_bestDown;
mutable float m_bottom;
FloorHeightWalker( const Vector3& current ) :
	m_current( current ), m_bestUp( g_MaxWorldCoord ), m_bestDown( g_MinWorldCoord ), m_bottom( g_MaxWorldCoord ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if( !path.top().get().visible() )
		return false;
	if ( !path.top().get().isRoot() && !node_is_group( path.top() ) ) {
		const AABB& aabb = instance.worldAABB();
		if( instance.isSelected() || ( m_current.x() > aabb.origin.x() - aabb.extents.x()
										&& m_current.x() < aabb.origin.x() + aabb.extents.x()
										&& m_current.y() > aabb.origin.y() - aabb.extents.y()
										&& m_current.y() < aabb.origin.y() + aabb.extents.y() ) ){
			const float floorHeight = aabb.origin.z() + aabb.extents.z() + 32.f;
			if ( floorHeight > m_current.z() + 0.1f && floorHeight < m_bestUp ) /* 0.1f epsilon to prevent jam at (close?) coords */
				m_bestUp = floorHeight;
			if ( floorHeight < m_current.z() - 0.1f && floorHeight > m_bestDown )
				m_bestDown = floorHeight;
			const float bottom = aabb.origin.z() - aabb.extents.z() - 16.f;
			if( m_bottom > bottom )
				m_bottom = bottom;
		}
	}
	return true;
}
};

void CamWnd::Cam_ChangeFloor( bool up ){
	FloorHeightWalker walker( m_Camera.origin );
	GlobalSceneGraph().traverse( walker );
	float current = m_Camera.origin.z();

	if ( up ){
		if( walker.m_bottom != g_MaxWorldCoord && walker.m_bottom > current )
			current = walker.m_bottom;
		else if( walker.m_bestUp != g_MaxWorldCoord )
			current = walker.m_bestUp;
	}
	else{
		if ( walker.m_bestDown != g_MinWorldCoord )
			current = walker.m_bestDown;
		else if( walker.m_bottom != g_MaxWorldCoord && walker.m_bottom < current )
			current = walker.m_bottom;
	}

	if( m_Camera.origin.z() != current ){
		m_Camera.origin.z() = current;
		Camera_updateModelview( getCamera() );
		CamWnd_Update( *this );
		CameraMovedNotify();
	}
}


#if 0

// button_press
Sys_GetCursorPos( &m_PositionDragCursorX, &m_PositionDragCursorY );

// motion
if ( ( m_bFreeMove && ( buttons == ( RAD_CONTROL | RAD_SHIFT ) ) )
	 || ( !m_bFreeMove && ( buttons == ( RAD_RBUTTON | RAD_CONTROL ) ) ) ) {
	Cam_PositionDrag();
	CamWnd_Update( camwnd );
	CameraMovedNotify();
	return;
}

void CamWnd::Cam_PositionDrag(){
	int x, y;

	Sys_GetCursorPos( GTK_WINDOW( m_gl_widget ), &x, &y );
	if ( x != m_PositionDragCursorX || y != m_PositionDragCursorY ) {
		x -= m_PositionDragCursorX;
		vector3_add( m_Camera.origin, vector3_scaled( m_Camera.vright, x ) );
		y -= m_PositionDragCursorY;
		m_Camera.origin[2] -= y;
		Camera_updateModelview();
		CamWnd_Update( camwnd );
		CameraMovedNotify();

		Sys_SetCursorPos( GTK_WINDOW( m_parent ), m_PositionDragCursorX, m_PositionDragCursorY );
	}
}
#endif


// NOTE TTimo if there's an OS-level focus out of the application
//   then we can release the camera cursor grab
static gboolean camwindow_freemove_focusout( GtkWidget* widget, GdkEventFocus* event, gpointer data ){
	reinterpret_cast<CamWnd*>( data )->DisableFreeMove();
	return FALSE;
}

void CamWnd::EnableFreeMove(){
	//globalOutputStream() << "EnableFreeMove\n";

	ASSERT_MESSAGE( !m_bFreeMove, "EnableFreeMove: free-move was already enabled" );
	m_bFreeMove = true;
	Camera_clearMovementFlags( getCamera(), MOVE_ALL );

	CamWnd_Remove_Handlers_Move( *this );
	CamWnd_Add_Handlers_FreeMove( *this );

	gtk_window_set_focus( m_parent, m_gl_widget );
	m_freemove_handle_focusout = g_signal_connect( G_OBJECT( m_gl_widget ), "focus_out_event", G_CALLBACK( camwindow_freemove_focusout ), this );
	m_freezePointer.freeze_pointer( m_parent, m_gl_widget, Camera_motionDelta, &m_Camera );

	CamWnd_Update( *this );
}

void CamWnd::DisableFreeMove(){
	//globalOutputStream() << "DisableFreeMove\n";

	ASSERT_MESSAGE( m_bFreeMove, "DisableFreeMove: free-move was not enabled" );
	m_bFreeMove = false;
	Camera_clearMovementFlags( getCamera(), MOVE_ALL );

	CamWnd_Remove_Handlers_FreeMove( *this );
	CamWnd_Add_Handlers_Move( *this );

	m_freezePointer.unfreeze_pointer( m_parent, true );
	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_freemove_handle_focusout );

	CamWnd_Update( *this );
}


#include "renderer.h"

class CamRenderer : public Renderer
{
struct state_type
{
	state_type() : m_highlight( 0 ), m_state( 0 ), m_lights( 0 ){
	}
	unsigned int m_highlight;
	Shader* m_state;
	const LightList* m_lights;
};

std::vector<state_type> m_state_stack;
RenderStateFlags m_globalstate;
Shader* m_state_facewire;
Shader* m_state_wire;
Shader* m_state_select0;
Shader* m_state_select1;
const Vector3& m_viewer;

public:
CamRenderer( RenderStateFlags globalstate, Shader* facewire, Shader* wire, Shader* select0, Shader* select1, const Vector3& viewer ) :
	m_globalstate( globalstate ),
	m_state_facewire( facewire ),
	m_state_wire( wire ),
	m_state_select0( select0 ),
	m_state_select1( select1 ),
	m_viewer( viewer ){
	ASSERT_NOTNULL( select0 );
//	ASSERT_NOTNULL( select1 );
	m_state_stack.push_back( state_type() );
}

void SetState( Shader* state, EStyle style ){
	ASSERT_NOTNULL( state );
	if ( style == eFullMaterials ) {
		m_state_stack.back().m_state = state;
	}
}
EStyle getStyle() const {
	return eFullMaterials;
}
void PushState(){
	m_state_stack.push_back( m_state_stack.back() );
}
void PopState(){
	ASSERT_MESSAGE( !m_state_stack.empty(), "popping empty stack" );
	m_state_stack.pop_back();
}
void Highlight( EHighlightMode mode, bool bEnable = true ){
	( bEnable )
	? m_state_stack.back().m_highlight |= mode
	: m_state_stack.back().m_highlight &= ~mode;
}
void setLights( const LightList& lights ){
	m_state_stack.back().m_lights = &lights;
}
void addRenderable( const OpenGLRenderable& renderable, const Matrix4& world ){
	if ( m_state_stack.back().m_highlight & ePrimitive ) {
		m_state_select0->addRenderable( renderable, world, m_state_stack.back().m_lights );
	}
	else if ( m_state_wire && m_state_stack.back().m_highlight & ePrimitiveWire ) {
		m_state_wire->addRenderable( renderable, world, m_state_stack.back().m_lights );
	}
	if ( m_state_select1 && m_state_stack.back().m_highlight & eFace ) {
		m_state_select1->addRenderable( renderable, world, m_state_stack.back().m_lights );
	}
	if ( m_state_facewire && m_state_stack.back().m_highlight & eFaceWire ) {
		m_state_facewire->addRenderable( renderable, world, m_state_stack.back().m_lights );
	}

	m_state_stack.back().m_state->addRenderable( renderable, world, m_state_stack.back().m_lights );
}

void render( const Matrix4& modelview, const Matrix4& projection ){
	GlobalShaderCache().render( m_globalstate, modelview, projection, m_viewer );
}
};

/*
   ==============
   Cam_Draw
   ==============
 */
/*
void ShowStatsToggle(){
	g_camwindow_globals_private.m_showStats ^= 1;
}
typedef FreeCaller<ShowStatsToggle> ShowStatsToggleCaller;

void ShowStatsExport( const BoolImportCallback& importer ){
	importer( g_camwindow_globals_private.m_showStats );
}
typedef FreeCaller1<const BoolImportCallback&, ShowStatsExport> ShowStatsExportCaller;

ShowStatsExportCaller g_show_stats_caller;
BoolExportCallback g_show_stats_callback( g_show_stats_caller );
ToggleItem g_show_stats( g_show_stats_callback );
*/
BoolExportCaller g_show_stats_caller( g_camwindow_globals.m_showStats );
ToggleItem g_show_stats( g_show_stats_caller );
void ShowStatsToggle(){
	g_camwindow_globals.m_showStats ^= 1;
	g_show_stats.update();
	UpdateAllWindows();
}

BoolExportCaller g_show_workzone3d_caller( g_camwindow_globals_private.m_bShowWorkzone );
ToggleItem g_show_workzone3d( g_show_workzone3d_caller );
void ShowWorkzone3dToggle(){
	g_camwindow_globals_private.m_bShowWorkzone ^= 1;
	g_show_workzone3d.update();
	if ( g_camwnd != 0 ) {
		CamWnd_Update( *g_camwnd );
	}
}

BoolExportCaller g_show_size3d_caller( g_camwindow_globals_private.m_bShowSize );
ToggleItem g_show_size3d( g_show_size3d_caller );
void ShowSize3dToggle(){
	g_camwindow_globals_private.m_bShowSize ^= 1;
	g_show_size3d.update();
	if ( g_camwnd != 0 ) {
		CamWnd_Update( *g_camwnd );
	}
}

void CamWnd::Cam_Draw(){
//		globalOutputStream() << "Cam_Draw()\n";
	fbo_get()->start();

	glViewport( 0, 0, m_Camera.width, m_Camera.height );
#if 0
	GLint viewprt[4];
	glGetIntegerv( GL_VIEWPORT, viewprt );
#endif

	// enable depth buffer writes
	glDepthMask( GL_TRUE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	Vector3 clearColour( 0, 0, 0 );
	if ( m_Camera.draw_mode != cd_lighting ) {
		clearColour = g_camwindow_globals.color_cameraback;
	}

	glClearColor( clearColour[0], clearColour[1], clearColour[2], 0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	extern void Renderer_ResetStats();
	Renderer_ResetStats();
	extern void Cull_ResetStats();
	Cull_ResetStats();

	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( reinterpret_cast<const float*>( &m_Camera.projection ) );

	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( reinterpret_cast<const float*>( &m_Camera.modelview ) );


	// one directional light source directly behind the viewer
	{
		GLfloat inverse_cam_dir[4], ambient[4], diffuse[4]; //, material[4];

		ambient[0] = ambient[1] = ambient[2] = 0.4f;
		ambient[3] = 1.0f;
		diffuse[0] = diffuse[1] = diffuse[2] = 0.4f;
		diffuse[3] = 1.0f;
		//material[0] = material[1] = material[2] = 0.8f;
		//material[3] = 1.0f;

		inverse_cam_dir[0] = m_Camera.vpn[0];
		inverse_cam_dir[1] = m_Camera.vpn[1];
		inverse_cam_dir[2] = m_Camera.vpn[2];
		inverse_cam_dir[3] = 0;

		glLightfv( GL_LIGHT0, GL_POSITION, inverse_cam_dir );

		glLightfv( GL_LIGHT0, GL_AMBIENT, ambient );
		glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuse );

		glEnable( GL_LIGHT0 );
	}


	unsigned int globalstate = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_ALPHATEST | RENDER_BLEND | RENDER_CULLFACE | RENDER_COLOURARRAY | RENDER_OFFSETLINE | RENDER_POLYGONSMOOTH | RENDER_LINESMOOTH | RENDER_FOG | RENDER_COLOURCHANGE;
	switch ( m_Camera.draw_mode )
	{
	case cd_wire:
		break;
	case cd_solid:
		globalstate |= RENDER_FILL
					   | RENDER_LIGHTING
					   | RENDER_SMOOTH
					   | RENDER_SCALED;
		break;
	case cd_texture:
	case cd_texture_plus_wire:
		globalstate |= RENDER_FILL
					   | RENDER_LIGHTING
					   | RENDER_TEXTURE
					   | RENDER_SMOOTH
					   | RENDER_SCALED;
		break;
	case cd_lighting:
		globalstate |= RENDER_FILL
					   | RENDER_LIGHTING
					   | RENDER_TEXTURE
					   | RENDER_SMOOTH
					   | RENDER_SCALED
					   | RENDER_BUMP
					   | RENDER_PROGRAM
					   | RENDER_SCREEN;
		break;
	default:
		globalstate = 0;
		break;
	}

//	if ( !g_xywindow_globals.m_bNoStipple ) {
		globalstate |= RENDER_LINESTIPPLE | RENDER_POLYGONSTIPPLE;
//	}

	{
		CamRenderer renderer( globalstate,
							g_camwindow_globals_private.m_bFaceWire ? m_state_facewire : 0,
							m_Camera.draw_mode == cd_texture_plus_wire ? m_state_wire : 0,
							m_state_select0,
							g_camwindow_globals_private.m_bFaceFill ? m_state_select1 : 0,
							m_view.getViewer() );

		Scene_Render( renderer, m_view );

		if( g_camwindow_globals_private.m_bShowWorkzone && GlobalSelectionSystem().countSelected() != 0 && GlobalSelectionSystem().ManipulatorMode() != SelectionSystem::eUV ){
			m_draw_workzone.render( renderer, m_state_workzone );
		}

		if( g_camwindow_globals_private.m_bShowSize && GlobalSelectionSystem().countSelected() != 0 ){
			m_draw_size.render( renderer, m_state_text, m_view );
		}

		renderer.render( m_Camera.modelview, m_Camera.projection );
	}

	// prepare for 2d stuff
	glColor4f( 1, 1, 1, 1 );
	glDisable( GL_BLEND );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, (float)m_Camera.width, 0, (float)m_Camera.height, -100, 100 );
	glScalef( 1, -1, 1 );
	glTranslatef( 0, -(float)m_Camera.height, 0 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	if ( GlobalOpenGL().GL_1_3() ) {
		glClientActiveTexture( GL_TEXTURE0 );
		glActiveTexture( GL_TEXTURE0 );
	}

	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_LIGHTING );
	glDisable( GL_COLOR_MATERIAL );
	glDisable( GL_DEPTH_TEST );
	glLineWidth( 1 );

	// draw the crosshair
	if ( m_bFreeMove ) {
		glBegin( GL_LINES );
		glVertex2f( (float)m_Camera.width / 2.f, (float)m_Camera.height / 2.f + 6 );
		glVertex2f( (float)m_Camera.width / 2.f, (float)m_Camera.height / 2.f + 2 );
		glVertex2f( (float)m_Camera.width / 2.f, (float)m_Camera.height / 2.f - 6 );
		glVertex2f( (float)m_Camera.width / 2.f, (float)m_Camera.height / 2.f - 2 );
		glVertex2f( (float)m_Camera.width / 2.f + 6, (float)m_Camera.height / 2.f );
		glVertex2f( (float)m_Camera.width / 2.f + 2, (float)m_Camera.height / 2.f );
		glVertex2f( (float)m_Camera.width / 2.f - 6, (float)m_Camera.height / 2.f );
		glVertex2f( (float)m_Camera.width / 2.f - 2, (float)m_Camera.height / 2.f );
		glEnd();
	}

	if ( g_camwindow_globals.m_showStats ) {
		glRasterPos3f( 1.0f, static_cast<float>( m_Camera.height ) - GlobalOpenGL().m_font->getPixelDescent(), 0.0f );
		extern const char* Renderer_GetStats();
		GlobalOpenGL().drawString( Renderer_GetStats() );

		StringOutputStream stream;
		stream << " | f2f: " << m_render_time.elapsed_msec();
		GlobalOpenGL().drawString( stream.c_str() );
		m_render_time.start();

		glRasterPos3f( 1.0f, static_cast<float>( m_Camera.height ) - GlobalOpenGL().m_font->getPixelDescent() - GlobalOpenGL().m_font->getPixelHeight(), 0.0f );
		extern const char* Cull_GetStats();
		GlobalOpenGL().drawString( Cull_GetStats() );
	}

	// bind back to the default texture so that we don't have problems
	// elsewhere using/modifying texture maps between contexts
	glBindTexture( GL_TEXTURE_2D, 0 );

	fbo_get()->save();
}

void CamWnd::draw(){
	m_drawing = true;

	//globalOutputStream() << "draw...\n";
	if ( glwidget_make_current( m_gl_widget ) != FALSE ) {
		if ( Map_Valid( g_map ) && ScreenUpdates_Enabled() ) {
			GlobalOpenGL_debugAssertNoErrors();
			Cam_Draw();
			GlobalOpenGL_debugAssertNoErrors();
			//qglFinish();

			//m_XORRectangle.set( rect_t() );
		}

		glwidget_swap_buffers( m_gl_widget );
	}

	m_drawing = false;
}

void CamWnd::BenchMark(){
	double dStart = Sys_DoubleTime();
	for ( int i = 0 ; i < 100 ; i++ )
	{
		Vector3 angles;
		angles[CAMERA_ROLL] = 0;
		angles[CAMERA_PITCH] = 0;
		angles[CAMERA_YAW] = static_cast<float>( i * ( 360.0 / 100.0 ) );
		Camera_setAngles( *this, angles );
	}
	double dEnd = Sys_DoubleTime();
	globalOutputStream() << FloatFormat( dEnd - dStart, 5, 2 ) << " seconds\n";
}


void GlobalCamera_ResetAngles(){
	CamWnd& camwnd = *g_camwnd;
	Vector3 angles;
	angles[CAMERA_ROLL] = angles[CAMERA_PITCH] = 0;
	angles[CAMERA_YAW] = static_cast<float>( 22.5 * floor( ( Camera_getAngles( camwnd )[CAMERA_YAW] + 11 ) / 22.5 ) );
	Camera_setAngles( camwnd, angles );
}

#include "select.h"

Vector3 Camera_getFocusPos( camera_t& camera ){
	const Vector3 camorigin( Camera_getOrigin( camera ) );
	const AABB aabb( aabb_for_minmax( Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max ) );
	const View& view = *( camera.m_view );
#if 0
	Vector3 angles( Camera_getAngles( camera ) );
	Vector3 radangles( degrees_to_radians( angles[0] ), degrees_to_radians( angles[1] ), degrees_to_radians( angles[2] ) );
	Vector3 viewvector;
	viewvector[0] = cos( radangles[1] ) * cos( radangles[0] );
	viewvector[1] = sin( radangles[1] ) * cos( radangles[0] );
	viewvector[2] = sin( radangles[0] );
#elif 0
	const Vector3 viewvector( view.getViewDir() );
#elif 1
	const Vector3 viewvector( -camera.vpn );
#endif

	Plane3 frustumPlanes[4];
	frustumPlanes[0] = plane3_translated( view.getFrustum().left, camorigin - aabb.origin );
	frustumPlanes[1] = plane3_translated( view.getFrustum().right, camorigin - aabb.origin );
	frustumPlanes[2] = plane3_translated( view.getFrustum().top, camorigin - aabb.origin );
	frustumPlanes[3] = plane3_translated( view.getFrustum().bottom, camorigin - aabb.origin );

	float offset = 64.0f;

	Vector3 corners[8];
	aabb_corners( aabb, corners );

	for ( std::size_t i = 0; i < 4; ++i ){
		for ( std::size_t j = 0; j < 8; ++j ){
			const Ray ray( aabb.origin, -viewvector );
			//Plane3 newplane( frustumPlanes[i].normal(), vector3_dot( frustumPlanes[i].normal(), corners[j] - frustumPlanes[i].normal() * 16.0f ) );
			const Plane3 newplane( frustumPlanes[i].normal(), vector3_dot( frustumPlanes[i].normal(), corners[j] ) );
			const float d = vector3_dot( ray.direction, newplane.normal() );
			if( d != 0 ){
				const float s = vector3_dot( newplane.normal() * newplane.dist() - ray.origin, newplane.normal() ) / d;
				offset = std::max( offset, s );
			}
		}
	}

	const int off = camera.m_focus_offset;
	if( off < 0 || off > 16 ){
		offset -= offset * off / 8 * pow( 2.0f, static_cast<float>( off < 0 ? -off : off - 16 ) / 8.f );
	}
	else{
		offset -= offset * off / 8;
	}
	return ( aabb.origin - viewvector * offset );
}

void GlobalCamera_FocusOnSelected(){
	Camera_setOrigin( *g_camwnd, Camera_getFocusPos( g_camwnd->getCamera() ) );
}

void Camera_ChangeFloorUp(){
	CamWnd& camwnd = *g_camwnd;
	camwnd.Cam_ChangeFloor( true );
}

void Camera_ChangeFloorDown(){
	CamWnd& camwnd = *g_camwnd;
	camwnd.Cam_ChangeFloor( false );
}

void Camera_CubeIn(){
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale--;
	if ( g_camwindow_globals.m_nCubicScale < 1 ) {
		g_camwindow_globals.m_nCubicScale = 1;
	}
	Camera_updateProjection( camwnd.getCamera() );
	CamWnd_Update( camwnd );
	GridStatus_changed();
}

void Camera_CubeOut(){
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale++;
	if ( g_camwindow_globals.m_nCubicScale > 23 ) {
		g_camwindow_globals.m_nCubicScale = 23;
	}
	Camera_updateProjection( camwnd.getCamera() );
	CamWnd_Update( camwnd );
	GridStatus_changed();
}

bool Camera_GetFarClip(){
	return g_camwindow_globals_private.m_bCubicClipping;
}

BoolExportCaller g_getfarclip_caller( g_camwindow_globals_private.m_bCubicClipping );
ToggleItem g_getfarclip_item( g_getfarclip_caller );

void Camera_SetFarClip( bool value ){
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals_private.m_bCubicClipping = value;
	g_getfarclip_item.update();
	Camera_updateProjection( camwnd.getCamera() );
	CamWnd_Update( camwnd );
}

void Camera_ToggleFarClip(){
	Camera_SetFarClip( !Camera_GetFarClip() );
}


void CamWnd_constructToolbar( GtkToolbar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Cubic clip the camera view (Ctrl + \\)", "view_cubicclipping.png", "ToggleCubicClip" );
}

void CamWnd_registerShortcuts(){
	toggle_add_accelerator( "ToggleCubicClip" );

	if ( g_pGameDescription->mGameType == "doom3" ) {
		command_connect_accelerator( "TogglePreview" );
	}

	command_connect_accelerator( "CameraModeNext" );
	command_connect_accelerator( "CameraModePrev" );

	command_connect_accelerator( "CameraSpeedInc" );
	command_connect_accelerator( "CameraSpeedDec" );
}


void GlobalCamera_Benchmark(){
	CamWnd& camwnd = *g_camwnd;
	camwnd.BenchMark();
}

void GlobalCamera_Update(){
	CamWnd& camwnd = *g_camwnd;
	CamWnd_Update( camwnd );
}

camera_draw_mode CamWnd_GetMode(){
	return camera_t::draw_mode;
}
void CamWnd_SetMode( camera_draw_mode mode ){
	ShaderCache_setBumpEnabled( mode == cd_lighting );
	camera_t::draw_mode = mode;
	if ( g_camwnd != 0 ) {
		CamWnd_Update( *g_camwnd );
	}
}

void CamWnd_TogglePreview( void ){
	// gametype must be doom3 for this function to work
	// if the gametype is not doom3 something is wrong with the
	// global command list or somebody else calls this function.
	ASSERT_MESSAGE( g_pGameDescription->mGameType == "doom3", "CamWnd_TogglePreview called although mGameType is not doom3 compatible" );

	// switch between textured and lighting mode
	CamWnd_SetMode( ( CamWnd_GetMode() == cd_lighting ) ? cd_texture : cd_lighting );
}


CameraModel* g_camera_model = 0;

void CamWnd_LookThroughCamera( CamWnd& camwnd ){
	if ( g_camera_model != 0 ) {
		CamWnd_Add_Handlers_Move( camwnd );
		g_camera_model->setCameraView( 0, Callback() );
		g_camera_model = 0;
		Camera_updateModelview( camwnd.getCamera() );
		Camera_updateProjection( camwnd.getCamera() );
		CamWnd_Update( camwnd );
	}
}

inline CameraModel* Instance_getCameraModel( scene::Instance& instance ){
	return InstanceTypeCast<CameraModel>::cast( instance );
}

void CamWnd_LookThroughSelected( CamWnd& camwnd ){
	if ( g_camera_model != 0 ) {
		CamWnd_LookThroughCamera( camwnd );
	}

	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		CameraModel* cameraModel = Instance_getCameraModel( instance );
		if ( cameraModel != 0 ) {
			CamWnd_Remove_Handlers_Move( camwnd );
			g_camera_model = cameraModel;
			g_camera_model->setCameraView( &camwnd.getCameraView(), ReferenceCaller<CamWnd, CamWnd_LookThroughCamera>( camwnd ) );
		}
	}
}

void GlobalCamera_LookThroughSelected(){
	CamWnd_LookThroughSelected( *g_camwnd );
}

void GlobalCamera_LookThroughCamera(){
	CamWnd_LookThroughCamera( *g_camwnd );
}


void RenderModeImport( int value ){
	CamWnd_SetMode( static_cast<camera_draw_mode>( ( value < 0 || value >= camera_draw_mode_count)? 2 : value ) );
}
typedef FreeCaller1<int, RenderModeImport> RenderModeImportCaller;

void RenderModeExport( const IntImportCallback& importer ){
	importer( CamWnd_GetMode() );
}
typedef FreeCaller1<const IntImportCallback&, RenderModeExport> RenderModeExportCaller;

void CameraModeNext(){
	const int count = camera_draw_mode_count - ( g_pGameDescription->mGameType == "doom3"? 0 : 1 );
	CamWnd_SetMode( static_cast<camera_draw_mode>( ( CamWnd_GetMode() + 1 ) % count ) );
}
void CameraModePrev(){
	const int count = camera_draw_mode_count - ( g_pGameDescription->mGameType == "doom3"? 0 : 1 );
	CamWnd_SetMode( static_cast<camera_draw_mode>( ( CamWnd_GetMode() + count - 1 ) % count ) );
}

void CamMSAAImport( int value ){
	g_camwindow_globals_private.m_MSAA = value ? 1 << value : value;
	if ( g_camwnd != 0 ) {
		g_camwnd->fbo_get()->reset( g_camwnd->getCamera().width, g_camwnd->getCamera().height, g_camwindow_globals_private.m_MSAA, true );
	}
}
typedef FreeCaller1<int, CamMSAAImport> MSAAImportCaller;

void CamMSAAExport( const IntImportCallback& importer ){
	if( g_camwindow_globals_private.m_MSAA <= 0 ){
		importer( 0 );
	}
	else{
		int exponent = 1;
		while( !( ( g_camwindow_globals_private.m_MSAA >> exponent ) & 1 ) ){
			++exponent;
		}
		importer( exponent );
	}
}
typedef FreeCaller1<const IntImportCallback&, CamMSAAExport> MSAAExportCaller;

void fieldOfViewImport( float value ){
	camera_t::fieldOfView = value;
	if( g_camwnd ){
		Camera_updateProjection( g_camwnd->getCamera() );
	}
}
typedef FreeCaller1<float, fieldOfViewImport> fieldOfViewImportCaller;

void Camera_constructPreferences( PreferencesPage& page ){
	page.appendSlider( "Movement Speed", g_camwindow_globals_private.m_nMoveSpeed, TRUE, 0, 0, 500, 1, CAM_MAX_SPEED, 1, 10 );
	page.appendSlider( "Time to Max Speed", g_camwindow_globals_private.m_time_toMaxSpeed, TRUE, 0, 0, 200, 0, 5000, 10, 100 );
	page.appendSlider( "Scroll Move Speed", g_camwindow_globals_private.m_nScrollMoveSpeed, TRUE, 0, 0, 100, 0, 999, 1, 10 );
	page.appendSlider( "Strafe Speed", g_camwindow_globals_private.m_strafeSpeed, TRUE, 0, 0, 1, 0.1, 10, 0.1, 1 );
	page.appendSlider( "Mouse Sensitivity", g_camwindow_globals_private.m_angleSpeed, TRUE, 0, 0, 9, 0.1, 180, 0.1, 1 );
	page.appendCheckBox( "", "Invert mouse vertical axis", g_camwindow_globals_private.m_bCamInverseMouse );
	page.appendCheckBox( "", "Zoom In to Mouse pointer", g_camwindow_globals.m_bZoomInToPointer );
	page.appendCheckBox(
		"", "Discrete movement",
		FreeCaller1<bool, CamWnd_Move_Discrete_Import>(),
		BoolExportCaller( g_camwindow_globals_private.m_bCamDiscrete )
		);
	page.appendCheckBox(
		"", "Enable far-clip plane",
		FreeCaller1<bool, Camera_SetFarClip>(),
		BoolExportCaller( g_camwindow_globals_private.m_bCubicClipping )
		);
	page.appendCheckBox(
		"", "Colorize selection",
		BoolImportCaller( g_camwindow_globals_private.m_bFaceFill ),
		BoolExportCaller( g_camwindow_globals_private.m_bFaceFill )
		);
	page.appendCheckBox(
		"", "Selected faces wire",
		BoolImportCaller( g_camwindow_globals_private.m_bFaceWire ),
		BoolExportCaller( g_camwindow_globals_private.m_bFaceWire )
		);

	const char* render_modes[]{ "Wireframe", "Flatshade", "Textured", "Textured+Wire", "Lighting" };
	page.appendCombo(
		"Render Mode",
		StringArrayRange( render_modes, render_modes + ARRAY_SIZE( render_modes ) - ( g_pGameDescription->mGameType == "doom3"? 0 : 1 ) ),
		IntImportCallback( RenderModeImportCaller() ),
		IntExportCallback( RenderModeExportCaller() )
		);

	if( GlobalOpenGL().support_ARB_framebuffer_object ){
		const char* samples[] = { "0", "2", "4", "8", "16", "32" };

		page.appendCombo(
			"MSAA",
			STRING_ARRAY_RANGE( samples ),
			IntImportCallback( MSAAImportCaller() ),
			IntExportCallback( MSAAExportCaller() )
			);
	}

	const char* strafe_mode[] = { "None", "Up", "Forward", "Both", "Both Inverted" };

	page.appendCombo(
		"Strafe Mode",
		g_camwindow_globals_private.m_strafeMode,
		STRING_ARRAY_RANGE( strafe_mode )
		);

	page.appendSpinner(	"Field Of View", 110.0, 1.0, 175.0,
		FloatImportCallback( fieldOfViewImportCaller() ),
		FloatExportCallback( FloatExportCaller( camera_t::fieldOfView ) )
		);
}
void Camera_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Camera", "Camera View Preferences" ) );
	Camera_constructPreferences( page );
}
void Camera_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, Camera_constructPage>() );
}

#include "preferencesystem.h"
#include "stringio.h"
#include "dialog.h"

typedef FreeCaller1<bool, CamWnd_Move_Discrete_Import> CamWndMoveDiscreteImportCaller;

void CameraSpeed_increase(){
	g_camwindow_globals_private.m_nMoveSpeed = std::min( g_camwindow_globals_private.m_nMoveSpeed + CAM_SPEED_STEP, CAM_MAX_SPEED );
	globalOutputStream() << " ++Camera Move Speed: ";
	globalWarningStream() << g_camwindow_globals_private.m_nMoveSpeed << "\n";
}

void CameraSpeed_decrease(){
	g_camwindow_globals_private.m_nMoveSpeed = std::max( g_camwindow_globals_private.m_nMoveSpeed - CAM_SPEED_STEP, CAM_MIN_SPEED );
	globalOutputStream() << " --Camera Move Speed: ";
	globalWarningStream() << g_camwindow_globals_private.m_nMoveSpeed << "\n";
}

/// \brief Initialisation for things that have the same lifespan as this module.
void CamWnd_Construct(){
	GlobalCommands_insert( "CenterView", FreeCaller<GlobalCamera_ResetAngles>(), Accelerator( GDK_End ) );
	GlobalCommands_insert( "CameraFocusOnSelected", FreeCaller<GlobalCamera_FocusOnSelected>(), Accelerator( GDK_Tab ) );

	GlobalToggles_insert( "ToggleCubicClip", FreeCaller<Camera_ToggleFarClip>(), ToggleItem::AddCallbackCaller( g_getfarclip_item ), Accelerator( '\\', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "CubicClipZoomIn", FreeCaller<Camera_CubeIn>(), Accelerator( '[', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "CubicClipZoomOut", FreeCaller<Camera_CubeOut>(), Accelerator( ']', (GdkModifierType)GDK_CONTROL_MASK ) );

	GlobalCommands_insert( "UpFloor", FreeCaller<Camera_ChangeFloorUp>(), Accelerator( GDK_Prior ) );
	GlobalCommands_insert( "DownFloor", FreeCaller<Camera_ChangeFloorDown>(), Accelerator( GDK_Next ) );

	GlobalToggles_insert( "ToggleCamera", ToggleShown::ToggleCaller( g_camera_shown ), ToggleItem::AddCallbackCaller( g_camera_shown.m_item ), Accelerator( 'C', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
//	GlobalCommands_insert( "LookThroughSelected", FreeCaller<GlobalCamera_LookThroughSelected>() );
//	GlobalCommands_insert( "LookThroughCamera", FreeCaller<GlobalCamera_LookThroughCamera>() );

	if ( g_pGameDescription->mGameType == "doom3" ) {
		GlobalCommands_insert( "TogglePreview", FreeCaller<CamWnd_TogglePreview>(), Accelerator( GDK_F3 ) );
	}

	GlobalCommands_insert( "CameraModeNext", FreeCaller<CameraModeNext>(), Accelerator( '}', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "CameraModePrev", FreeCaller<CameraModePrev>(), Accelerator( '{', (GdkModifierType)GDK_SHIFT_MASK ) );

	GlobalCommands_insert( "CameraSpeedInc", FreeCaller<CameraSpeed_increase>(), Accelerator( GDK_KP_Add, (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "CameraSpeedDec", FreeCaller<CameraSpeed_decrease>(), Accelerator( GDK_KP_Subtract, (GdkModifierType)GDK_SHIFT_MASK ) );

	GlobalShortcuts_insert( "CameraForward", Accelerator( GDK_Up ) );
	GlobalShortcuts_insert( "CameraBack", Accelerator( GDK_Down ) );
	GlobalShortcuts_insert( "CameraLeft", Accelerator( GDK_Left ) );
	GlobalShortcuts_insert( "CameraRight", Accelerator( GDK_Right ) );
	GlobalShortcuts_insert( "CameraStrafeRight", Accelerator( 'D' ) );
	GlobalShortcuts_insert( "CameraStrafeLeft", Accelerator( 'A' ) );

	GlobalShortcuts_insert( "CameraUp", accelerator_null() );
	GlobalShortcuts_insert( "CameraDown", accelerator_null() );
	GlobalShortcuts_insert( "CameraAngleUp", accelerator_null() );
	GlobalShortcuts_insert( "CameraAngleDown", accelerator_null() );

	GlobalShortcuts_insert( "CameraFreeMoveForward", Accelerator( 'W' ) );
	GlobalShortcuts_insert( "CameraFreeMoveBack", Accelerator( 'S' ) );
	GlobalShortcuts_insert( "CameraFreeMoveLeft", Accelerator( 'A' ) );
	GlobalShortcuts_insert( "CameraFreeMoveRight", Accelerator( 'D' ) );

	GlobalShortcuts_insert( "CameraFreeMoveForward2", Accelerator( GDK_Up ) );
	GlobalShortcuts_insert( "CameraFreeMoveBack2", Accelerator( GDK_Down ) );
	GlobalShortcuts_insert( "CameraFreeMoveLeft2", Accelerator( GDK_Left ) );
	GlobalShortcuts_insert( "CameraFreeMoveRight2", Accelerator( GDK_Right ) );

	GlobalShortcuts_insert( "CameraFreeMoveUp", accelerator_null() );
	GlobalShortcuts_insert( "CameraFreeMoveDown", accelerator_null() );

	GlobalShortcuts_insert( "CameraFreeFocus", Accelerator( GDK_Tab ) );

	GlobalToggles_insert( "ShowStats", FreeCaller<ShowStatsToggle>(), ToggleItem::AddCallbackCaller( g_show_stats ) );
	GlobalToggles_insert( "ShowWorkzone3d", FreeCaller<ShowWorkzone3dToggle>(), ToggleItem::AddCallbackCaller( g_show_workzone3d ) );
	GlobalToggles_insert( "ShowSize3d", FreeCaller<ShowSize3dToggle>(), ToggleItem::AddCallbackCaller( g_show_size3d ) );

	GlobalPreferenceSystem().registerPreference( "ShowStats", BoolImportStringCaller( g_camwindow_globals.m_showStats ), BoolExportStringCaller( g_camwindow_globals.m_showStats ) );
	GlobalPreferenceSystem().registerPreference( "ShowWorkzone3d", BoolImportStringCaller( g_camwindow_globals_private.m_bShowWorkzone ), BoolExportStringCaller( g_camwindow_globals_private.m_bShowWorkzone ) );
	GlobalPreferenceSystem().registerPreference( "ShowSize3d", BoolImportStringCaller( g_camwindow_globals_private.m_bShowSize ), BoolExportStringCaller( g_camwindow_globals_private.m_bShowSize ) );
	GlobalPreferenceSystem().registerPreference( "CamMoveSpeed", IntImportStringCaller( g_camwindow_globals_private.m_nMoveSpeed ), IntExportStringCaller( g_camwindow_globals_private.m_nMoveSpeed ) );
	GlobalPreferenceSystem().registerPreference( "CamMoveTimeToMaxSpeed", IntImportStringCaller( g_camwindow_globals_private.m_time_toMaxSpeed ), IntExportStringCaller( g_camwindow_globals_private.m_time_toMaxSpeed ) );
	GlobalPreferenceSystem().registerPreference( "ScrollMoveSpeed", IntImportStringCaller( g_camwindow_globals_private.m_nScrollMoveSpeed ), IntExportStringCaller( g_camwindow_globals_private.m_nScrollMoveSpeed ) );
	GlobalPreferenceSystem().registerPreference( "CamStrafeSpeed", FloatImportStringCaller( g_camwindow_globals_private.m_strafeSpeed ), FloatExportStringCaller( g_camwindow_globals_private.m_strafeSpeed ) );
	GlobalPreferenceSystem().registerPreference( "Sensitivity", FloatImportStringCaller( g_camwindow_globals_private.m_angleSpeed ), FloatExportStringCaller( g_camwindow_globals_private.m_angleSpeed ) );
	GlobalPreferenceSystem().registerPreference( "CamInverseMouse", BoolImportStringCaller( g_camwindow_globals_private.m_bCamInverseMouse ), BoolExportStringCaller( g_camwindow_globals_private.m_bCamInverseMouse ) );
	GlobalPreferenceSystem().registerPreference( "CamDiscrete", makeBoolStringImportCallback( CamWndMoveDiscreteImportCaller() ), BoolExportStringCaller( g_camwindow_globals_private.m_bCamDiscrete ) );
	GlobalPreferenceSystem().registerPreference( "CubicClipping", BoolImportStringCaller( g_camwindow_globals_private.m_bCubicClipping ), BoolExportStringCaller( g_camwindow_globals_private.m_bCubicClipping ) );
	GlobalPreferenceSystem().registerPreference( "CubicScale", IntImportStringCaller( g_camwindow_globals.m_nCubicScale ), IntExportStringCaller( g_camwindow_globals.m_nCubicScale ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors4", Vector3ImportStringCaller( g_camwindow_globals.color_cameraback ), Vector3ExportStringCaller( g_camwindow_globals.color_cameraback ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors12", Vector3ImportStringCaller( g_camwindow_globals.color_selbrushes3d ), Vector3ExportStringCaller( g_camwindow_globals.color_selbrushes3d ) );
	GlobalPreferenceSystem().registerPreference( "CameraRenderMode", makeIntStringImportCallback( RenderModeImportCaller() ), makeIntStringExportCallback( RenderModeExportCaller() ) );
	GlobalPreferenceSystem().registerPreference( "CameraMSAA", IntImportStringCaller( g_camwindow_globals_private.m_MSAA ), IntExportStringCaller( g_camwindow_globals_private.m_MSAA ) );
	GlobalPreferenceSystem().registerPreference( "StrafeMode", IntImportStringCaller( g_camwindow_globals_private.m_strafeMode ), IntExportStringCaller( g_camwindow_globals_private.m_strafeMode ) );
	GlobalPreferenceSystem().registerPreference( "CameraFaceWire", BoolImportStringCaller( g_camwindow_globals_private.m_bFaceWire ), BoolExportStringCaller( g_camwindow_globals_private.m_bFaceWire ) );
	GlobalPreferenceSystem().registerPreference( "CameraFaceFill", BoolImportStringCaller( g_camwindow_globals_private.m_bFaceFill ), BoolExportStringCaller( g_camwindow_globals_private.m_bFaceFill ) );
	GlobalPreferenceSystem().registerPreference( "3DZoomInToPointer", BoolImportStringCaller( g_camwindow_globals.m_bZoomInToPointer ), BoolExportStringCaller( g_camwindow_globals.m_bZoomInToPointer ) );
	GlobalPreferenceSystem().registerPreference( "fieldOfView", FloatImportStringCaller( camera_t::fieldOfView ), FloatExportStringCaller( camera_t::fieldOfView ) );
	GlobalPreferenceSystem().registerPreference( "CamVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_camera_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_camera_shown ) ) );

	CamWnd_constructStatic();

	Camera_registerPreferencesPage();
}
void CamWnd_Destroy(){
	CamWnd_destroyStatic();
}
