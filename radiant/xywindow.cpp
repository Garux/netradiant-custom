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
// XY Window
//
// Leonardo Zide (leo@lokigames.com)
//

#include "xywindow.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "igl.h"
#include "ibrush.h"
#include "iundo.h"
#include "iimage.h"
#include "ifilesystem.h"
#include "os/path.h"
#include "image.h"
#include "gtkutil/messagebox.h"

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QTimer>

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"

#include "scenelib.h"
#include "eclasslib.h"
#include "renderer.h"
#include "moduleobserver.h"

#include "gtkutil/menu.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/fbo.h"
#include "gtkmisc.h"
#include "select.h"
#include "brushmanip.h"
#include "selection.h"
#include "entity.h"
#include "camwindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "commands.h"
#include "feedback.h"
#include "grid.h"
#include "windowobservers.h"

#include "render.h"

bool g_bCamEntityMenu = false;



struct xywindow_globals_private_t
{
	bool d_showgrid = true;

	// these are in the View > Show menu with Show coordinates
	bool show_names = false;
	bool show_coordinates = true;
	bool show_angles = true;
	bool show_outline = true;
	bool show_axis = true;

	bool show_workzone = false;

	bool show_blocks = false;

	bool m_bChaseMouse = true;
	bool m_bShowSize = true;

	int m_MSAA = 8;

	bool m_bZoomToPointer = true;
};

xywindow_globals_t g_xywindow_globals;
xywindow_globals_private_t g_xywindow_globals_private;

const unsigned int RAD_NONE =    0x00;
const unsigned int RAD_SHIFT =   0x01;
const unsigned int RAD_ALT =     0x02;
const unsigned int RAD_CONTROL = 0x04;
const unsigned int RAD_PRESS   = 0x08;
const unsigned int RAD_LBUTTON = 0x10;
const unsigned int RAD_MBUTTON = 0x20;
const unsigned int RAD_RBUTTON = 0x40;

inline ButtonIdentifier button_for_flags( unsigned int flags ){
	if ( flags & RAD_LBUTTON ) {
		return c_buttonLeft;
	}
	if ( flags & RAD_RBUTTON ) {
		return c_buttonRight;
	}
	if ( flags & RAD_MBUTTON ) {
		return c_buttonMiddle;
	}
	return c_buttonInvalid;
}

inline ModifierFlags modifiers_for_flags( unsigned int flags ){
	ModifierFlags modifiers = c_modifierNone;
	if ( flags & RAD_SHIFT ) {
		modifiers |= c_modifierShift;
	}
	if ( flags & RAD_CONTROL ) {
		modifiers |= c_modifierControl;
	}
	if ( flags & RAD_ALT ) {
		modifiers |= c_modifierAlt;
	}
	return modifiers;
}

inline unsigned int buttons_for_button_and_modifiers( ButtonIdentifier button, ModifierFlags flags ){
	unsigned int buttons = 0;

	switch ( button.get() )
	{
	case ButtonEnumeration::LEFT: buttons |= RAD_LBUTTON; break;
	case ButtonEnumeration::MIDDLE: buttons |= RAD_MBUTTON; break;
	case ButtonEnumeration::RIGHT: buttons |= RAD_RBUTTON; break;
	default: break;
	}

	if ( bitfield_enabled( flags, c_modifierControl ) ) {
		buttons |= RAD_CONTROL;
	}

	if ( bitfield_enabled( flags, c_modifierShift ) ) {
		buttons |= RAD_SHIFT;
	}

	if ( bitfield_enabled( flags, c_modifierAlt ) ) {
		buttons |= RAD_ALT;
	}

	return buttons;
}

inline unsigned int buttons_for_event_button( QMouseEvent* event ){
	unsigned int flags = 0;

	switch ( event->button() )
	{
	case Qt::MouseButton::LeftButton: flags |= RAD_LBUTTON; break;
	case Qt::MouseButton::MiddleButton: flags |= RAD_MBUTTON; break;
	case Qt::MouseButton::RightButton: flags |= RAD_RBUTTON; break;
	default : break;
	}

	if ( event->modifiers() & Qt::KeyboardModifier::ControlModifier ) {
		flags |= RAD_CONTROL;
	}

	if ( event->modifiers() & Qt::KeyboardModifier::ShiftModifier ) {
		flags |= RAD_SHIFT;
	}

	if ( event->modifiers() & Qt::KeyboardModifier::AltModifier ) {
		flags |= RAD_ALT;
	}

	return flags;
}

inline unsigned int buttons_for_state( const QMouseEvent& event ){
	unsigned int flags = 0;

	if ( event.buttons() & Qt::MouseButton::LeftButton ) {
		flags |= RAD_LBUTTON;
	}

	if ( event.buttons() & Qt::MouseButton::MiddleButton ) {
		flags |= RAD_MBUTTON;
	}

	if ( event.buttons() & Qt::MouseButton::RightButton ) {
		flags |= RAD_RBUTTON;
	}

	if ( event.modifiers() & Qt::KeyboardModifier::ControlModifier ) {
		flags |= RAD_CONTROL;
	}

	if ( event.modifiers() & Qt::KeyboardModifier::ShiftModifier ) {
		flags |= RAD_SHIFT;
	}

	if ( event.modifiers() & Qt::KeyboardModifier::AltModifier ) {
		flags |= RAD_ALT;
	}

	return flags;
}


void XYWnd::SetScale( float f ){
	const float max_scale = 64.f;
	const float min_scale = std::min( Width(), Height() )
	                        / ( ( 1.1f + ( 1.f - GetMaxGridCoord() / g_MaxWorldCoord ) ) // adaptive min scale factor: from 2.0375 with 4096 grid to 1.1 with 64*1024
	                        * 2.f * GetMaxGridCoord() );
	f = std::min( max_scale, std::max( min_scale, f ) );
	if( !float_equal_epsilon( m_fScale, f, float_mid( m_fScale, f ) * 1e-5f ) ){
		m_fScale = f;
		updateProjection();
		updateModelview();
		XYWnd_Update( *this );
	}
}

void XYWnd::ZoomIn(){
	SetScale( Scale() * 5.0f / 4.0f );
}

// NOTE: the zoom out factor is 4/5, we could think about customizing it
//  we don't go below a zoom factor corresponding to 10% of the max world size
//  (this has to be computed against the window size)
void XYWnd::ZoomOut(){
	SetScale( Scale() * 4.0f / 5.0f );
}

void XYWnd::ZoomInWithMouse( int x, int y ){
	const float old_scale = Scale();
	ZoomIn();
	ZoomCompensateOrigin( x, y, old_scale );
}
void XYWnd::ZoomOutWithMouse( int x, int y ){
	const float old_scale = Scale();
	ZoomOut();
	ZoomCompensateOrigin( x, y, old_scale );
}
void XYWnd::ZoomCompensateOrigin( int x, int y, float old_scale ){
	if ( g_xywindow_globals_private.m_bZoomToPointer && old_scale != Scale() ) {
		const float scale_diff = 1.0 / old_scale - 1.0 / Scale();
		NDIM1NDIM2( m_viewType )
		Vector3 origin = GetOrigin();
		origin[nDim1] += scale_diff * ( x - 0.5 * Width() );
		origin[nDim2] -= scale_diff * ( y - 0.5 * Height() );
		SetOrigin( origin );
	}
}

void XYWnd::FocusOnBounds( const AABB& bounds ){
	SetOrigin( bounds.origin );
	NDIM1NDIM2( m_viewType )
	SetScale( std::min( Width() / ( 3.f * std::max( 128.f, bounds.extents[ nDim1 ] ) ),
	                    Height() / ( 3.f * std::max( 128.f, bounds.extents[ nDim2 ] ) ) ) );

}

VIEWTYPE GlobalXYWnd_getCurrentViewType(){
	ASSERT_NOTNULL( g_pParentWnd );
	ASSERT_NOTNULL( g_pParentWnd->ActiveXY() );
	return g_pParentWnd->ActiveXY()->GetViewType();
}

// =============================================================================
// variables

bool g_bCrossHairs = false;

QMenu* XYWnd::m_mnuDrop = 0;


#include "timer.h"
static Timer g_chasemouse_timer;
static QTimer g_chasemouse_caller;

void XYWnd::ChaseMouse(){
	const float multiplier = g_chasemouse_timer.elapsed_msec() / 10.0f;
	g_chasemouse_timer.start();
	Scroll( float_to_integer( multiplier * m_chasemouse_delta_x ), float_to_integer( multiplier * -m_chasemouse_delta_y ) );
	//globalOutputStream() << "chasemouse: multiplier=" << multiplier << " x=" << m_chasemouse_delta_x << " y=" << m_chasemouse_delta_y << '\n';

	XY_MouseMoved( m_chasemouse_current_x, m_chasemouse_current_y, getButtonState() );
}

bool XYWnd::chaseMouseMotion( const int x, const int y ){
	m_chasemouse_delta_x = 0;
	m_chasemouse_delta_y = 0;

	if ( g_xywindow_globals_private.m_bChaseMouse && getButtonState() == RAD_LBUTTON ) {
		const int epsilon = 16;

		if ( x < epsilon ) {
			m_chasemouse_delta_x = std::max( x, 0 ) - epsilon;
		}
		else if ( ( x - m_nWidth ) > -epsilon ) {
			m_chasemouse_delta_x = std::min( ( x - m_nWidth ), 0 ) + epsilon;
		}

		if ( y < epsilon ) {
			m_chasemouse_delta_y = std::max( y, 0 ) - epsilon;
		}
		else if ( ( y - m_nHeight ) > -epsilon ) {
			m_chasemouse_delta_y = std::min( ( y - m_nHeight ), 0 ) + epsilon;
		}

		if ( m_chasemouse_delta_y != 0 || m_chasemouse_delta_x != 0 ) {
			//globalOutputStream() << "chasemouse motion: x=" << x << " y=" << y << "... ";
			m_chasemouse_current_x = x;
			m_chasemouse_current_y = y;
			if ( !g_chasemouse_caller.isActive() ) {
				//globalOutputStream() << "chasemouse timer start... ";
				g_chasemouse_timer.start();
				g_chasemouse_caller.disconnect(); // disconnect everything
				g_chasemouse_caller.callOnTimeout( [this](){ ChaseMouse(); } );
				g_chasemouse_caller.start( 4 ); // with 0 consumes entire thread by spamming calls 🤷‍♀️
			}
			return true;
		}
		else
		{
			// if( g_chasemouse_caller.isActive() ) globalOutputStream() << "chasemouse cancel\n";
			g_chasemouse_caller.stop();
		}
	}
	else
	{
		// if( g_chasemouse_caller.isActive() ) globalOutputStream() << "chasemouse cancel\n";
		g_chasemouse_caller.stop();
	}
	return false;
}

// =============================================================================
// XYWnd class
Shader* XYWnd::m_state_selected = 0;

//outline camera crosshair rectangle
void XYWnd::overlayDraw(){
	gl().glViewport( 0, 0, m_nWidth, m_nHeight );

	gl().glDisable( GL_LINE_STIPPLE );
	gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	gl().glDisableClientState( GL_NORMAL_ARRAY );
	gl().glDisableClientState( GL_COLOR_ARRAY );
	gl().glDisable( GL_TEXTURE_2D );
	gl().glDisable( GL_LIGHTING );
	gl().glDisable( GL_COLOR_MATERIAL );
	gl().glDisable( GL_DEPTH_TEST );
	gl().glDisable( GL_TEXTURE_1D );

//	gl().glDisable( GL_BLEND );
	gl().glLineWidth( 1 );

	if ( g_xywindow_globals_private.show_outline && Active() ) {
		gl().glMatrixMode( GL_PROJECTION );
		gl().glLoadIdentity();
		gl().glOrtho( 0, m_nWidth, 0, m_nHeight, 0, 1 );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glLoadIdentity();

		// four view mode doesn't colorize
		( g_pParentWnd->CurrentStyle() == MainFrame::eSplit )
			? gl().glColor3fv( vector3_to_array( g_xywindow_globals.color_viewname ) )
			: gl().glColor3ubv( m_viewType == YZ? &g_colour_x.r
			                  : m_viewType == XZ? &g_colour_y.r
			                  :                   &g_colour_z.r );
		gl().glBegin( GL_LINE_LOOP );
		gl().glVertex2f( 0.5, 0.5 );
		gl().glVertex2f( m_nWidth - 0.5, 0.5 );
		gl().glVertex2f( m_nWidth - 0.5, m_nHeight - 0.5 );
		gl().glVertex2f( 0.5, m_nHeight - 0.5 );
		gl().glEnd();
	}

	{
		NDIM1NDIM2( m_viewType )

		gl().glMatrixMode( GL_PROJECTION );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glLoadIdentity();
		gl().glScalef( m_fScale, m_fScale, 1 );
		gl().glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );
		DrawCameraIcon( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ), Camera_getAngles( *g_pParentWnd->GetCamWnd() ) );
	}

	if ( g_bCrossHairs ) {
		gl().glMatrixMode( GL_PROJECTION );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

		NDIM1NDIM2( m_viewType )
		Vector3 v( g_vector3_identity );
		gl().glColor4f( 0.2f, 0.9f, 0.2f, 0.8f );
		gl().glBegin( GL_LINES );
		for( int i = 0, dim1 = nDim1, dim2 = nDim2; i < 2; ++i, std::swap( dim1, dim2 ) ){
			v[dim1] = m_mousePosition[dim1];
			v[dim2] = 2.0f * -GetMaxGridCoord();
			gl().glVertex3fv( vector3_to_array( v ) );
			v[dim2] = 2.0f * GetMaxGridCoord();
			gl().glVertex3fv( vector3_to_array( v ) );
		}
		gl().glEnd();
	}

	m_XORRectangle.render( m_XORRect, Width(), Height() );
}

void xy_update_xor_rectangle( XYWnd& self, rect_t area ){
	self.m_XORRect = area;
	self.queueDraw();
}


class XYGLWidget : public QOpenGLWidget
{
	XYWnd& m_xywnd;
	DeferredMotion m_deferred_motion;
	FBO *m_fbo{};
	qreal m_scale;
public:
	XYGLWidget( XYWnd& xywnd ) : QOpenGLWidget(), m_xywnd( xywnd ),
		m_deferred_motion( [this]( const QMouseEvent& event ){
				if ( m_xywnd.chaseMouseMotion( event.x() * m_scale, event.y() * m_scale ) ) {
					return;
				}
				m_xywnd.XY_MouseMoved( event.x() * m_scale, event.y() * m_scale, buttons_for_state( event ) );
			} )
	{
		setMouseTracking( true );
	}

	~XYGLWidget() override {
		delete m_fbo;
		glwidget_context_destroyed();
	}

protected:
	void initializeGL() override
	{
		glwidget_context_created( *this );
	}
	void resizeGL( int w, int h ) override
	{
		m_scale = devicePixelRatioF();
		m_xywnd.m_nWidth = float_to_integer( w * m_scale );
		m_xywnd.m_nHeight = float_to_integer( h * m_scale );
		m_xywnd.updateProjection();
		m_xywnd.m_window_observer->onSizeChanged( m_xywnd.Width(), m_xywnd.Height() );

		m_xywnd.m_drawRequired = true;

		delete m_fbo;
		m_fbo = new FBO( m_xywnd.Width(), m_xywnd.Height(), false, g_xywindow_globals_private.m_MSAA );
	}
	void paintGL() override
	{
		if( m_fbo->m_samples != g_xywindow_globals_private.m_MSAA ){
			delete m_fbo;
			m_fbo = new FBO( m_xywnd.Width(), m_xywnd.Height(), false, g_xywindow_globals_private.m_MSAA );
		}

		if ( Map_Valid( g_map ) && ScreenUpdates_Enabled() && m_fbo->bind() ) {
			GlobalOpenGL_debugAssertNoErrors();
			if( m_xywnd.m_drawRequired ){
				m_xywnd.m_drawRequired = false;
				m_xywnd.XY_Draw();
				GlobalOpenGL_debugAssertNoErrors();
			}
			m_fbo->blit();
			m_fbo->release();
			m_xywnd.overlayDraw();
			GlobalOpenGL_debugAssertNoErrors();
		}
	}

	void mousePressEvent( QMouseEvent *event ) override {
		setFocus();

		if( !m_xywnd.Active() ){
			g_pParentWnd->SetActiveXY( &m_xywnd );
		}

		m_xywnd.ButtonState_onMouseDown( buttons_for_event_button( event ) );

		m_xywnd.onMouseDown( WindowVector( event->x(), event->y() ) * m_scale, button_for_button( event->button() ), modifiers_for_state( event->modifiers() ) );
	}
	void mouseMoveEvent( QMouseEvent *event ) override {
		m_deferred_motion.motion( event );
	}
	void mouseReleaseEvent( QMouseEvent *event ) override {
		m_xywnd.XY_MouseUp( event->x() * m_scale, event->y() * m_scale, buttons_for_event_button( event ) );

		m_xywnd.ButtonState_onMouseUp( buttons_for_event_button( event ) );

		m_xywnd.chaseMouseMotion( event->x() * m_scale, event->y() * m_scale ); /* stop chaseMouseMotion this way */
	}
	void wheelEvent( QWheelEvent *event ) override {
		setFocus();
		QWidget* window = m_xywnd.m_parent != 0 ? m_xywnd.m_parent : MainFrame_getWindow();
		if( !window->isActiveWindow() ){
			window->activateWindow();
			window->raise();
		}

		if( !m_xywnd.Active() ){
			g_pParentWnd->SetActiveXY( &m_xywnd );
		}
		if ( event->angleDelta().y() > 0 ) {
			m_xywnd.ZoomInWithMouse( event->position().x() * m_scale, event->position().y() * m_scale );
		}
		else if ( event->angleDelta().y() < 0 ) {
			m_xywnd.ZoomOutWithMouse( event->position().x() * m_scale, event->position().y() * m_scale );
		}
	}

	void focusInEvent( QFocusEvent *event ) override {
		if( !m_xywnd.Active() ){
			g_pParentWnd->SetActiveXY( &m_xywnd );
		}
	}
};

XYWnd::XYWnd() :
	m_gl_widget( new XYGLWidget( *this ) ),
	m_deferredDraw( WidgetQueueDrawCaller( *m_gl_widget ) ),
	m_parent( 0 ),
	m_window_observer( NewWindowObserver() )
{
	m_bActive = false;
	m_buttonstate = 0;

	m_bNewBrushDrag = false;
	m_move_started = false;
	m_zoom_started = false;

	m_nWidth = 0;
	m_nHeight = 0;

	m_vOrigin[0] = 0;
	m_vOrigin[1] = 20;
	m_vOrigin[2] = 46;
	m_fScale = 1;
	m_viewType = XY;

	m_entityCreate = false;

	GlobalWindowObservers_add( m_window_observer );
	GlobalWindowObservers_connectWidget( m_gl_widget );

	m_window_observer->setRectangleDrawCallback( ReferenceCaller<XYWnd, void(rect_t), xy_update_xor_rectangle>( *this ) );
	m_window_observer->setView( m_view );

	Map_addValidCallback( g_map, DeferredDrawOnMapValidChangedCaller( m_deferredDraw ) ); //. correct would be m_drawRequired = true here

	updateProjection();
	updateModelview();

	AddSceneChangeCallback( ReferenceCaller<XYWnd, void(), &XYWnd_Update>( *this ) );
	AddCameraMovedCallback( MemberCaller<XYWnd, void(), &XYWnd::queueDraw>( *this ) );

	onMouseDown.connectLast( makeSignalHandler3( MouseDownCaller(), *this ) );
}

XYWnd::~XYWnd(){
	onDestroyed();

	m_window_observer->release();
}

void XYWnd::captureStates(){
	m_state_selected = GlobalShaderCache().capture( "$XY_OVERLAY" );
}

void XYWnd::releaseStates(){
	GlobalShaderCache().release( "$XY_OVERLAY" );
}

const Vector3& XYWnd::GetOrigin() const {
	return m_vOrigin;
}

void XYWnd::SetOrigin( const Vector3& origin ){
	for( std::size_t i = 0; i < 3; ++i )
		m_vOrigin[i] = std::min( GetMaxGridCoord(), std::max( -GetMaxGridCoord(), origin[i] ) );
	updateModelview();
	XYWnd_Update( *this );
}

void XYWnd::Scroll( int x, int y ){
	NDIM1NDIM2( m_viewType )

	m_vOrigin[nDim1] += x / m_fScale;
	m_vOrigin[nDim2] += y / m_fScale;

	SetOrigin( m_vOrigin );
}

void XYWnd::SetCustomPivotOrigin( int x, int y ) const {
	bool set[3] = { true, true, true };
	set[GetViewType()] = false;
	GlobalSelectionSystem().setCustomTransformOrigin( XY_ToPoint( x, y ), set );
	SceneChangeNotify();
}

unsigned int MoveCamera_buttons(){
	return RAD_CONTROL | RAD_MBUTTON;
}

void XYWnd_PositionCamera( XYWnd* xywnd, int x, int y, CamWnd& camwnd ){
	Vector3 origin = xywnd->XY_ToPoint( x, y, true );
	origin[xywnd->GetViewType()] = Camera_getOrigin( camwnd )[xywnd->GetViewType()];
	Camera_setOrigin( camwnd, origin );
}

unsigned int OrientCamera_buttons(){
	return RAD_MBUTTON;
}

void XYWnd_OrientCamera( XYWnd* xywnd, int x, int y, CamWnd& camwnd ){
	//globalOutputStream() << Camera_getAngles( camwnd ) << "  b4\n";
	const Vector3 point = xywnd->XY_ToPoint( x, y ) - Camera_getOrigin( camwnd );
	const VIEWTYPE viewtype = xywnd->GetViewType();
	NDIM1NDIM2( viewtype )
	const int nAngle = ( viewtype == XY ) ? CAMERA_YAW : CAMERA_PITCH;
	if ( point[nDim2] || point[nDim1] ) {
		Vector3 angles( Camera_getAngles( camwnd ) );
		angles[nAngle] = static_cast<float>( radians_to_degrees( atan2( point[nDim2], point[nDim1] ) ) );
		if( angles[CAMERA_YAW] < 0 )
			angles[CAMERA_YAW] += 360;
		if ( nAngle == CAMERA_PITCH ){
			if( fabs( angles[CAMERA_PITCH] ) > 90 ){
				angles[CAMERA_PITCH] = ( angles[CAMERA_PITCH] > 0 ) ? ( -angles[CAMERA_PITCH] + 180 ) : ( -angles[CAMERA_PITCH] - 180 );
				if( viewtype == YZ ){
					if( angles[CAMERA_YAW] < 180 ){
						angles[CAMERA_YAW] = 360 - angles[CAMERA_YAW];
					}
				}
				else if( angles[CAMERA_YAW] < 90 || angles[CAMERA_YAW] > 270 ){
					angles[CAMERA_YAW] = 180 - angles[CAMERA_YAW];
				}
			}
			else{
				if( viewtype == YZ ){
					if( angles[CAMERA_YAW] > 180 ){
						angles[CAMERA_YAW] = 360 - angles[CAMERA_YAW];
					}
				}
				else if( angles[CAMERA_YAW] > 90 && angles[CAMERA_YAW] < 270 ){
					angles[CAMERA_YAW] = 180 - angles[CAMERA_YAW];
				}
			}
		}
		Camera_setAngles( camwnd, angles );
	}
	//globalOutputStream() << Camera_getAngles( camwnd ) << '\n';
}

unsigned int SetCustomPivotOrigin_buttons(){
	return RAD_MBUTTON | RAD_SHIFT;
}

/*
   ==============
   NewBrushDrag
   ==============
 */
unsigned int NewBrushDrag_buttons(){
	return RAD_LBUTTON;
}

void XYWnd::NewBrushDrag_Begin( int x, int y ){
	m_NewBrushDrag = 0;
	m_nNewBrushPress = XY_ToPoint( x, y, true );

	m_bNewBrushDrag = true;
}

void XYWnd::NewBrushDrag_End( int x, int y ){
	if ( m_NewBrushDrag != 0 ) {
		GlobalUndoSystem().finish( "brushDragNew" );
	}
}

void XYWnd::NewBrushDrag( int x, int y, bool square, bool cube ){
	Vector3 mins = m_nNewBrushPress;
	Vector3 maxs = XY_ToPoint( x, y, true );
	const Vector3 maxs_real = XY_ToPoint( x, y );

	const int nDim = GetViewType();
	NDIM1NDIM2( nDim );

	// avoid snapping to zero bounds
	// if brush is already inserted or move is decent nuff
	if( m_NewBrushDrag != nullptr || vector3_length( maxs_real - mins ) > GetSnapGridSize() / sqrt( 2.0 ) )
		for( auto i : { nDim1, nDim2 } )
			if( maxs[i] == mins[i] )
				maxs[i] = mins[i] + std::copysign( GetSnapGridSize(), maxs_real[i] - mins[i] );

	mins[nDim] = float_snapped( Select_getWorkZone().d_work_min[nDim], GetSnapGridSize() );
	maxs[nDim] = float_snapped( Select_getWorkZone().d_work_max[nDim], GetSnapGridSize() );

	if ( maxs[nDim] <= mins[nDim] ) {
		maxs[nDim] = mins[nDim] + GetGridSize();
	}

	if( square || cube ){
		const float squaresize = std::max( fabs( maxs[nDim1] - mins[nDim1] ), fabs( maxs[nDim2] - mins[nDim2] ) );
		for( auto i : { nDim1, nDim2 } )
			maxs[i] = mins[i] + std::copysign( squaresize, maxs[i] - mins[i] );
		if( cube ){
			maxs[nDim] = mins[nDim] + squaresize;
		}
	}

	for ( int i = 0; i < 3; i++ )
	{
		if ( mins[i] == maxs[i] )
			return; // don't create a degenerate brush
		if ( mins[i] > maxs[i] )
			std::swap( mins[i], maxs[i] );
	}

	if ( m_NewBrushDrag == 0 )
		GlobalUndoSystem().start();

	Scene_BrushResize_Cuboid( m_NewBrushDrag, aabb_for_minmax( mins, maxs ) );
}


static int g_entityCreationOffset = 0;

void entitycreate_activated( const QAction *action ){
	if( g_bCamEntityMenu ){
		const Vector3 viewvector = -Camera_getViewVector( *g_pParentWnd->GetCamWnd() );
		const float offset_for_multiple = std::max( GetSnapGridSize(), 8.f ) * g_entityCreationOffset;
		Vector3 point = viewvector * ( 64.f + offset_for_multiple ) + Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
		vector3_snap( point, GetSnapGridSize() );
		Entity_createFromSelection( action->text().toLatin1().constData(), point );
	}
	else{
		g_pParentWnd->ActiveXY()->OnEntityCreate( action->text().toLatin1().constData() );
	}
	++g_entityCreationOffset;
}

class EntityMenu : public QMenu
{
	bool m_mouse_handled{};
	bool m_hide_menu;
public:
	EntityMenu( const char* name ) : QMenu( name ){
	}
protected:
	void mousePressEvent( QMouseEvent *event ) override {
		/* create entities, don't close menu */
		if( event->button() == Qt::MouseButton::LeftButton && event->modifiers() == Qt::KeyboardModifier::ControlModifier ){
			if( QAction *action = actionAt( event->pos() ) ){
				if( action->menu() == nullptr ){
					m_mouse_handled = true;
					m_hide_menu = false;
					entitycreate_activated( action );
					return;
				}
			}
		}
		/* convert entities */
		else if( event->button() == Qt::MouseButton::RightButton ){
			if( QAction *action = actionAt( event->pos() ) ){
				if( action->menu() == nullptr ){
					m_mouse_handled = true;
					m_hide_menu = ( event->modifiers() != Qt::KeyboardModifier::ControlModifier );
					Scene_EntitySetClassname_Selected( action->text().toLatin1().constData() );
					return;
				}
			}
		}
		m_mouse_handled = false;
		m_hide_menu = false;
		QMenu::mousePressEvent( event );
	}
	void mouseReleaseEvent( QMouseEvent *event ) override {
		if( m_mouse_handled ){
			m_mouse_handled = false; // reset, so releases w/o press and clicks on non functional items take standard path
			if( m_hide_menu ){
				XYWnd::m_mnuDrop->hide();
			}
			return;
		}
		QMenu::mouseReleaseEvent( event );
	}
};

class EntityClassMenuInserter : public EntityClassVisitor
{
	typedef std::pair<QMenu*, CopiedString> MenuPair;
	typedef std::vector<MenuPair> MenuStack;
	MenuStack m_stack;
	CopiedString m_previous;
public:
	EntityClassMenuInserter( QMenu* menu ){
		m_stack.reserve( 2 );
		m_stack.push_back( MenuPair( menu, "" ) );
	}
	~EntityClassMenuInserter(){
		if ( !m_previous.empty() ) {
			addItem( m_previous.c_str(), "" );
		}
	}
	void visit( EntityClass* e ){
		ASSERT_MESSAGE( !string_empty( e->name() ), "entity-class has no name" );
		if ( !m_previous.empty() ) {
			addItem( m_previous.c_str(), e->name() );
		}
		m_previous = e->name();
	}
	void pushMenu( const CopiedString& name ){
		QMenu *submenu = new EntityMenu( name.c_str() );
		m_stack.back().first->addMenu( submenu );
		m_stack.push_back( MenuPair( submenu, name ) );
	}
	void popMenu(){
		m_stack.pop_back();
	}
	void addItem( const char* name, const char* next ){
		const char* underscore = strchr( name, '_' );

		if ( underscore != 0 && underscore != name ) {
			bool nextEqual = string_equal_n( name, next, ( underscore + 1 ) - name );
			const char* parent = m_stack.back().second.c_str();

			if ( !string_empty( parent )
			     && string_length( parent ) == std::size_t( underscore - name )
			     && string_equal_n( name, parent, underscore - name ) ) { // this is a child
			}
			else if ( nextEqual ) {
				if ( m_stack.size() == 2 ) {
					popMenu();
				}
				pushMenu( CopiedString( StringRange( name, underscore ) ) );
			}
			else if ( m_stack.size() == 2 ) {
				popMenu();
			}
		}
		else if ( m_stack.size() == 2 ) {
			popMenu();
		}

		m_stack.back().first->addAction( name );
	}
};

void XYWnd::OnContextMenu(){
	if ( m_mnuDrop == 0 ) { // first time, load it up
		m_mnuDrop = new EntityMenu( "" );
//		m_mnuDrop->setTearOffEnabled( true ); // problematic mouse events override in torn off menu
		QObject::connect( m_mnuDrop, &QMenu::triggered, entitycreate_activated ); //will receive submenu actions too

		EntityClassMenuInserter inserter( m_mnuDrop );
		GlobalEntityClassManager().forEach( inserter );
	}

	g_entityCreationOffset = 0;
	g_bCamEntityMenu = false;
	m_mnuDrop->popup( QCursor::pos() );
}

static FreezePointer g_xywnd_freezePointer;

unsigned int Move_buttons(){
	return RAD_RBUTTON;
}

void XYWnd::Move_Begin(){
	if ( m_move_started ) {
		Move_End();
	}
	m_move_started = true;
	g_xywnd_freezePointer.freeze_pointer( m_gl_widget,
		[this]( int x, int y, const QMouseEvent *event ){
			EntityCreate_MouseMove( x, y );
			Scroll( -x, y );
		},
		[this](){
			Move_End();
		} );
}

void XYWnd::Move_End(){
	m_move_started = false;
	g_xywnd_freezePointer.unfreeze_pointer( false );
}

unsigned int Zoom_buttons(){
	return RAD_RBUTTON | RAD_ALT;
}

static int g_dragZoom = 0;
static int g_zoom2x = 0;
static int g_zoom2y = 0;

void XYWnd::Zoom_Begin( int x, int y ){
	if ( m_zoom_started ) {
		Zoom_End();
	}
	m_zoom_started = true;
	g_dragZoom = 0;
	g_zoom2x = x;
	g_zoom2y = y;
	g_xywnd_freezePointer.freeze_pointer( m_gl_widget,
		[this]( int x, int y, const QMouseEvent *event ){
			if ( y != 0 ) {
				g_dragZoom += y;
				const int threshold = 16;
				while ( abs( g_dragZoom ) > threshold )
				{
					if ( g_dragZoom > 0 ) {
						ZoomOutWithMouse( g_zoom2x, g_zoom2y );
						g_dragZoom -= threshold;
					}
					else
					{
						ZoomInWithMouse( g_zoom2x, g_zoom2y );
						g_dragZoom += threshold;
					}
				}
			}
		},
		[this](){
			Zoom_End();
		} );
}

void XYWnd::Zoom_End(){
	m_zoom_started = false;
	g_xywnd_freezePointer.unfreeze_pointer( false );
}

void XYWnd::SetViewType( VIEWTYPE viewType ){
	m_viewType = viewType;
	updateModelview();

	if ( m_parent != 0 ) {
		m_parent->setWindowTitle( ViewType_getTitle( m_viewType ) );
	}
}


bool isClipperMode(){
	return GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip;
}

void XYWnd::mouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ){
	XY_MouseDown( static_cast<int>( position.x() ), static_cast<int>( position.y() ), buttons_for_button_and_modifiers( button, modifiers ) );
}
void XYWnd::XY_MouseDown( int x, int y, unsigned int buttons ){
	if ( buttons == Move_buttons() ) {
		Move_Begin();
		EntityCreate_MouseDown( x, y );
	}
	else if ( buttons == Zoom_buttons() ) {
		Zoom_Begin( x, y );
	}
	else if ( buttons == NewBrushDrag_buttons() && GlobalSelectionSystem().countSelected() == 0 && !isClipperMode() ) {
		NewBrushDrag_Begin( x, y );
	}
	// control mbutton = move camera
	else if ( buttons == MoveCamera_buttons() ) {
		XYWnd_PositionCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}
	// mbutton = angle camera
	else if ( buttons == OrientCamera_buttons() ) {
		XYWnd_OrientCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}
	else if ( buttons == SetCustomPivotOrigin_buttons() ) {
		SetCustomPivotOrigin( x, y );
	}
	else
	{
		m_window_observer->onMouseDown( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
	}
}

void XYWnd::XY_MouseUp( int x, int y, unsigned int buttons ){
	if ( m_move_started ) {
		Move_End();
		EntityCreate_MouseUp( x, y );
	}
	else if ( m_zoom_started ) {
		Zoom_End();
	}
	else if ( m_bNewBrushDrag ) {
		m_bNewBrushDrag = false;
		NewBrushDrag_End( x, y );
		if ( m_NewBrushDrag == 0 ) {
			//L button w/o created brush = tunnel selection
			m_window_observer->onMouseUp( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
		}
	}
	else
	{
		m_window_observer->onMouseUp( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
	}
}

void XYWnd::XY_MouseMoved( int x, int y, unsigned int buttons ){
	m_mousePosition = XY_ToPoint( x, y, true );

	// rbutton = drag xy origin
	if ( m_move_started ) {
	}
	// zoom in/out
	else if ( m_zoom_started ) {
	}

	// lbutton without selection = drag new brush
	else if ( m_bNewBrushDrag ) {
		NewBrushDrag( x, y, buttons & RAD_SHIFT, buttons & RAD_CONTROL );
	}

	// control mbutton = move camera
	else if ( buttons == MoveCamera_buttons() ) {
		XYWnd_PositionCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}

	// mbutton = angle camera
	else if ( buttons == OrientCamera_buttons() ) {
		XYWnd_OrientCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}

	else if ( buttons == SetCustomPivotOrigin_buttons() ) {
		SetCustomPivotOrigin( x, y );
	}

	else
	{
		m_window_observer->onMouseMotion( WindowVector( x, y ), modifiers_for_flags( buttons ) );

		{
			const auto status = StringStream<64>( "x:: ", FloatFormat( m_mousePosition[0], 6, 1 ),
			                                    "  y:: ", FloatFormat( m_mousePosition[1], 6, 1 ),
			                                    "  z:: ", FloatFormat( m_mousePosition[2], 6, 1 ) );
			g_pParentWnd->SetStatusText( c_status_position, status );
		}

		if ( g_bCrossHairs && button_for_flags( buttons ) == c_buttonInvalid ) { // don't update with a button pressed, observer calls update itself
			queueDraw();
		}
	}
}

void XYWnd::EntityCreate_MouseDown( int x, int y ){
	m_entityCreate = true;
	m_entityCreate_x = x;
	m_entityCreate_y = y;
}

void XYWnd::EntityCreate_MouseMove( int x, int y ){
	if ( m_entityCreate && ( m_entityCreate_x != x || m_entityCreate_y != y ) ) {
		m_entityCreate = false;
	}
}

void XYWnd::EntityCreate_MouseUp( int x, int y ){
	if ( m_entityCreate ) {
		m_entityCreate = false;
		OnContextMenu();
	}
}

inline float screen_normalised( int pos, unsigned int size ){
	return ( ( 2.0f * pos ) / size ) - 1.0f;
}

inline float normalised_to_world( float normalised, float world_origin, float normalised2world_scale ){
	return world_origin + normalised * normalised2world_scale;
}


Vector3 XYWnd::XY_ToPoint( int x, int y, bool snap /* = false */ ) const {
	Vector3 point( g_vector3_identity );
	const float normalised2world_scale_x = m_nWidth / 2 / m_fScale;
	const float normalised2world_scale_y = m_nHeight / 2 / m_fScale;
	NDIM1NDIM2( m_viewType )
	point[nDim1] = normalised_to_world( screen_normalised( x, m_nWidth ), m_vOrigin[nDim1], normalised2world_scale_x );
	point[nDim2] = normalised_to_world( -screen_normalised( y, m_nHeight ), m_vOrigin[nDim2], normalised2world_scale_y );
	return snap? vector3_snapped( point, GetSnapGridSize() ) : point;
}


void BackgroundImage::render( const VIEWTYPE viewtype ){
	if( viewtype == _viewtype && _tex > 0 ){
//		gl().glPushAttrib( GL_ALL_ATTRIB_BITS ); //bug with intel

		gl().glActiveTexture( GL_TEXTURE0 );
		gl().glClientActiveTexture( GL_TEXTURE0 );

		gl().glEnable( GL_TEXTURE_2D );
		gl().glEnable( GL_BLEND );
		gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		gl().glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
//		gl().glPolygonMode( GL_FRONT, GL_FILL );
		gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		gl().glDisable( GL_CULL_FACE );
		gl().glDisable( GL_DEPTH_TEST );

		gl().glBindTexture( GL_TEXTURE_2D, _tex );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

		gl().glBegin( GL_QUADS );

		gl().glColor4f( 1, 1, 1, _alpha );
		gl().glTexCoord2f( 0, 1 );
		gl().glVertex2f( _xmin, _ymin );

		gl().glTexCoord2f( 1, 1 );
		gl().glVertex2f( _xmax, _ymin );

		gl().glTexCoord2f( 1, 0 );
		gl().glVertex2f( _xmax, _ymax );

		gl().glTexCoord2f( 0, 0 );
		gl().glVertex2f( _xmin, _ymax );

		gl().glEnd();
		gl().glBindTexture( GL_TEXTURE_2D, 0 );

//		gl().glPopAttrib();
	}
}

#include "qe3.h"
#include "os/file.h"
const char* BackgroundImage::background_image_dialog(){
	auto buffer = StringStream( g_qeglobals.m_userGamePath, "textures/" );

	if ( !file_readable( buffer ) ) {
		// just go to fsmain
		buffer( g_qeglobals.m_userGamePath );
	}

	const char *filename = file_dialog( MainFrame_getWindow(), true, "Background Image", buffer );
	if ( filename != 0 ) {
		// use VFS to get the correct relative path
		const char* relative = path_make_relative( filename, GlobalFileSystem().findRoot( filename ) );
		if ( relative == filename ) {
			globalWarningStream() << "WARNING: could not extract the relative path, using full path instead\n";
		}
		return relative;
	}
	return 0;
}

void BackgroundImage::free_tex(){
	if( _tex > 0 ){
		gl().glDeleteTextures( 1, &_tex );
		_tex = 0;
	}
}

#include "texturelib.h"
void LoadTextureRGBA( qtexture_t* q, unsigned char* pPixels, int nWidth, int nHeight );

void BackgroundImage::set( const VIEWTYPE viewtype ){
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	NDIM1NDIM2( viewtype )
	if( !( bounds.extents[nDim1] > 0 && bounds.extents[nDim2] > 0 ) ){
		qt_MessageBox( MainFrame_getWindow(), "Select some objects to get the bounding box for image.\n",
		                "No selection", EMessageBoxType::Error );
	}
	else{
		free_tex();
		const char *filename = background_image_dialog();
		if( filename ){
			const auto filename_noext = StringStream( PathExtensionless( filename ) );
			Image *image = QERApp_LoadImage( 0, filename_noext );
			if ( !image ) {
				globalErrorStream() << "Could not load texture " << filename_noext << '\n';
			}
			else{
				qtexture_t* qtex = (qtexture_t*)malloc( sizeof( qtexture_t ) ); /* srs hack :E */
				LoadTextureRGBA( qtex, image->getRGBAPixels(), image->getWidth(), image->getHeight() );
				if( qtex->texture_number > 0 ){
					globalOutputStream() << "Loaded background texture " << filename << '\n';
					_tex = qtex->texture_number;
					_viewtype = viewtype;

					_xmin = bounds.origin[nDim1] - bounds.extents[nDim1];
					_ymin = bounds.origin[nDim2] - bounds.extents[nDim2];
					_xmax = bounds.origin[nDim1] + bounds.extents[nDim1];
					_ymax = bounds.origin[nDim2] + bounds.extents[nDim2];
				}
				image->release();
				free( qtex );
			}
		}
	}
}

void WXY_SetBackgroundImage(){
	g_pParentWnd->ActiveXY()->setBackgroundImage();
}

/*
   ============================================================================

   DRAWING

   ============================================================================
 */

/*
   ==============
   XY_DrawGrid
   ==============
 */

void XYWnd::XY_DrawAxis(){
	const char g_AxisName[3] = { 'X', 'Y', 'Z' };
	NDIM1NDIM2( m_viewType )
	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	const Colour4b colourX = ( m_viewType == YZ ) ? g_colour_y : g_colour_x;
	const Colour4b colourY = ( m_viewType == XY ) ? g_colour_y : g_colour_z;
#if 0 //gray for nonActive
	if( !Active() ){
		float grayX = vector3_dot( colourX, Vector3( 0.2989, 0.5870, 0.1140 ) );
		float grayY = vector3_dot( colourY, Vector3( 0.2989, 0.5870, 0.1140 ) );
		colourX[0] = colourX[1] = colourX[2] = grayX;
		colourY[0] = colourY[1] = colourY[2] = grayY;
	}
#endif
	// draw two lines with corresponding axis colors to highlight current view
	// horizontal line: nDim1 color
	gl().glLineWidth( 2 );
	gl().glBegin( GL_LINES );
	gl().glColor3ubv( &colourX.r );
	gl().glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	gl().glVertex2f( m_vOrigin[nDim1] - w + 65 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	gl().glVertex2f( 0, 0 );
	gl().glVertex2f( 32 / m_fScale, 0 );
	gl().glColor3ubv( &colourY.r );
	gl().glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	gl().glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale );
	gl().glVertex2f( 0, 0 );
	gl().glVertex2f( 0, 32 / m_fScale );
	gl().glEnd();
	gl().glLineWidth( 1 );
	// now print axis symbols
	const int fontHeight = GlobalOpenGL().m_font->getPixelHeight();
	const float fontWidth = fontHeight * .55f;
	gl().glColor3ubv( &colourX.r );
	gl().glRasterPos2f( m_vOrigin[nDim1] - w + ( 65 - 3 - fontWidth ) / m_fScale, m_vOrigin[nDim2] + h - ( 45 + 3 + fontHeight ) / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim1] );
	gl().glRasterPos2f( ( 32 - fontWidth / 2 ) / m_fScale, -( 0 + 3 + fontHeight ) / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim1] );
	gl().glColor3ubv( &colourY.r );
	gl().glRasterPos2f( m_vOrigin[nDim1] - w + ( 40 - 4 - fontWidth ) / m_fScale, m_vOrigin[nDim2] + h - ( 20 + 3 + fontHeight ) / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim2] );
	gl().glRasterPos2f( ( 0 - 3 - fontWidth ) / m_fScale, ( 32 - fontHeight / 2 ) / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim2] );
}

void XYWnd::XY_DrawGrid() {
	float x, y;
	char text[32];
	float step, minor_step, stepx, stepy;
	step = minor_step = stepx = stepy = GetGridSize();

	int minor_power = Grid_getPower();
	while ( ( minor_step * m_fScale ) <= 4.0f ) { // make sure minor grid spacing is at least 4 pixels on the screen
		++minor_power;
		minor_step *= 2;
	}

	int power = minor_power;
	while ( ( power % 3 ) != 0 || ( step * m_fScale ) <= 32.0f ) { // make sure major grid spacing is at least 32 pixels on the screen
		++power;
		step = pow( 2.0f, power );
	}

	const int mask = ( 1 << ( power - minor_power ) ) - 1;

	while ( ( stepx * m_fScale ) <= 32.0f ) // text step x must be at least 32
		stepx *= 2;
	while ( ( stepy * m_fScale ) <= 32.0f ) // text step y must be at least 32
		stepy *= 2;

	const float a = ( ( GetSnapGridSize() > 0.0f ) ? 1.0f : 0.3f );

	gl().glDisable( GL_TEXTURE_2D );
	gl().glDisable( GL_TEXTURE_1D );
	gl().glDisable( GL_DEPTH_TEST );
	gl().glDisable( GL_BLEND );
	gl().glLineWidth( 1 );

	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	NDIM1NDIM2( m_viewType )

	const float xb = step * floor( std::max( m_vOrigin[nDim1] - w, g_region_mins[nDim1] ) / step );
	const float xe = step * ceil( std::min( m_vOrigin[nDim1] + w, g_region_maxs[nDim1] ) / step );
	const float yb = step * floor( std::max( m_vOrigin[nDim2] - h, g_region_mins[nDim2] ) / step );
	const float ye = step * ceil( std::min( m_vOrigin[nDim2] + h, g_region_maxs[nDim2] ) / step );

	// djbob
	// draw minor blocks
	if ( g_xywindow_globals_private.d_showgrid /*|| a < 1.0f*/ ) {
		if ( a < 1.0f ) {
			gl().glEnable( GL_BLEND );
		}

		if ( g_xywindow_globals.color_gridminor != g_xywindow_globals.color_gridback ) {
			gl().glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridminor, a ) ) );

			gl().glBegin( GL_LINES );
			int i = 0;
			for ( x = xb; x < xe; x += minor_step, ++i ) {
				if ( ( i & mask ) != 0 ) {
					gl().glVertex2f( x, yb );
					gl().glVertex2f( x, ye );
				}
			}
			i = 0;
			for ( y = yb; y < ye; y += minor_step, ++i ) {
				if ( ( i & mask ) != 0 ) {
					gl().glVertex2f( xb, y );
					gl().glVertex2f( xe, y );
				}
			}
			gl().glEnd();
		}

		// draw major blocks
		if ( g_xywindow_globals.color_gridmajor != g_xywindow_globals.color_gridminor ) {
			gl().glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridmajor, a ) ) );

			gl().glBegin( GL_LINES );
			for ( x = xb; x <= xe; x += step ) {
				gl().glVertex2f( x, yb );
				gl().glVertex2f( x, ye );
			}
			for ( y = yb; y <= ye; y += step ) {
				gl().glVertex2f( xb, y );
				gl().glVertex2f( xe, y );
			}
			gl().glEnd();
		}

		if ( a < 1.0f ) {
			gl().glDisable( GL_BLEND );
		}

		if( g_region_active ){
			const float xb_ = step * floor( std::max( m_vOrigin[nDim1] - w, -GetMaxGridCoord() ) / step );
			const float xe_ = step * ceil( std::min( m_vOrigin[nDim1] + w, GetMaxGridCoord() ) / step );
			const float yb_ = step * floor( std::max( m_vOrigin[nDim2] - h, -GetMaxGridCoord() ) / step );
			const float ye_ = step * ceil( std::min( m_vOrigin[nDim2] + h, GetMaxGridCoord() ) / step );

			gl().glEnable( GL_BLEND );
			// draw minor blocks
			if ( g_xywindow_globals.color_gridminor != g_xywindow_globals.color_gridback ) {
				gl().glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridminor, .5f ) ) );

				gl().glBegin( GL_LINES );
				int i = 0;
				for ( x = xb_; x < xe_; x += minor_step, ++i ) {
					if ( ( i & mask ) != 0 ) {
						gl().glVertex2f( x, yb_ );
						gl().glVertex2f( x, ye_ );
					}
				}
				i = 0;
				for ( y = yb_; y < ye_; y += minor_step, ++i ) {
					if ( ( i & mask ) != 0 ) {
						gl().glVertex2f( xb_, y );
						gl().glVertex2f( xe_, y );
					}
				}
				gl().glEnd();
			}

			// draw major blocks
			if ( g_xywindow_globals.color_gridmajor != g_xywindow_globals.color_gridminor ) {
				gl().glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridmajor, .5f ) ) );

				gl().glBegin( GL_LINES );
				for ( x = xb_; x <= xe_; x += step ) {
					gl().glVertex2f( x, yb_ );
					gl().glVertex2f( x, ye_ );
				}
				for ( y = yb_; y <= ye_; y += step ) {
					gl().glVertex2f( xb_, y );
					gl().glVertex2f( xe_, y );
				}
				gl().glEnd();
			}
			gl().glDisable( GL_BLEND );
		}
	}

	// draw coordinate text if needed
	if ( g_xywindow_globals_private.show_coordinates ) {
		gl().glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridtext, 1.0f ) ) );
		const float offx = m_vOrigin[nDim2] + h - ( 1 + GlobalOpenGL().m_font->getPixelHeight() ) / m_fScale;
		const float offy = m_vOrigin[nDim1] - w +  4                                            / m_fScale;
		const float fontDescent = ( GlobalOpenGL().m_font->getPixelDescent() - 1 ) / m_fScale;
		for ( x = xb - fmod( xb, stepx ); x <= xe; x += stepx ) {
			gl().glRasterPos2f( x, offx );
			sprintf( text, "%g", x );
			GlobalOpenGL().drawString( text );
		}
		for ( y = yb - fmod( yb, stepy ); y <= ye; y += stepy ) {
			gl().glRasterPos2f( offy, y - fontDescent );
			sprintf( text, "%g", y );
			GlobalOpenGL().drawString( text );
		}

	}

	if ( g_xywindow_globals_private.show_axis ){
		XY_DrawAxis();
	}
	else{
		gl().glColor3fv( vector3_to_array( Active()? g_xywindow_globals.color_viewname : g_xywindow_globals.color_gridtext ) );
		gl().glRasterPos2f( m_vOrigin[nDim1] - w + 35 / m_fScale, m_vOrigin[nDim2] + h - ( GlobalOpenGL().m_font->getPixelHeight() * 2 ) / m_fScale );
		GlobalOpenGL().drawString( ViewType_getTitle( m_viewType ) );
	}

	// show current work zone?
	// the work zone is used to place dropped points and brushes
	if ( g_xywindow_globals_private.show_workzone ) {
		gl().glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );
		gl().glBegin( GL_LINES );
		gl().glVertex2f( xb, Select_getWorkZone().d_work_min[nDim2] );
		gl().glVertex2f( xe, Select_getWorkZone().d_work_min[nDim2] );
		gl().glVertex2f( xb, Select_getWorkZone().d_work_max[nDim2] );
		gl().glVertex2f( xe, Select_getWorkZone().d_work_max[nDim2] );
		gl().glVertex2f( Select_getWorkZone().d_work_min[nDim1], yb );
		gl().glVertex2f( Select_getWorkZone().d_work_min[nDim1], ye );
		gl().glVertex2f( Select_getWorkZone().d_work_max[nDim1], yb );
		gl().glVertex2f( Select_getWorkZone().d_work_max[nDim1], ye );
		gl().glEnd();
	}
}

/*
   ==============
   XY_DrawBlockGrid
   ==============
 */
void XYWnd::XY_DrawBlockGrid(){
	int bs[3] = { 1024, 1024, 1024 }; // compiler's default

	if ( Map_FindWorldspawn( g_map ) == 0 ) {
		return;
	}
	const char *value = Node_getEntity( *Map_GetWorldspawn( g_map ) )->getKeyValue( "_blocksize" );
	if ( !string_empty( value ) ) {
		const int scanned = sscanf( value, "%i %i %i", bs, bs + 1, bs + 2 );
		if( scanned == 1 || scanned == 2 ) /* handle legacy case */
			bs[1] = bs[2] = bs[0];
	}

	NDIM1NDIM2( m_viewType )

	int bs1 = bs[nDim1];
	int bs2 = bs[nDim2];

	if( bs1 <= 0 && bs2 <= 0 ) // zero disables
		return;

	gl().glDisable( GL_TEXTURE_2D );
	gl().glDisable( GL_TEXTURE_1D );
	gl().glDisable( GL_DEPTH_TEST );
	gl().glDisable( GL_BLEND );

	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	float xb = std::max( m_vOrigin[nDim1] - w, g_region_mins[nDim1] );
	float xe = std::min( m_vOrigin[nDim1] + w, g_region_maxs[nDim1] );
	float yb = std::max( m_vOrigin[nDim2] - h, g_region_mins[nDim2] );
	float ye = std::min( m_vOrigin[nDim2] + h, g_region_maxs[nDim2] );

	if( bs1 > 0 ){
		bs1 = std::clamp( bs1, 256, 65536 );
		xb = bs1 * floor( xb / bs1 );
		xe = bs1 * ceil( xe / bs1 );
	}
	if( bs2 > 0 ){
		bs2 = std::clamp( bs2, 256, 65536 );
		yb = bs2 * floor( yb / bs2 );
		ye = bs2 * ceil( ye / bs2 );
	}

	// draw major blocks

	gl().glColor3fv( vector3_to_array( g_xywindow_globals.color_gridblock ) );
	gl().glLineWidth( 2 );

	gl().glBegin( GL_LINES );

	if( bs1 > 0 ) {
		for ( float x = xb; x <= xe; x += bs1 )
		{
			gl().glVertex2f( x, yb );
			gl().glVertex2f( x, ye );
		}
	}

	if ( bs2 > 0 ) {
		for ( float y = yb; y <= ye; y += bs2 )
		{
			gl().glVertex2f( xb, y );
			gl().glVertex2f( xe, y );
		}
	}

	gl().glEnd();
	gl().glLineWidth( 1 );

#if 0
	// draw coordinate text if needed
	char text[32];
	if ( m_viewType == XY && m_fScale > .1 ) {
		for ( float x = xb; x < xe; x += bs1 )
			for ( float y = yb; y < ye; y += bs2 )
			{
				gl().glRasterPos2f( x + ( bs1 / 2 ), y + ( bs2 / 2 ) );
				sprintf( text, "%i,%i", (int)floor( x / bs1 ), (int)floor( y / bs2 ) );
				GlobalOpenGL().drawString( text );
			}
	}
#endif
	gl().glColor4f( 0, 0, 0, 0 );
}

void XYWnd::DrawCameraIcon( const Vector3& origin, const Vector3& angles ){
//	globalOutputStream() << "pitch " << angles[CAMERA_PITCH] << "   yaw " << angles[CAMERA_YAW] << '\n';
	const float fov = 48 / m_fScale;
	const float box = 16 / m_fScale;

	NDIM1NDIM2( m_viewType )
	const float x = origin[nDim1];
	const float y = origin[nDim2];
	const double a = ( m_viewType == XY )?
	                 degrees_to_radians( angles[CAMERA_YAW] )
	                 : ( m_viewType == YZ )?
	                 degrees_to_radians( ( angles[CAMERA_YAW] > 180 ) ? ( 180.0f - angles[CAMERA_PITCH] ) : angles[CAMERA_PITCH] )
	                 : degrees_to_radians( ( angles[CAMERA_YAW] < 270 && angles[CAMERA_YAW] > 90 ) ? ( 180.0f - angles[CAMERA_PITCH] ) : angles[CAMERA_PITCH] );

	gl().glColor3fv( vector3_to_array( g_xywindow_globals.color_camera ) );
	gl().glBegin( GL_LINE_STRIP );
	gl().glVertex3f( x - box, y              , 0 );
	gl().glVertex3f( x      , y + ( box / 2 ), 0 );
	gl().glVertex3f( x + box, y              , 0 );
	gl().glVertex3f( x      , y - ( box / 2 ), 0 );
	gl().glVertex3f( x - box, y              , 0 );
	gl().glVertex3f( x + box, y              , 0 );
	gl().glEnd();

	gl().glBegin( GL_LINE_STRIP );
	gl().glVertex3f( x + static_cast<float>( fov * cos( a + c_pi / 4 ) ), y + static_cast<float>( fov * sin( a + c_pi / 4 ) ), 0 );
	gl().glVertex3f( x, y, 0 );
	gl().glVertex3f( x + static_cast<float>( fov * cos( a - c_pi / 4 ) ), y + static_cast<float>( fov * sin( a - c_pi / 4 ) ), 0 );
	gl().glEnd();
}


void XYWnd::PaintSizeInfo( const int nDim1, const int nDim2 ){
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	if ( bounds.extents == g_vector3_identity ) {
		return;
	}

	const Vector3 min = bounds.origin - bounds.extents;
	const Vector3 max = bounds.origin + bounds.extents;
	const Vector3 mid = bounds.origin;
	const Vector3 size = bounds.extents * 2;

	const char* dimStrings[] = {"x:", "y:", "z:"};

	gl().glColor3fv( vector3_to_array( g_xywindow_globals.color_selbrushes * .65f ) );

	StringOutputStream dimensions( 16 );

	Vector3 v( g_vector3_identity );

	gl().glBegin( GL_LINE_STRIP );
	v[nDim1] = min[nDim1];
	v[nDim2] = min[nDim2] - 6.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2] - 10.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1];
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2] - 6.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	gl().glEnd();

	gl().glBegin( GL_LINE_STRIP );
	v[nDim2] = max[nDim2];
	v[nDim1] = max[nDim1] + 6.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1] + 10.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2];
	gl().glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1] + 6.f / m_fScale;
	gl().glVertex3fv( vector3_to_array( v ) );
	gl().glEnd();

	const int fontHeight = GlobalOpenGL().m_font->getPixelHeight();

	v[nDim1] = mid[nDim1];
	v[nDim2] = min[nDim2] - ( 10 + 2 + fontHeight ) / m_fScale;
	gl().glRasterPos3fv( vector3_to_array( v ) );
	GlobalOpenGL().drawString( dimensions( dimStrings[nDim1], size[nDim1] ) );

	v[nDim1] = max[nDim1] + 16.f / m_fScale;
	v[nDim2] = mid[nDim2] - fontHeight / m_fScale / 2;
	gl().glRasterPos3fv( vector3_to_array( v ) );
	GlobalOpenGL().drawString( dimensions( dimStrings[nDim2], size[nDim2] ) );

	v[nDim1] = min[nDim1] + 4.f / m_fScale;
	v[nDim2] = max[nDim2] + 5.f / m_fScale;
	gl().glRasterPos3fv( vector3_to_array( v ) );
	GlobalOpenGL().drawString( dimensions( '(', dimStrings[nDim1], min[nDim1], "  ", dimStrings[nDim2], max[nDim2], ')' ) );
}

class XYRenderer : public Renderer
{
	struct state_type
	{
		state_type() :
			m_highlight( 0 ),
			m_state( 0 ){
		}
		unsigned int m_highlight;
		Shader* m_state;
	};
public:
	XYRenderer( RenderStateFlags globalstate, Shader* selected ) :
		m_globalstate( globalstate ),
		m_state_selected( selected ){
		ASSERT_NOTNULL( selected );
		m_state_stack.push_back( state_type() );
	}

	void SetState( Shader* state, EStyle style ){
		ASSERT_NOTNULL( state );
		if ( style == eWireframeOnly ) {
			m_state_stack.back().m_state = state;
		}
	}
	EStyle getStyle() const {
		return eWireframeOnly;
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
	void addRenderable( const OpenGLRenderable& renderable, const Matrix4& localToWorld ){
		if ( m_state_stack.back().m_highlight & ePrimitive ) {
			m_state_selected->addRenderable( renderable, localToWorld );
		}
		else
		{
			m_state_stack.back().m_state->addRenderable( renderable, localToWorld );
		}
	}

	void render( const Matrix4& modelview, const Matrix4& projection ){
		GlobalShaderCache().render( m_globalstate, modelview, projection );
	}
private:
	std::vector<state_type> m_state_stack;
	RenderStateFlags m_globalstate;
	Shader* m_state_selected;
};

void XYWnd::updateProjection(){
	m_projection[0] = 1.0f / static_cast<float>( m_nWidth / 2 );
	m_projection[5] = 1.0f / static_cast<float>( m_nHeight / 2 );
	m_projection[10] = 1.0f / ( g_MaxWorldCoord * m_fScale );

	m_projection[12] = 0.0f;
	m_projection[13] = 0.0f;
	m_projection[14] = -1.0f;

	m_projection[1] = m_projection[2] = m_projection[3] =
	m_projection[4] = m_projection[6] = m_projection[7] =
	m_projection[8] = m_projection[9] = m_projection[11] = 0.0f;

	m_projection[15] = 1.0f;

	m_view.Construct( m_projection, m_modelview, m_nWidth, m_nHeight );
}

// note: modelview matrix must have a uniform scale, otherwise strange things happen when rendering the rotation manipulator.
void XYWnd::updateModelview(){
	NDIM1NDIM2( m_viewType )

	// translation
	m_modelview[12] = -m_vOrigin[nDim1] * m_fScale;
	m_modelview[13] = -m_vOrigin[nDim2] * m_fScale;
	m_modelview[14] = g_MaxWorldCoord * m_fScale;

	// axis base
	switch ( m_viewType )
	{
	case XY:
		m_modelview[0]  =  m_fScale;
		m_modelview[1]  =  0;
		m_modelview[2]  =  0;

		m_modelview[4]  =  0;
		m_modelview[5]  =  m_fScale;
		m_modelview[6]  =  0;

		m_modelview[8]  =  0;
		m_modelview[9]  =  0;
		m_modelview[10] = -m_fScale;
		break;
	case XZ:
		m_modelview[0]  =  m_fScale;
		m_modelview[1]  =  0;
		m_modelview[2]  =  0;

		m_modelview[4]  =  0;
		m_modelview[5]  =  0;
		m_modelview[6]  =  m_fScale;

		m_modelview[8]  =  0;
		m_modelview[9]  =  m_fScale;
		m_modelview[10] =  0;
		break;
	case YZ:
		m_modelview[0]  =  0;
		m_modelview[1]  =  0;
		m_modelview[2]  = -m_fScale;

		m_modelview[4]  =  m_fScale;
		m_modelview[5]  =  0;
		m_modelview[6]  =  0;

		m_modelview[8]  =  0;
		m_modelview[9]  =  m_fScale;
		m_modelview[10] =  0;
		break;
	}

	m_modelview[3] = m_modelview[7] = m_modelview[11] = 0;
	m_modelview[15] = 1;

	m_view.Construct( m_projection, m_modelview, m_nWidth, m_nHeight );
}

/*
   ==============
   XY_Draw
   ==============
 */

//#define DBG_SCENEDUMP

void XYWnd::XY_Draw(){
//		globalOutputStream() << "XY_Draw()\n";
	/* workaround poorly visible white lights on bright background */
	GlobalEntityCreator().setLightColorize( vector3_min_component( g_xywindow_globals.color_gridback ) < .9f );
	//
	// clear
	//
	gl().glViewport( 0, 0, m_nWidth, m_nHeight );
	gl().glClearColor( g_xywindow_globals.color_gridback[0],
	                   g_xywindow_globals.color_gridback[1],
	                   g_xywindow_globals.color_gridback[2], 0 );
	gl().glClear( GL_COLOR_BUFFER_BIT );

	extern void Renderer_ResetStats();
	Renderer_ResetStats();

	//
	// set up viewpoint
	//

	gl().glMatrixMode( GL_PROJECTION );
	gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

	gl().glMatrixMode( GL_MODELVIEW );
	gl().glLoadIdentity();
	gl().glScalef( m_fScale, m_fScale, 1 );
	NDIM1NDIM2( m_viewType )
	gl().glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );

	gl().glDisable( GL_LINE_STIPPLE );
	gl().glLineWidth( 1 );
	gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	gl().glDisableClientState( GL_NORMAL_ARRAY );
	gl().glDisableClientState( GL_COLOR_ARRAY );
	gl().glDisable( GL_TEXTURE_2D );
	gl().glDisable( GL_LIGHTING );
	gl().glDisable( GL_COLOR_MATERIAL );
	gl().glDisable( GL_DEPTH_TEST );

	m_backgroundImage.render( m_viewType );

	XY_DrawGrid();

	if ( g_xywindow_globals_private.show_blocks ) {
		XY_DrawBlockGrid();
	}

	gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

	unsigned int globalstate = RENDER_COLOURARRAY | RENDER_COLOURWRITE;
	if ( !g_xywindow_globals.m_bNoStipple ) {
		globalstate |= RENDER_LINESTIPPLE;
	}

	{
		XYRenderer renderer( globalstate, m_state_selected );

		Scene_Render( renderer, m_view );

		GlobalOpenGL_debugAssertNoErrors();
		renderer.render( m_modelview, m_projection );
		GlobalOpenGL_debugAssertNoErrors();
	}

	gl().glDepthMask( GL_FALSE );

	GlobalOpenGL_debugAssertNoErrors();

	gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisable( GL_LINE_STIPPLE );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glLineWidth( 1 );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glActiveTexture( GL_TEXTURE0 );
	gl().glClientActiveTexture( GL_TEXTURE0 );
	gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisableClientState( GL_NORMAL_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisableClientState( GL_COLOR_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisable( GL_TEXTURE_2D );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisable( GL_LIGHTING );
	GlobalOpenGL_debugAssertNoErrors();
	gl().glDisable( GL_COLOR_MATERIAL );
	GlobalOpenGL_debugAssertNoErrors();

	GlobalOpenGL_debugAssertNoErrors();


	// size info
	if ( g_xywindow_globals_private.m_bShowSize && GlobalSelectionSystem().countSelected() != 0 ) {
		PaintSizeInfo( nDim1, nDim2 );
	}

	GlobalOpenGL_debugAssertNoErrors();

	{
		// reset modelview
		gl().glLoadIdentity();
		gl().glScalef( m_fScale, m_fScale, 1 );
		gl().glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );

		Feedback_draw2D( m_viewType );
	}

	if( g_camwindow_globals.m_showStats ){
		gl().glMatrixMode( GL_PROJECTION );
		gl().glLoadIdentity();
		gl().glOrtho( 0, m_nWidth, 0, m_nHeight, 0, 1 );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glLoadIdentity();

		gl().glColor3fv( vector3_to_array( g_xywindow_globals.color_viewname ) );

		gl().glRasterPos3f( 2.f, 0.f, 0.0f );
		extern const char* Renderer_GetStats( int frame2frame );
		GlobalOpenGL().drawString( Renderer_GetStats( m_render_time.elapsed_msec() ) );
		m_render_time.start();
	}
}

void XYWnd_MouseToPoint( XYWnd* xywnd, int x, int y, Vector3& point ){
	point = xywnd->XY_ToPoint( x, y, true );
	const int nDim = xywnd->GetViewType();
	const float fWorkMid = float_mid( Select_getWorkZone().d_work_min[nDim], Select_getWorkZone().d_work_max[nDim] );
	point[nDim] = float_snapped( fWorkMid, GetGridSize() );
}

void XYWnd::OnEntityCreate( const char* item ){
	Vector3 point;
	XYWnd_MouseToPoint( this, m_entityCreate_x, m_entityCreate_y, point );

	const float offset = std::max( 8.f, GetSnapGridSize() ) * g_entityCreationOffset;
	NDIM1NDIM2( m_viewType )
	point += g_vector3_axes[nDim1] * offset;
	(void)nDim2;

	Entity_createFromSelection( item, point );
}



inline AABB GetCenterBbox(){
	return ( GlobalSelectionSystem().countSelected() != 0 )?
	         GlobalSelectionSystem().getBoundsSelected() :
	         AABB( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ), Vector3( 128.f, 128.f, 128.f ) );
}

void XYWnd_Centralize( XYWnd* xywnd ){
	xywnd->SetOrigin( GetCenterBbox().origin );
}

void XY_Centralize(){
	const Vector3 position( GetCenterBbox().origin );
	g_pParentWnd->forEachXYWnd( [&position]( XYWnd* xywnd ){
		xywnd->SetOrigin( position );
	} );
}

void XY_Focus(){
	const AABB bounds( GetCenterBbox() );
	g_pParentWnd->forEachXYWnd( [&bounds]( XYWnd* xywnd ){
		xywnd->FocusOnBounds( bounds );
	} );
}



void XY_SetViewType( VIEWTYPE viewtype ){
	if ( g_pParentWnd->CurrentStyle() != MainFrame::eSplit ) { // do not want this in a split window
		XYWnd* xywnd = g_pParentWnd->ActiveXY();
		xywnd->SetViewType( viewtype );
		XYWnd_Centralize( xywnd );
	}
	else{
		XY_Centralize(); // do something else that the user may want here
	}
}

void XY_Top(){
	XY_SetViewType( XY );
}

void XY_Front(){
	XY_SetViewType( XZ );
}

void XY_Side(){
	XY_SetViewType( YZ );
}

void XY_NextView(){
	XY_SetViewType( static_cast<VIEWTYPE>( ( g_pParentWnd->ActiveXY()->GetViewType() + 2 ) % 3 ) );
}

void XY_Zoom100(){
	g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
		xywnd->SetScale( 1 );
	} );
}

void XY_ZoomIn(){
	g_pParentWnd->ActiveXY()->ZoomIn();
}

void XY_ZoomOut(){
	g_pParentWnd->ActiveXY()->ZoomOut();
}



ToggleShown g_xy_top_shown( true );

void XY_Top_Shown_Construct( QWidget* parent ){
	g_xy_top_shown.connect( parent );
}

ToggleShown g_yz_side_shown( false );

void YZ_Side_Shown_Construct( QWidget* parent ){
	g_yz_side_shown.connect( parent );
}

ToggleShown g_xz_front_shown( false );

void XZ_Front_Shown_Construct( QWidget* parent ){
	g_xz_front_shown.connect( parent );
}


class EntityClassMenu : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	EntityClassMenu() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			if ( XYWnd::m_mnuDrop != 0 ) {
				delete XYWnd::m_mnuDrop;
				XYWnd::m_mnuDrop = 0;
			}
		}
	}
};

EntityClassMenu g_EntityClassMenu;




void ShowNamesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowNames() );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowNamesExport> ShowNamesExportCaller;
ShowNamesExportCaller g_show_names_caller;
ToggleItem g_show_names( g_show_names_caller );
void ShowNamesToggle(){
	GlobalEntityCreator().setShowNames( !GlobalEntityCreator().getShowNames() );
	g_show_names.update();
	UpdateAllWindows();
}

void ShowBboxesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowBboxes() );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowBboxesExport> ShowBboxesExportCaller;
ShowBboxesExportCaller g_show_bboxes_caller;
ToggleItem g_show_bboxes( g_show_bboxes_caller );
void ShowBboxesToggle(){
	GlobalEntityCreator().setShowBboxes( !GlobalEntityCreator().getShowBboxes() );
	g_show_bboxes.update();
	UpdateAllWindows();
}

void ShowConnectionsExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowConnections() );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowConnectionsExport> ShowConnectionsExportCaller;
ShowConnectionsExportCaller g_show_connections_caller;
ToggleItem g_show_connections( g_show_connections_caller );
void ShowConnectionsToggle(){
	GlobalEntityCreator().setShowConnections( !GlobalEntityCreator().getShowConnections() );
	g_show_connections.update();
	UpdateAllWindows();
}

void ShowAnglesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowAngles() );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowAnglesExport> ShowAnglesExportCaller;
ShowAnglesExportCaller g_show_angles_caller;
ToggleItem g_show_angles( g_show_angles_caller );
void ShowAnglesToggle(){
	GlobalEntityCreator().setShowAngles( !GlobalEntityCreator().getShowAngles() );
	g_show_angles.update();
	UpdateAllWindows();
}

ToggleItem g_show_blocks( BoolExportCaller( g_xywindow_globals_private.show_blocks ) );
void ShowBlocksToggle(){
	g_xywindow_globals_private.show_blocks ^= 1;
	g_show_blocks.update();
	XY_UpdateAllWindows();
}

ToggleItem g_show_coordinates( BoolExportCaller( g_xywindow_globals_private.show_coordinates ) );
void ShowCoordinatesToggle(){
	g_xywindow_globals_private.show_coordinates ^= 1;
	g_show_coordinates.update();
	XY_UpdateAllWindows();
}

ToggleItem g_show_outline( BoolExportCaller( g_xywindow_globals_private.show_outline ) );
void ShowOutlineToggle(){
	g_xywindow_globals_private.show_outline ^= 1;
	g_show_outline.update();
	XY_UpdateAllWindows();
}

ToggleItem g_show_axes( BoolExportCaller( g_xywindow_globals_private.show_axis ) );
void ShowAxesToggle(){
	g_xywindow_globals_private.show_axis ^= 1;
	g_show_axes.update();
	XY_UpdateAllWindows();
}


ToggleItem g_show_workzone( BoolExportCaller( g_xywindow_globals_private.show_workzone ) );
void ShowWorkzoneToggle(){
	g_xywindow_globals_private.show_workzone ^= 1;
	g_show_workzone.update();
	XY_UpdateAllWindows();
}

/*
void ShowAxesToggle(){
	g_xywindow_globals_private.show_axis ^= 1;
	XY_UpdateAllWindows();
}
typedef FreeCaller<void(), ShowAxesToggle> ShowAxesToggleCaller;
void ShowAxesExport( const BoolImportCallback& importer ){
	importer( g_xywindow_globals_private.show_axis );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowAxesExport> ShowAxesExportCaller;

ShowAxesExportCaller g_show_axes_caller;
BoolExportCallback g_show_axes_callback( g_show_axes_caller );
ToggleItem g_show_axes( g_show_axes_callback );
*/

/*
BoolExportCaller g_texdef_movelock_caller( g_brush_texturelock_enabled );
ToggleItem g_texdef_movelock_item( g_texdef_movelock_caller );

void Texdef_ToggleMoveLock(){
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
}
*/

ToggleItem g_show_size_item( BoolExportCaller( g_xywindow_globals_private.m_bShowSize ) );
void ToggleShowSizeInfo(){
	g_xywindow_globals_private.m_bShowSize = !g_xywindow_globals_private.m_bShowSize;
	g_show_size_item.update();
	XY_UpdateAllWindows();
}

ToggleItem g_show_crosshair_item{ BoolExportCaller( g_bCrossHairs ) };
void ToggleShowCrosshair(){
	g_bCrossHairs ^= 1;
	g_show_crosshair_item.update();
	XY_UpdateAllWindows();
}

ToggleItem g_show_grid_item( BoolExportCaller( g_xywindow_globals_private.d_showgrid ) );
void ToggleShowGrid(){
	g_xywindow_globals_private.d_showgrid = !g_xywindow_globals_private.d_showgrid;
	g_show_grid_item.update();
	XY_UpdateAllWindows();
}

void MSAAImport( int value ){
	g_xywindow_globals_private.m_MSAA = value ? 1 << value : value;
}
typedef FreeCaller<void(int), MSAAImport> MSAAImportCaller;

void MSAAExport( const IntImportCallback& importer ){
	if( g_xywindow_globals_private.m_MSAA <= 0 ){
		importer( 0 );
	}
	else{
		int exponent = 1;
		while( !( ( g_xywindow_globals_private.m_MSAA >> exponent ) & 1 ) ){
			++exponent;
		}
		importer( exponent );
	}
}
typedef FreeCaller<void(const IntImportCallback&), MSAAExport> MSAAExportCaller;


inline void Colour4b_importString( Colour4b& self, const char* string ){
	if ( 4 != sscanf( string, "%hhu %hhu %hhu %hhu", &self.r, &self.g, &self.b, &self.a ) ) {
		self = Colour4b( 0, 0, 0, 255 );
	}
}
typedef ReferenceCaller<Colour4b, void(const char*), Colour4b_importString> Colour4bImportStringCaller;
inline void Colour4b_exportString( const Colour4b& self, const StringImportCallback& importer ){
	char buffer[64];
	sprintf( buffer, "%hhu %hhu %hhu %hhu", self.r, self.g, self.b, self.a );
	importer( buffer );
}
typedef ConstReferenceCaller<Colour4b, void(const StringImportCallback&), Colour4b_exportString> Colour4bExportStringCaller;


void XYShow_registerCommands(){
	GlobalToggles_insert( "ShowSize2d", makeCallbackF( ToggleShowSizeInfo ), ToggleItem::AddCallbackCaller( g_show_size_item ), QKeySequence( "J" ) );
	GlobalToggles_insert( "ToggleCrosshairs", makeCallbackF( ToggleShowCrosshair ), ToggleItem::AddCallbackCaller( g_show_crosshair_item ), QKeySequence( "Shift+X" ) );
	GlobalToggles_insert( "ToggleGrid", makeCallbackF( ToggleShowGrid ), ToggleItem::AddCallbackCaller( g_show_grid_item ), QKeySequence( "0" ) );

	GlobalToggles_insert( "ShowAngles", makeCallbackF( ShowAnglesToggle ), ToggleItem::AddCallbackCaller( g_show_angles ) );
	GlobalToggles_insert( "ShowNames", makeCallbackF( ShowNamesToggle ), ToggleItem::AddCallbackCaller( g_show_names ) );
	GlobalToggles_insert( "ShowBboxes", makeCallbackF( ShowBboxesToggle ), ToggleItem::AddCallbackCaller( g_show_bboxes ) );
	GlobalToggles_insert( "ShowConnections", makeCallbackF( ShowConnectionsToggle ), ToggleItem::AddCallbackCaller( g_show_connections ) );
	GlobalToggles_insert( "ShowBlocks", makeCallbackF( ShowBlocksToggle ), ToggleItem::AddCallbackCaller( g_show_blocks ) );
	GlobalToggles_insert( "ShowCoordinates", makeCallbackF( ShowCoordinatesToggle ), ToggleItem::AddCallbackCaller( g_show_coordinates ) );
	GlobalToggles_insert( "ShowWindowOutline", makeCallbackF( ShowOutlineToggle ), ToggleItem::AddCallbackCaller( g_show_outline ) );
	GlobalToggles_insert( "ShowAxes", makeCallbackF( ShowAxesToggle ), ToggleItem::AddCallbackCaller( g_show_axes ) );
	GlobalToggles_insert( "ShowWorkzone2d", makeCallbackF( ShowWorkzoneToggle ), ToggleItem::AddCallbackCaller( g_show_workzone ) );
}

void XYWnd_registerShortcuts(){
	command_connect_accelerator( "ToggleCrosshairs" );
	command_connect_accelerator( "ShowSize2d" );
}



void Orthographic_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Solid selection boxes ( no stipple )", g_xywindow_globals.m_bNoStipple );
	//page.appendCheckBox( "", "Display size info", g_xywindow_globals_private.m_bShowSize );
	page.appendCheckBox( "", "Chase mouse during drags", g_xywindow_globals_private.m_bChaseMouse );
	page.appendCheckBox( "", "Zoom to Mouse pointer", g_xywindow_globals_private.m_bZoomToPointer );

	{
		const char* samples[] = { "0", "2", "4", "8", "16", "32" };

		page.appendCombo(
		    "MSAA",
		    StringArrayRange( samples ),
		    IntImportCallback( MSAAImportCaller() ),
		    IntExportCallback( MSAAExportCaller() )
		);
	}
}
void Orthographic_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Orthographic", "Orthographic View Preferences" ) );
	Orthographic_constructPreferences( page );
}
void Orthographic_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( Orthographic_constructPage ) );
}


#include "preferencesystem.h"
#include "stringio.h"


void XYWindow_Construct(){
	GlobalToggles_insert( "ToggleView", ToggleShown::ToggleCaller( g_xy_top_shown ), ToggleItem::AddCallbackCaller( g_xy_top_shown.m_item ) );
	GlobalToggles_insert( "ToggleSideView", ToggleShown::ToggleCaller( g_yz_side_shown ), ToggleItem::AddCallbackCaller( g_yz_side_shown.m_item ) );
	GlobalToggles_insert( "ToggleFrontView", ToggleShown::ToggleCaller( g_xz_front_shown ), ToggleItem::AddCallbackCaller( g_xz_front_shown.m_item ) );
	GlobalCommands_insert( "NextView", makeCallbackF( XY_NextView ), QKeySequence( "Ctrl+Tab" ) );
	GlobalCommands_insert( "ZoomIn", makeCallbackF( XY_ZoomIn ), QKeySequence( "Delete" ) );
	GlobalCommands_insert( "ZoomOut", makeCallbackF( XY_ZoomOut ), QKeySequence( "Insert" ) );
	GlobalCommands_insert( "ViewTop", makeCallbackF( XY_Top ), QKeySequence( Qt::Key_7 + Qt::KeypadModifier ) );
	GlobalCommands_insert( "ViewFront", makeCallbackF( XY_Front ), QKeySequence( Qt::Key_1 + Qt::KeypadModifier ) );
	GlobalCommands_insert( "ViewSide", makeCallbackF( XY_Side ), QKeySequence( Qt::Key_3 + Qt::KeypadModifier ) );
	GlobalCommands_insert( "Zoom100", makeCallbackF( XY_Zoom100 ) );
	GlobalCommands_insert( "CenterXYView", makeCallbackF( XY_Centralize ), QKeySequence( "Ctrl+Shift+Tab" ) );
	GlobalCommands_insert( "XYFocusOnSelected", makeCallbackF( XY_Focus ), QKeySequence( "`" ) );

	GlobalPreferenceSystem().registerPreference( "XYMSAA", IntImportStringCaller( g_xywindow_globals_private.m_MSAA ), IntExportStringCaller( g_xywindow_globals_private.m_MSAA ) );
	GlobalPreferenceSystem().registerPreference( "2DZoomInToPointer", BoolImportStringCaller( g_xywindow_globals_private.m_bZoomToPointer ), BoolExportStringCaller( g_xywindow_globals_private.m_bZoomToPointer ) );
	GlobalPreferenceSystem().registerPreference( "ChaseMouse", BoolImportStringCaller( g_xywindow_globals_private.m_bChaseMouse ), BoolExportStringCaller( g_xywindow_globals_private.m_bChaseMouse ) );
	GlobalPreferenceSystem().registerPreference( "ShowSize2d", BoolImportStringCaller( g_xywindow_globals_private.m_bShowSize ), BoolExportStringCaller( g_xywindow_globals_private.m_bShowSize ) );
	GlobalPreferenceSystem().registerPreference( "ShowCrosshair", BoolImportStringCaller( g_bCrossHairs ), BoolExportStringCaller( g_bCrossHairs ) );
	GlobalPreferenceSystem().registerPreference( "NoStipple", BoolImportStringCaller( g_xywindow_globals.m_bNoStipple ), BoolExportStringCaller( g_xywindow_globals.m_bNoStipple ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowCoords", BoolImportStringCaller( g_xywindow_globals_private.show_coordinates ), BoolExportStringCaller( g_xywindow_globals_private.show_coordinates ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowOutlines", BoolImportStringCaller( g_xywindow_globals_private.show_outline ), BoolExportStringCaller( g_xywindow_globals_private.show_outline ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowAxis", BoolImportStringCaller( g_xywindow_globals_private.show_axis ), BoolExportStringCaller( g_xywindow_globals_private.show_axis ) );
	GlobalPreferenceSystem().registerPreference( "ShowWorkzone2d", BoolImportStringCaller( g_xywindow_globals_private.show_workzone ), BoolExportStringCaller( g_xywindow_globals_private.show_workzone ) );

	GlobalPreferenceSystem().registerPreference( "ColorAxisX", Colour4bImportStringCaller( g_colour_x ), Colour4bExportStringCaller( g_colour_x ) );
	GlobalPreferenceSystem().registerPreference( "ColorAxisY", Colour4bImportStringCaller( g_colour_y ), Colour4bExportStringCaller( g_colour_y ) );
	GlobalPreferenceSystem().registerPreference( "ColorAxisZ", Colour4bImportStringCaller( g_colour_z ), Colour4bExportStringCaller( g_colour_z ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridBackground", Vector3ImportStringCaller( g_xywindow_globals.color_gridback ), Vector3ExportStringCaller( g_xywindow_globals.color_gridback ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridMinor", Vector3ImportStringCaller( g_xywindow_globals.color_gridminor ), Vector3ExportStringCaller( g_xywindow_globals.color_gridminor ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridMajor", Vector3ImportStringCaller( g_xywindow_globals.color_gridmajor ), Vector3ExportStringCaller( g_xywindow_globals.color_gridmajor ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridBlocks", Vector3ImportStringCaller( g_xywindow_globals.color_gridblock ), Vector3ExportStringCaller( g_xywindow_globals.color_gridblock ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridText", Vector3ImportStringCaller( g_xywindow_globals.color_gridtext ), Vector3ExportStringCaller( g_xywindow_globals.color_gridtext ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridWorldspawn", Vector3ImportStringCaller( g_xywindow_globals.color_brushes ), Vector3ExportStringCaller( g_xywindow_globals.color_brushes ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridActive", Vector3ImportStringCaller( g_xywindow_globals.color_viewname ), Vector3ExportStringCaller( g_xywindow_globals.color_viewname ) );
	GlobalPreferenceSystem().registerPreference( "ColorClipperSplit", Vector3ImportStringCaller( g_xywindow_globals.color_clipper ), Vector3ExportStringCaller( g_xywindow_globals.color_clipper ) );
	GlobalPreferenceSystem().registerPreference( "ColorGridSelection", Vector3ImportStringCaller( g_xywindow_globals.color_selbrushes ), Vector3ExportStringCaller( g_xywindow_globals.color_selbrushes ) );
	GlobalPreferenceSystem().registerPreference( "ColorCameraIcon", Vector3ImportStringCaller( g_xywindow_globals.color_camera ), Vector3ExportStringCaller( g_xywindow_globals.color_camera ) );


	GlobalPreferenceSystem().registerPreference( "XYVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_xy_top_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_xy_top_shown ) ) );
	GlobalPreferenceSystem().registerPreference( "XZVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_xz_front_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_xz_front_shown ) ) );
	GlobalPreferenceSystem().registerPreference( "YZVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_yz_side_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_yz_side_shown ) ) );

	Orthographic_registerPreferencesPage();

	XYWnd::captureStates();
	GlobalEntityClassManager().attach( g_EntityClassMenu );
}

void XYWindow_Destroy(){
	GlobalEntityClassManager().detach( g_EntityClassMenu );
	XYWnd::releaseStates();
}
