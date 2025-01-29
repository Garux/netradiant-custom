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

#pragma once

#include "math/matrix.h"
#include "signal/signal.h"

#include "gtkutil/cursor.h"
#include "gtkutil/xorrectangle.h"
#include "view.h"
#include "map.h"

#include "qerplugin.h"

extern bool g_bCamEntityMenu;

class Shader;
class SelectionSystemWindowObserver;
namespace scene
{
class Node;
}

inline const char* ViewType_getTitle( VIEWTYPE viewtype ){
	if ( viewtype == XY ) {
		return "XY Top";
	}
	if ( viewtype == XZ ) {
		return "XZ Front";
	}
	if ( viewtype == YZ ) {
		return "YZ Side";
	}
	return "";
}

class BackgroundImage
{
	GLuint _tex;
	const float _alpha;
	float _xmin, _ymin, _xmax, _ymax;
	VIEWTYPE _viewtype;
public:
	BackgroundImage() : _tex( 0 ), _alpha( 1 ){
	}
	~BackgroundImage(){
		free_tex();
	}
	void set( const VIEWTYPE viewtype );
	void render( const VIEWTYPE viewtype );
private:
	const char* background_image_dialog();
	void free_tex();
};

#include "timer.h"
class QWidget;

class XYWnd
{
	QWidget* m_gl_widget;

	DeferredDraw m_deferredDraw;

public:

	QWidget* m_parent;
	XYWnd();
	~XYWnd();

	void queueDraw(){
		m_deferredDraw.draw();
	}
	QWidget* GetWidget(){
		return m_gl_widget;
	}

	SelectionSystemWindowObserver* m_window_observer;
	XORRectangle m_XORRectangle;
	rect_t m_XORRect;

	static void captureStates();
	static void releaseStates();
	static void recaptureStates(){
		releaseStates();
		captureStates();
	}

	const Vector3& GetOrigin() const;
	void SetOrigin( const Vector3& origin );
	void Scroll( int x, int y );

	bool m_drawRequired{}; // whether complete redraw is required, or just overlay update is enough
	void XY_Draw();
	void overlayDraw();
	void DrawCameraIcon( const Vector3& origin, const Vector3& angles );
	void XY_DrawBlockGrid();
	void XY_DrawAxis();
	void XY_DrawGrid();

	void XY_MouseUp( int x, int y, unsigned int buttons );
	void XY_MouseDown( int x, int y, unsigned int buttons );
	void XY_MouseMoved( int x, int y, unsigned int buttons );

	void NewBrushDrag_Begin( int x, int y );
	void NewBrushDrag( int x, int y, bool square, bool cube );
	void NewBrushDrag_End( int x, int y );

	Vector3 XY_ToPoint( int x, int y, bool snap = false ) const;

	void Move_Begin();
	void Move_End();
	bool m_move_started;

	void Zoom_Begin( int x, int y );
	void Zoom_End();
	bool m_zoom_started;

	void ZoomIn();
	void ZoomOut();
	void ZoomInWithMouse( int x, int y );
	void ZoomOutWithMouse( int x, int y );
	void ZoomCompensateOrigin( int x, int y, float old_scale );
	void FocusOnBounds( const AABB& bounds );

	void SetActive( bool b ){
		m_bActive = b;
		queueDraw();
	};
	bool Active(){
		return m_bActive;
	};

	void SetCustomPivotOrigin( int x, int y ) const;

	void SetViewType( VIEWTYPE n );
	bool m_bActive;

	static class QMenu* m_mnuDrop;

	int m_chasemouse_current_x, m_chasemouse_current_y;
	int m_chasemouse_delta_x, m_chasemouse_delta_y;


	void ChaseMouse();
	bool chaseMouseMotion( int x, int y );

	void updateModelview();
	void updateProjection();
	Matrix4 m_projection;
	Matrix4 m_modelview;

	int m_nWidth;
	int m_nHeight;

	void setBackgroundImage(){
		m_backgroundImage.set( m_viewType );
	}
private:
	BackgroundImage m_backgroundImage;

	float m_fScale;
	Vector3 m_vOrigin;


	View m_view;
	static Shader* m_state_selected;

	unsigned int m_buttonstate;

	Vector3 m_nNewBrushPress;
	scene::Node* m_NewBrushDrag;
	bool m_bNewBrushDrag;

	Vector3 m_mousePosition;

	VIEWTYPE m_viewType;

	void PaintSizeInfo( int nDim1, int nDim2 );

	int m_entityCreate_x, m_entityCreate_y;
	bool m_entityCreate;

	Timer m_render_time;

public:
	void OnContextMenu();
	void ButtonState_onMouseDown( unsigned int buttons ){
		//m_buttonstate |= buttons;
		m_buttonstate = buttons;
	}
	void ButtonState_onMouseUp( unsigned int buttons ){
		//m_buttonstate &= ~buttons;
		m_buttonstate = 0;
	}
	unsigned int getButtonState() const {
		return m_buttonstate;
	}
	void EntityCreate_MouseDown( int x, int y );
	void EntityCreate_MouseMove( int x, int y );
	void EntityCreate_MouseUp( int x, int y );

	void OnEntityCreate( const char* item );
	VIEWTYPE GetViewType() const {
		return m_viewType;
	}
	void SetScale( float f );
	float Scale() const {
		return m_fScale;
	}
	int Width() const {
		return m_nWidth;
	}
	int Height() const {
		return m_nHeight;
	}

	Signal0 onDestroyed;
	Signal3<const WindowVector&, ButtonIdentifier, ModifierFlags> onMouseDown;
	void mouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers );
	typedef Member<XYWnd, void(const WindowVector&, ButtonIdentifier, ModifierFlags), &XYWnd::mouseDown> MouseDownCaller;
};

inline void XYWnd_Update( XYWnd& xywnd ){
	xywnd.m_drawRequired = true;
	xywnd.queueDraw();
}

void XY_Centralize();

struct xywindow_globals_t
{
	Vector3 color_gridback = { .225803f, .225803f, .225803f };
	Vector3 color_gridminor = { .254902f, .254902f, .254902f };
	Vector3 color_gridmajor = { .294118f, .294118f, .294118f };
	Vector3 color_gridblock = { 1.0f, 1.0f, 1.0f };
	Vector3 color_gridtext = { .972549f, .972549f, .972549f };
	Vector3 color_brushes = { 0.f, 0.f, 0.f };
	Vector3 color_selbrushes = { 1.0f, 0.627451f, 0.0f };
	Vector3 color_clipper = { 0.0f, 0.0f, 1.0f };
	Vector3 color_viewname = { 0.516136f, 0.516136f, 0.516136f };
	Vector3 color_camera = { 0.0, 0.0, 1.0 };

	bool m_bNoStipple = true;
};

extern xywindow_globals_t g_xywindow_globals;


VIEWTYPE GlobalXYWnd_getCurrentViewType();

void XY_Top_Shown_Construct( QWidget* parent );
void YZ_Side_Shown_Construct( QWidget* parent );
void XZ_Front_Shown_Construct( QWidget* parent );

void XYWindow_Construct();
void XYWindow_Destroy();

void WXY_SetBackgroundImage();

void XYShow_registerCommands();
void XYWnd_registerShortcuts();
