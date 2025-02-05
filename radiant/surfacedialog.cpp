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
// Surface Dialog
//
// Leonardo Zide (leo@lokigames.com)
//

#include "surfacedialog.h"

#include "debugging/debugging.h"

#include "iscenegraph.h"
#include "itexdef.h"
#include "iundo.h"
#include "iselection.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QGroupBox>
#include <QCheckBox>

#include "signal/isignal.h"
#include "generic/object.h"
#include "math/vector.h"
#include "texturelib.h"
#include "shaderlib.h"
#include "stringio.h"
#include "os/path.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/entry.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/glwidget.h"           //Shamus: For Textool
#include "gtkutil/guisettings.h"
#include "gtkutil/spinbox.h"
#include "map.h"
#include "select.h"
#include "patchmanip.h"
#include "brushmanip.h"
#include "patchdialog.h"
#include "preferences.h"
#include "brush_primit.h"
#include "xywindow.h"
#include "mainframe.h"
#include "gtkdlgs.h"
#include "dialog.h"
#include "brush.h"              //Shamus: for Textool
#include "patch.h"
#include "commands.h"
#include "stream/stringstream.h"
#include "grid.h"
#include "textureentry.h"


class Increment
{
	float& m_f;
public:
	QDoubleSpinBox* m_spin;
	QLineEdit* m_entry;
	Increment( float& f ) : m_f( f ), m_spin( 0 ), m_entry( 0 ){
	}
	void cancel(){
		entry_set_float( m_entry, m_f );
	}
	typedef MemberCaller<Increment, void(), &Increment::cancel> CancelCaller;
	void apply(){
		m_f = entry_get_float( m_entry );
		m_spin->setSingleStep( m_f );
	}
	typedef MemberCaller<Increment, void(), &Increment::apply> ApplyCaller;
};

void SurfaceInspector_GridChange();

class SurfaceInspector : public Dialog
{
	void BuildDialog() override;

	NonModalEntry *m_textureEntry;

	IdleDraw m_idleDraw;

	QCheckBox* m_surfaceFlags[32];
	QCheckBox* m_contentFlags[32];

	NonModalEntry *m_valueEntry;
public:

// Dialog Data
	float m_fitHorizontal;
	float m_fitVertical;

	Increment m_hshiftIncrement;
	Increment m_vshiftIncrement;
	Increment m_hscaleIncrement;
	Increment m_vscaleIncrement;
	Increment m_rotateIncrement;

	SurfaceInspector() :
		m_idleDraw( UpdateCaller( *this ) ),
		m_hshiftIncrement( g_si_globals.shift[0] ),
		m_vshiftIncrement( g_si_globals.shift[1] ),
		m_hscaleIncrement( g_si_globals.scale[0] ),
		m_vscaleIncrement( g_si_globals.scale[1] ),
		m_rotateIncrement( g_si_globals.rotate ){
		m_fitVertical = 1;
		m_fitHorizontal = 1;
	}

	void constructWindow( QWidget* main_window ){
		Create( main_window );
		AddGridChangeCallback( FreeCaller<void(), SurfaceInspector_GridChange>() );
	}
	void destroyWindow(){
		Destroy();
	}
	bool visible() const {
		return GetWidget()->isVisible();
	}
	void queueDraw(){
		if ( visible() ) {
			m_idleDraw.queueDraw();
		}
	}

	void Update();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::Update> UpdateCaller;
	void ApplyShader();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyShader> ApplyShaderCaller;

//void ApplyTexdef();
//typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef> ApplyTexdefCaller;
	void ApplyTexdef_HShift();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef_HShift> ApplyTexdef_HShiftCaller;
	void ApplyTexdef_VShift();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef_VShift> ApplyTexdef_VShiftCaller;
	void ApplyTexdef_HScale();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef_HScale> ApplyTexdef_HScaleCaller;
	void ApplyTexdef_VScale();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef_VScale> ApplyTexdef_VScaleCaller;
	void ApplyTexdef_Rotation();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyTexdef_Rotation> ApplyTexdef_RotationCaller;

	void ApplyFlags();
	typedef MemberCaller<SurfaceInspector, void(), &SurfaceInspector::ApplyFlags> ApplyFlagsCaller;
};

namespace
{
SurfaceInspector* g_SurfaceInspector;

inline SurfaceInspector& getSurfaceInspector(){
	ASSERT_NOTNULL( g_SurfaceInspector );
	return *g_SurfaceInspector;
}
}

void SurfaceInspector_constructWindow( QWidget* main_window ){
	getSurfaceInspector().constructWindow( main_window );
}
void SurfaceInspector_destroyWindow(){
	getSurfaceInspector().destroyWindow();
}

void SurfaceInspector_queueDraw(){
	getSurfaceInspector().queueDraw();
}

namespace
{
CopiedString g_selectedShader;
TextureProjection g_selectedTexdef;
ContentsFlagsValue g_selectedFlags;
}

void SurfaceInspector_SetSelectedShader( const char* shader ){
	g_selectedShader = shader;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SetSelectedTexdef( const TextureProjection& projection ){
	g_selectedTexdef = projection;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SetSelectedFlags( const ContentsFlagsValue& flags ){
	g_selectedFlags = flags;
	SurfaceInspector_queueDraw();
}

static bool s_texture_selection_dirty = false;
static bool s_patch_mode = false;

void SurfaceInspector_updateSelection(){
	s_texture_selection_dirty = true;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SelectionChanged( const Selectable& selectable ){
	SurfaceInspector_updateSelection();
}

void SurfaceInspector_SetCurrent_FromSelected(){
	if ( s_texture_selection_dirty == true ) {
		s_texture_selection_dirty = false;
		if ( !g_SelectedFaceInstances.empty() ) {
			s_patch_mode = false;
			TextureProjection projection;
//This *may* be the point before it fucks up... Let's see!
//Yep, there was a call to removeScale in there...
			Scene_BrushGetTexdef_Component_Selected( GlobalSceneGraph(), projection );

			SurfaceInspector_SetSelectedTexdef( projection );

			CopiedString name;
			Scene_BrushGetShader_Component_Selected( GlobalSceneGraph(), name );
			if ( !name.empty() ) {
				SurfaceInspector_SetSelectedShader( name.c_str() );
			}

			ContentsFlagsValue flags;
			Scene_BrushGetFlags_Component_Selected( GlobalSceneGraph(), flags );
			SurfaceInspector_SetSelectedFlags( flags );
		}
		else
		{
			TextureProjection projection;
			CopiedString name;
			s_patch_mode = false;
			if( !Scene_BrushGetShaderTexdef_Selected( GlobalSceneGraph(), name, projection ) )
				if( Scene_PatchGetShaderTexdef_Selected( GlobalSceneGraph(), name, projection ) )
					s_patch_mode = true;
			SurfaceInspector_SetSelectedTexdef( projection );
			if ( !name.empty() ) {
				SurfaceInspector_SetSelectedShader( name.c_str() );
			}

			ContentsFlagsValue flags( 0, 0, 0, false );
			Scene_BrushGetFlags_Selected( GlobalSceneGraph(), flags );
			SurfaceInspector_SetSelectedFlags( flags );
		}
	}
}

const char* SurfaceInspector_GetSelectedShader(){
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedShader.c_str();
}

const TextureProjection& SurfaceInspector_GetSelectedTexdef(){
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedTexdef;
}

const ContentsFlagsValue& SurfaceInspector_GetSelectedFlags(){
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedFlags;
}



/*
   ===================================================

   SURFACE INSPECTOR

   ===================================================
 */

si_globals_t g_si_globals;


// make the shift increments match the grid settings
// the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
// this depends on a scale value if you have selected a particular texture on which you want it to work:
// we move the textures in pixels, not world units. (i.e. increment values are in pixel)
// depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
// increment * scale = gridsize
// hscale and vscale are optional parameters, if they are zero they will be set to the default scale
// NOTE: the default scale depends if you are using BP mode or regular.
// For regular it's 0.5f (128 pixels cover 64 world units), for BP it's simply 1.0f
// see fenris #2810
void DoSnapTToGrid( float hscale, float vscale ){
	g_si_globals.shift[0] = static_cast<float>( float_to_integer( static_cast<float>( GetGridSize() ) / hscale ) );
	g_si_globals.shift[1] = static_cast<float>( float_to_integer( static_cast<float>( GetGridSize() ) / vscale ) );
	getSurfaceInspector().queueDraw();
}

void SurfaceInspector_GridChange(){
	if ( g_si_globals.m_bSnapTToGrid ) {
		DoSnapTToGrid( Texdef_getDefaultTextureScale(), Texdef_getDefaultTextureScale() );
	}
}

// make the shift increments match the grid settings
// the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
// this depends on the current texture scale used?
// we move the textures in pixels, not world units. (i.e. increment values are in pixel)
// depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
// increment * scale = gridsize
static void OnBtnMatchGrid(){
	const float hscale = getSurfaceInspector().m_hscaleIncrement.m_spin->value();
	const float vscale = getSurfaceInspector().m_vscaleIncrement.m_spin->value();

	if ( hscale == 0.0f || vscale == 0.0f ) {
		globalErrorStream() << "ERROR: unexpected scale == 0.0f\n";
		return;
	}

	DoSnapTToGrid( hscale, vscale );
}

// DoSurface will always try to show the surface inspector
// or update it because something new has been selected
// Shamus: It does get called when the SI is hidden, but not when you select something new. ;-)
void DoSurface(){
	getSurfaceInspector().Update();
	//getSurfaceInspector().importData(); //happens in .ShowDlg() anyway
	getSurfaceInspector().ShowDlg();
}

void SurfaceInspector_toggleShown(){
	if ( getSurfaceInspector().visible() ) {
		getSurfaceInspector().HideDlg();
	}
	else
	{
		DoSurface();
	}
}

#include "camwindow.h"

enum EProjectTexture
{
	eProjectAxial = 0,
	eProjectOrtho = 1,
	eProjectCam = 2,
};

void SurfaceInspector_ProjectTexture( EProjectTexture type, bool isGuiClick ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_QUAKE )
		globalWarningStream() << "function doesn't work for *brushes*, having Axial Projection type\n"; //works for patches

	texdef_t texdef;
	if( isGuiClick ){ //gui buttons
		getSurfaceInspector().exportData();
		texdef.shift[0] = getSurfaceInspector().m_hshiftIncrement.m_spin->value();
		texdef.shift[1] = getSurfaceInspector().m_vshiftIncrement.m_spin->value();
		texdef.scale[0] = getSurfaceInspector().m_hscaleIncrement.m_spin->value();
		texdef.scale[1] = getSurfaceInspector().m_vscaleIncrement.m_spin->value();
		texdef.rotate = getSurfaceInspector().m_rotateIncrement.m_spin->value();
	}
	else{ //bind
		texdef.scale[0] = texdef.scale[1] = Texdef_getDefaultTextureScale();
	}

	const auto str = StringStream<32>( "textureProject", ( type == eProjectAxial? "Axial" : type == eProjectOrtho? "Ortho" : "Cam" ) );
	UndoableCommand undo( str );

	Vector3 direction;

	switch ( type )
	{
	case eProjectAxial:
		return Select_ProjectTexture( texdef, 0 );
	case eProjectOrtho:
		direction = g_vector3_axes[GlobalXYWnd_getCurrentViewType()];
		break;
	case eProjectCam:
		//direction = -g_pParentWnd->GetCamWnd()->getCamera().vpn;
		direction = -Camera_getViewVector( *g_pParentWnd->GetCamWnd() );
		break;
	}

	Select_ProjectTexture( texdef, &direction );
}

void SurfaceInspector_ProjectTexture_eProjectAxial(){
	SurfaceInspector_ProjectTexture( eProjectAxial, false );
}
void SurfaceInspector_ProjectTexture_eProjectOrtho(){
	SurfaceInspector_ProjectTexture( eProjectOrtho, false );
}
void SurfaceInspector_ProjectTexture_eProjectCam(){
	SurfaceInspector_ProjectTexture( eProjectCam, false );
}

void SurfaceInspector_ResetTexture(){
	UndoableCommand undo( "textureReset/Cap" );
	TextureProjection projection;
	TexDef_Construct_Default( projection );

	Select_SetTexdef( projection, false, true );
	Scene_PatchCapTexture_Selected( GlobalSceneGraph() );
}

void SurfaceInspector_InvertTextureHorizontally(){
	UndoableCommand undo( "textureInvertHorizontally" );
	const float shift = -getSurfaceInspector().m_hshiftIncrement.m_spin->value();
	const float scale = -getSurfaceInspector().m_hscaleIncrement.m_spin->value();
	Select_SetTexdef( &shift, 0, &scale, 0, 0 );
	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 0 );
}
void SurfaceInspector_InvertTextureVertically(){
	UndoableCommand undo( "textureInvertVertically" );
	const float shift = -getSurfaceInspector().m_vshiftIncrement.m_spin->value();
	const float scale = -getSurfaceInspector().m_vscaleIncrement.m_spin->value();
	Select_SetTexdef( 0, &shift, 0, &scale, 0 );
	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 1 );
}


void SurfaceInspector_FitTexture(){
	UndoableCommand undo( "textureAutoFit" );
	getSurfaceInspector().exportData();
	Select_FitTexture( getSurfaceInspector().m_fitHorizontal, getSurfaceInspector().m_fitVertical );
}

void SurfaceInspector_FaceFitWidth(){
	UndoableCommand undo( "textureAutoFitWidth" );
	getSurfaceInspector().exportData();
	Select_FitTexture( getSurfaceInspector().m_fitHorizontal, 0 );
}

void SurfaceInspector_FaceFitHeight(){
	UndoableCommand undo( "textureAutoFitHeight" );
	getSurfaceInspector().exportData();
	Select_FitTexture( 0, getSurfaceInspector().m_fitVertical );
}

void SurfaceInspector_FaceFitWidthOnly(){
	UndoableCommand undo( "textureAutoFitWidthOnly" );
	getSurfaceInspector().exportData();
	Select_FitTexture( getSurfaceInspector().m_fitHorizontal, 0, true );
}

void SurfaceInspector_FaceFitHeightOnly(){
	UndoableCommand undo( "textureAutoFitHeightOnly" );
	getSurfaceInspector().exportData();
	Select_FitTexture( 0, getSurfaceInspector().m_fitVertical, true );
}

class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick ){
			QMouseEvent *mouseEvent = static_cast<QMouseEvent *>( event );
			if( mouseEvent->button() == Qt::MouseButton::RightButton ){
				SurfaceInspector_FaceFitWidthOnly();
				return true;
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
OnBtnFaceFitWidthOnly;

class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick ){
			QMouseEvent *mouseEvent = static_cast<QMouseEvent *>( event );
			if( mouseEvent->button() == Qt::MouseButton::RightButton ){
				SurfaceInspector_FaceFitHeightOnly();
				return true;
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
OnBtnFaceFitHeightOnly;

static void OnBtnUnsetFlags(){
	UndoableCommand undo( "flagsUnSetSelected" );
	Select_SetFlags( ContentsFlagsValue( 0, 0, 0, false ) );
}


typedef const char* FlagName;

const FlagName surfaceflagNamesDefault[32] = {
	"surf1",
	"surf2",
	"surf3",
	"surf4",
	"surf5",
	"surf6",
	"surf7",
	"surf8",
	"surf9",
	"surf10",
	"surf11",
	"surf12",
	"surf13",
	"surf14",
	"surf15",
	"surf16",
	"surf17",
	"surf18",
	"surf19",
	"surf20",
	"surf21",
	"surf22",
	"surf23",
	"surf24",
	"surf25",
	"surf26",
	"surf27",
	"surf28",
	"surf29",
	"surf30",
	"surf31",
	"surf32"
};

const FlagName contentflagNamesDefault[32] = {
	"cont1",
	"cont2",
	"cont3",
	"cont4",
	"cont5",
	"cont6",
	"cont7",
	"cont8",
	"cont9",
	"cont10",
	"cont11",
	"cont12",
	"cont13",
	"cont14",
	"cont15",
	"cont16",
	"cont17",
	"cont18",
	"cont19",
	"cont20",
	"cont21",
	"cont22",
	"cont23",
	"cont24",
	"cont25",
	"cont26",
	"cont27",
	"cont28",
	"cont29",
	"cont30",
	"cont31",
	"cont32"
};

const char* getSurfaceFlagName( std::size_t bit ){
	const char* value = g_pGameDescription->getKeyValue( surfaceflagNamesDefault[bit] );
	if ( string_empty( value ) ) {
		return surfaceflagNamesDefault[bit];
	}
	return value;
}

const char* getContentFlagName( std::size_t bit ){
	const char* value = g_pGameDescription->getKeyValue( contentflagNamesDefault[bit] );
	if ( string_empty( value ) ) {
		return contentflagNamesDefault[bit];
	}
	return value;
}

class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		// QEvent::KeyPress & return true: override QDialog keyPressEvent also
		if( event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Return
			 || keyEvent->key() == Qt::Key_Enter
			 || keyEvent->key() == Qt::Key_Escape
			 || keyEvent->key() == Qt::Key_Tab
			 || ( ( keyEvent->modifiers() == Qt::KeyboardModifier::NoModifier
			     || keyEvent->modifiers() == Qt::KeyboardModifier::KeypadModifier ) // do not filter editor's shortcuts with modifiers
			  && ( keyEvent->key() == Qt::Key_Up
			    || keyEvent->key() == Qt::Key_Down
			    || keyEvent->key() == Qt::Key_PageUp
			    || keyEvent->key() == Qt::Key_PageDown ) )
			){
				event->accept();
				return true;
			}
		}
		// clear focus widget while showing to keep global shortcuts working
#ifdef WIN32
		else if( event->type() == QEvent::Show ) {
#else
		else if( event->type() == QEvent::WindowActivate ) { // fixme hack hack: events order varies in OSes, QEvent::Show doesn't work in Linux
		// QEvent::WindowActivate seems preferable for usability, but allows QLineEdit content selection w/o focusing it in WIN32
#endif
			QTimer::singleShot( 0, [obj](){
				if( static_cast<QWidget*>( obj )->focusWidget() != nullptr )
					static_cast<QWidget*>( obj )->focusWidget()->clearFocus();
			} );
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_pressedKeysFilter;

// =============================================================================
// SurfaceInspector class

void SurfaceInspector::BuildDialog(){
	GetWidget()->setWindowTitle( "Surface Inspector" );

	g_guiSettings.addWindow( GetWidget(), "SurfaceInspector/geometry", 99, 99 );

	GetWidget()->installEventFilter( &g_pressedKeysFilter );

//.	window_connect_focus_in_clear_focus_widget( window );

	{
		auto *vbox = new QVBoxLayout( GetWidget() );
		{
			auto *form = new QFormLayout;
			vbox->addLayout( form );
			{
				m_textureEntry = new NonModalEntry( ApplyShaderCaller( *this ), UpdateCaller( *this ) );
				GlobalTextureEntryCompletion::instance().connect( m_textureEntry );
				form->addRow( "Texture", m_textureEntry );
			}
		}
		{
			auto *grid = new QGridLayout; // 5 x 5
			vbox->addLayout( grid );

			struct GridRowAdder
			{
				QGridLayout * const m_grid;
				const int m_colLabel;
				const int m_colField;
				int m_row = 0;
				void addRow( QWidget *label, QWidget *field ){
					m_grid->addWidget( label, m_row, m_colLabel );
					m_grid->addWidget( field, m_row, m_colField );
					++m_row;
				}
				void addRow( const char *label, QWidget *field ){
					addRow( new QLabel( label ), field );
				}
			};

			{
				GridRowAdder adder{ grid, 0, 2 };
				adder.m_grid->setColumnStretch( adder.m_colField, 1 );
				{
					auto spin = new NonModalSpinner( -8192, 8192, 0, 2, 2 );
					spin->setCallbacks( ApplyTexdef_HShiftCaller( *this ), UpdateCaller( *this ) );
					m_hshiftIncrement.m_spin = spin;
					adder.addRow( new SpinBoxLabel( "Horizontal shift", spin ), spin );
				}
				{
					auto spin = new NonModalSpinner( -8192, 8192, 0, 2, 2 );
					spin->setCallbacks( ApplyTexdef_VShiftCaller( *this ), UpdateCaller( *this ) );
					m_vshiftIncrement.m_spin = spin;
					adder.addRow( new SpinBoxLabel( "Vertical shift", spin ), spin );
				}
				{
					auto spin = new NonModalSpinner( -8192, 8192, .5, 5, .5 );
					spin->setCallbacks( ApplyTexdef_HScaleCaller( *this ), UpdateCaller( *this ) );
					m_hscaleIncrement.m_spin = spin;
					adder.addRow( new SpinBoxLabel( "Horizontal stretch", spin ), spin );

					auto *b = new QToolButton;
					adder.m_grid->addWidget( b, adder.m_row - 1, adder.m_colLabel + 1 );
					b->setText( "-" );
					QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_InvertTextureHorizontally );
				}
				{
					auto spin = new NonModalSpinner( -8192, 8192, .5, 5, .5 );
					spin->setCallbacks( ApplyTexdef_VScaleCaller( *this ), UpdateCaller( *this ) );
					m_vscaleIncrement.m_spin = spin;
					adder.addRow( new SpinBoxLabel( "Vertical stretch", spin ), spin );

					auto *b = new QToolButton;
					adder.m_grid->addWidget( b, adder.m_row - 1, adder.m_colLabel + 1 );
					b->setText( "-" );
					QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_InvertTextureVertically );
				}
				{
					auto spin = new NonModalSpinner( -360, 360, 0, 2, 45, true );
					spin->setCallbacks( ApplyTexdef_RotationCaller( *this ), UpdateCaller( *this ) );
					m_rotateIncrement.m_spin = spin;
					adder.addRow( new SpinBoxLabel( "Rotate", spin ), spin );
				}
			}
			{
				GridRowAdder adder{ grid, 3, 4 };
				{
					auto entry = new NonModalEntry( Increment::ApplyCaller( m_hshiftIncrement ), Increment::CancelCaller( m_hshiftIncrement ) );
					m_hshiftIncrement.m_entry = entry;
					adder.addRow( "Step", entry );
				}
				{
					auto entry = new NonModalEntry( Increment::ApplyCaller( m_vshiftIncrement ), Increment::CancelCaller( m_vshiftIncrement ) );
					m_vshiftIncrement.m_entry = entry;
					adder.addRow( "Step", entry );
				}
				{
					auto entry = new NonModalEntry( Increment::ApplyCaller( m_hscaleIncrement ), Increment::CancelCaller( m_hscaleIncrement ) );
					m_hscaleIncrement.m_entry = entry;
					adder.addRow( "Step", entry );
				}
				{
					auto entry = new NonModalEntry( Increment::ApplyCaller( m_vscaleIncrement ), Increment::CancelCaller( m_vscaleIncrement ) );
					m_vscaleIncrement.m_entry = entry;
					adder.addRow( "Step", entry );
				}
				{
					auto entry = new NonModalEntry( Increment::ApplyCaller( m_rotateIncrement ), Increment::CancelCaller( m_rotateIncrement ) );
					m_rotateIncrement.m_entry = entry;
					adder.addRow( "Step", entry );
				}
			}
		}
		{
			// match grid button
			auto *b = new QPushButton( "Match Grid" );
			b->setMinimumWidth( 10 );
			vbox->addWidget( b, 0, Qt::AlignmentFlag::AlignRight );
			QObject::connect( b, &QAbstractButton::clicked, OnBtnMatchGrid );
		}

		{
			auto *frame = new QGroupBox( "Texturing" );
			vbox->addWidget( frame );
			auto *grid = new QGridLayout( frame ); // 4 x 4
			{
				auto *b = new QPushButton( "Width" );
				b->setMinimumWidth( 10 );
				b->setToolTip( "Fit texture width, scale height\nRightClick: fit width, keep height" );
				grid->addWidget( b, 0, 2 );
				QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_FaceFitWidth );
				b->installEventFilter( &OnBtnFaceFitWidthOnly );
			}
			{
				auto *b = new QPushButton( "Height" );
				b->setMinimumWidth( 10 );
				b->setToolTip( "Fit texture height, scale width\nRightClick: fit height, keep width" );
				grid->addWidget( b, 0, 3 );
				QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_FaceFitHeight );
				b->installEventFilter( &OnBtnFaceFitHeightOnly );
			}
			{
				auto *b = new QPushButton( "Reset" );
				b->setMinimumWidth( 10 );
				grid->addWidget( b, 1, 0 );
				QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_ResetTexture );
			}
			{
				auto *b = new QPushButton( "Fit" );
				b->setMinimumWidth( 10 );
				grid->addWidget( b, 1, 1 );
				QObject::connect( b, &QAbstractButton::clicked, SurfaceInspector_FitTexture );
			}
			{
				auto *spin = new DoubleSpinBox( 0, 1 << 9, 1, 3, 1 );
				grid->addWidget( spin, 1, 2 );
				AddDialogData( *spin, m_fitHorizontal );
			}
			{
				auto *spin = new DoubleSpinBox( 0, 1 << 9, 1, 3, 1 );
				grid->addWidget( spin, 1, 3 );
				AddDialogData( *spin, m_fitVertical );
			}
			{
				grid->addWidget( new QLabel( "Project:" ), 2, 0 );
			}
			{
				auto *b = new QPushButton( "Axial" );
				b->setMinimumWidth( 10 );
				b->setToolTip( "Axial projection (along nearest axis)" );
				grid->addWidget( b, 2, 1 );
				QObject::connect( b, &QAbstractButton::clicked, [](){ SurfaceInspector_ProjectTexture( eProjectAxial, true ); } );
			}
			{
				auto *b = new QPushButton( "Ortho" );
				b->setMinimumWidth( 10 );
				b->setToolTip( "Project along active ortho view" );
				grid->addWidget( b, 2, 2 );
				QObject::connect( b, &QAbstractButton::clicked, [](){ SurfaceInspector_ProjectTexture( eProjectOrtho, true ); } );
			}
			{
				auto *b = new QPushButton( "Cam" );
				b->setMinimumWidth( 10 );
				b->setToolTip( "Project along camera view direction" );
				grid->addWidget( b, 2, 3 );
				QObject::connect( b, &QAbstractButton::clicked, [](){ SurfaceInspector_ProjectTexture( eProjectCam, true ); } );
			}
			{
				grid->addWidget( new QLabel( "Patch" ), 3, 0 );
			}
			{
				auto *b = new QPushButton( "Natural" );
				b->setMinimumWidth( 10 );
				grid->addWidget( b, 3, 1 );
				QObject::connect( b, &QAbstractButton::clicked, Patch_NaturalTexture );
			}
			if ( g_pGameDescription->mGameType == "doom3" ){
				grid->addWidget( patch_tesselation_create(), 3, 2, 1, 2 );
			}
		}
		if ( !string_empty( g_pGameDescription->getKeyValue( "si_flags" ) ) )
		{
			{
				auto *frame = new QGroupBox( "Surface Flags" );
				frame->setCheckable( true );
				frame->setChecked( false );
				vbox->addWidget( frame );

				auto *box = new QVBoxLayout( frame );
				box->setContentsMargins( 0, 0, 0, 0 );
				auto *container = new QWidget;
				box->addWidget( container );
				auto *grid = new QGridLayout( container );

				// QObject::connect( frame, &QGroupBox::clicked, container, &QWidget::setVisible );
				QObject::connect( frame, &QGroupBox::clicked, [container, wnd = GetWidget()]( bool checked ){
					container->setVisible( checked );
					QTimer::singleShot( 0, [wnd](){ wnd->adjustSize(); wnd->resize( 99, 99 ); } );
				} );
				container->setVisible( false );
				{
					QCheckBox** p = m_surfaceFlags;

					for ( int c = 0; c != 4; ++c )
					{
						for ( int r = 0; r != 8; ++r )
						{
							auto *check = new QCheckBox( getSurfaceFlagName( c * 8 + r ) );
							grid->addWidget( check, r, c );
							*p++ = check;
							QObject::connect( check, &QAbstractButton::clicked, ApplyFlagsCaller( *this ) );
						}
					}
				}
			}
			{
				auto *frame = new QGroupBox( "Content Flags" );
				frame->setCheckable( true );
				frame->setChecked( false );
				vbox->addWidget( frame );

				auto *box = new QVBoxLayout( frame );
				box->setContentsMargins( 0, 0, 0, 0 );
				auto *container = new QWidget;
				box->addWidget( container );
				auto *grid = new QGridLayout( container );

				QObject::connect( frame, &QGroupBox::clicked, [container, wnd = GetWidget()]( bool checked ){
					container->setVisible( checked );
					QTimer::singleShot( 0, [wnd](){ wnd->adjustSize(); wnd->resize( 99, 99 ); } );
				} );
				container->setVisible( false );
				{
					QCheckBox** p = m_contentFlags;

					for ( int c = 0; c != 4; ++c )
					{
						for ( int r = 0; r != 8; ++r )
						{
							auto *check = new QCheckBox( getContentFlagName( c * 8 + r ) );
							grid->addWidget( check, r, c );
							*p++ = check;
							QObject::connect( check, &QAbstractButton::clicked, ApplyFlagsCaller( *this ) );
						}
					}
					// not allowed to modify detail flag using Surface Inspector
					m_contentFlags[BRUSH_DETAIL_FLAG]->setEnabled( false );
				}
			}
			{
				auto *frame = new QGroupBox( "Value" );
				vbox->addWidget( frame );
				{
					{
						m_valueEntry = new NonModalEntry( ApplyFlagsCaller( *this ), UpdateCaller( *this ) );
					}
					{
						auto *b = new QPushButton( "Unset" );
						b->setToolTip( "Unset flags" );
						QObject::connect( b, &QAbstractButton::clicked, OnBtnUnsetFlags );

						auto *form = new QFormLayout( frame );
						form->addRow( m_valueEntry, b );
					}
				}
			}
		}
		vbox->addStretch( 1 );
	}
}

/*
   ==============
   Update

   Set the fields to the current texdef (i.e. map/texdef -> dialog widgets)
   if faces selected (instead of brushes) -> will read this face texdef, else current texdef
   if only patches selected, will read the patch texdef
   ===============
 */

void SurfaceInspector::Update(){
	const char * name = SurfaceInspector_GetSelectedShader();

	if ( shader_is_texture( name ) ) {
		m_textureEntry->setText( shader_get_textureName( name ) );
	}
	else
	{
		m_textureEntry->clear();
	}

	texdef_t shiftScaleRotate;
	if( s_patch_mode )
		ShiftScaleRotate_fromPatch( shiftScaleRotate, SurfaceInspector_GetSelectedTexdef() );
	else
		ShiftScaleRotate_fromFace( shiftScaleRotate, SurfaceInspector_GetSelectedTexdef() );

	{
		m_hshiftIncrement.m_spin->setValue( shiftScaleRotate.shift[0] );
		m_hshiftIncrement.m_spin->setSingleStep( g_si_globals.shift[0] );
		entry_set_float( m_hshiftIncrement.m_entry, g_si_globals.shift[0] );
	}

	{
		m_vshiftIncrement.m_spin->setValue( shiftScaleRotate.shift[1] );
		m_vshiftIncrement.m_spin->setSingleStep( g_si_globals.shift[1] );
		entry_set_float( m_vshiftIncrement.m_entry, g_si_globals.shift[1] );
	}

	{
		m_hscaleIncrement.m_spin->setValue( shiftScaleRotate.scale[0] );
		m_hscaleIncrement.m_spin->setSingleStep( g_si_globals.scale[0] );
		entry_set_float( m_hscaleIncrement.m_entry, g_si_globals.scale[0] );
	}

	{
		m_vscaleIncrement.m_spin->setValue( shiftScaleRotate.scale[1] );
		m_vscaleIncrement.m_spin->setSingleStep( g_si_globals.scale[1] );
		entry_set_float( m_vscaleIncrement.m_entry, g_si_globals.scale[1] );
	}

	{
		m_rotateIncrement.m_spin->setValue( shiftScaleRotate.rotate );
		m_rotateIncrement.m_spin->setSingleStep( g_si_globals.rotate );
		entry_set_float( m_rotateIncrement.m_entry, g_si_globals.rotate );
	}

	patch_tesselation_update();

	if ( !string_empty( g_pGameDescription->getKeyValue( "si_flags" ) ) ) {
		ContentsFlagsValue flags( SurfaceInspector_GetSelectedFlags() );

		entry_set_int( m_valueEntry, flags.m_value );

		for ( QCheckBox** p = m_surfaceFlags; p != m_surfaceFlags + 32; ++p )
		{
			( *p )->setChecked( flags.m_surfaceFlags & ( 1 << ( p - m_surfaceFlags ) ) );
		}

		for ( QCheckBox** p = m_contentFlags; p != m_contentFlags + 32; ++p )
		{
			( *p )->setChecked( flags.m_contentFlags & ( 1 << ( p - m_contentFlags ) ) );
		}
	}
}

/*
   ==============
   Apply

   Reads the fields to get the current texdef (i.e. widgets -> MAP)
   in brush primitive mode, grab the fake shift scale rot and compute a new texture matrix
   ===============
 */
void SurfaceInspector::ApplyShader(){
	const auto name = StringStream<64>( GlobalTexturePrefix_get(), PathCleaned( m_textureEntry->text().toLatin1().constData() ) );

	// TTimo: detect and refuse invalid texture names (at least the ones with spaces)
	if ( !texdef_name_valid( name ) ) {
		globalErrorStream() << "invalid texture name '" << name << "'\n";
		SurfaceInspector_queueDraw();
		return;
	}

	UndoableCommand undo( "textureNameSetSelected" );
	Select_SetShader( name );
}
#if 0
void SurfaceInspector::ApplyTexdef(){
	texdef_t shiftScaleRotate;

	shiftScaleRotate.shift[0] = m_hshiftIncrement.m_spin->value();
	shiftScaleRotate.shift[1] = m_vshiftIncrement.m_spin->value();
	shiftScaleRotate.scale[0] = m_hscaleIncrement.m_spin->value();
	shiftScaleRotate.scale[1] = m_vscaleIncrement.m_spin->value();
	shiftScaleRotate.rotate = m_rotateIncrement.m_spin->value();

	TextureProjection projection;
	ShiftScaleRotate_toFace( shiftScaleRotate, projection );

	UndoableCommand undo( "textureProjectionSetSelected" );
	Select_SetTexdef( projection );
}
#endif
void SurfaceInspector::ApplyTexdef_HShift(){
	const float value = m_hshiftIncrement.m_spin->value();
	const auto command = StringStream<64>( "textureProjectionSetSelected -hShift ", value );
	UndoableCommand undo( command );
	Select_SetTexdef( &value, 0, 0, 0, 0 );
	Patch_SetTexdef( &value, 0, 0, 0, 0 );
}

void SurfaceInspector::ApplyTexdef_VShift(){
	const float value = m_vshiftIncrement.m_spin->value();
	const auto command = StringStream<64>( "textureProjectionSetSelected -vShift ", value );
	UndoableCommand undo( command );
	Select_SetTexdef( 0, &value, 0, 0, 0 );
	Patch_SetTexdef( 0, &value, 0, 0, 0 );
}

void SurfaceInspector::ApplyTexdef_HScale(){
	const float value = m_hscaleIncrement.m_spin->value();
	const auto command = StringStream<64>( "textureProjectionSetSelected -hScale ", value );
	UndoableCommand undo( command );
	Select_SetTexdef( 0, 0, &value, 0, 0 );
	Patch_SetTexdef( 0, 0, &value, 0, 0 );
}

void SurfaceInspector::ApplyTexdef_VScale(){
	const float value = m_vscaleIncrement.m_spin->value();
	const auto command = StringStream<64>( "textureProjectionSetSelected -vScale ", value );
	UndoableCommand undo( command );
	Select_SetTexdef( 0, 0, 0, &value, 0 );
	Patch_SetTexdef( 0, 0, 0, &value, 0 );
}

void SurfaceInspector::ApplyTexdef_Rotation(){
	const float value = m_rotateIncrement.m_spin->value();
	const auto command = StringStream<64>( "textureProjectionSetSelected -rotation ", static_cast<float>( float_to_integer( value * 100.f ) ) / 100.f );
	UndoableCommand undo( command );
	Select_SetTexdef( 0, 0, 0, 0, &value );
	Patch_SetTexdef( 0, 0, 0, 0, &value );
}

void SurfaceInspector::ApplyFlags(){
	unsigned int surfaceflags = 0;
	for ( QCheckBox** p = m_surfaceFlags; p != m_surfaceFlags + 32; ++p )
	{
		if ( ( *p )->isChecked() ) {
			surfaceflags |= ( 1 << ( p - m_surfaceFlags ) );
		}
	}

	unsigned int contentflags = 0;
	for ( QCheckBox** p = m_contentFlags; p != m_contentFlags + 32; ++p )
	{
		if ( ( *p )->isChecked() ) {
			contentflags |= ( 1 << ( p - m_contentFlags ) );
		}
	}

	int value = entry_get_int( m_valueEntry );

	UndoableCommand undo( "flagsSetSelected" );
	Select_SetFlags( ContentsFlagsValue( surfaceflags, contentflags, value, true ) );
}



enum EPasteMode{
	ePasteNone,
	ePasteValues,
	ePasteSeamless,
	ePasteProject,
};

EPasteMode pastemode_for_modifiers( bool shift, bool ctrl ){
	if( shift )
		return ctrl? ePasteProject : ePasteValues;
	else if( ctrl )
		return ePasteSeamless;
	return ePasteNone;
}
bool pastemode_if_setShader( EPasteMode mode, bool alt ){
	return ( mode == ePasteNone ) || !alt;
}


class PatchData
{
	size_t m_width = 0;
	size_t m_height = 0;
	typedef Array<PatchControl> PatchControlArray;
	PatchControlArray m_ctrl;
public:
	void copy( const Patch& patch ){
		m_width = patch.getWidth();
		m_height = patch.getHeight();
		m_ctrl = patch.getControlPoints();
	}
	size_t getWidth() const {
		return m_width;
	}
	size_t getHeight() const {
		return m_height;
	}
	const PatchControl& ctrlAt( size_t row, size_t col ) const {
		return m_ctrl[row * m_width + col];
	}
	const PatchControl *data() const {
		return m_ctrl.data();
	}
};

class FaceTexture
{
public:
	TextureProjection m_projection; //BP part is removeScale()'d
	ContentsFlagsValue m_flags;

	Plane3 m_plane;
	Winding m_winding;
	std::size_t m_width;
	std::size_t m_height;

	PatchData m_patch;

	float m_light;
	Vector3 m_colour;

	enum ePasteSource{
		eBrush,
		ePatch
	} m_pasteSource;

	FaceTexture() : m_plane( 0, 0, 1, 0 ), m_width( 64 ), m_height( 64 ), m_light( 300 ), m_colour( 1, 1, 1 ), m_pasteSource( eBrush ) {
		m_projection.m_basis_s = Vector3( 0.7071067811865, 0.7071067811865, 0 );
		m_projection.m_basis_t = Vector3( -0.4082482904639, 0.4082482904639, -0.4082482904639 * 2.0 );
	}
};

FaceTexture g_faceTextureClipboard;

void FaceTextureClipboard_setDefault(){
	g_faceTextureClipboard.m_flags = ContentsFlagsValue( 0, 0, 0, false );
	g_faceTextureClipboard.m_projection.m_texdef = texdef_t();
	g_faceTextureClipboard.m_projection.m_brushprimit_texdef = brushprimit_texdef_t();
	TexDef_Construct_Default( g_faceTextureClipboard.m_projection );
}

void TextureClipboard_textureSelected( const char* shader ){
	FaceTextureClipboard_setDefault();
}


class PatchEdgeIter
{
public:
	enum Type
	{
		eRowForward, // iterate inside a row
		eRowBack,
		eColForward, // iterate inside a column
		eColBack
	};
private:
	const PatchControl* const m_ctrl;
	const int m_width;
	const int m_height;
	const Type m_type;
	int m_row;
	int m_col;
	const PatchControl& ctrlAt( size_t row, size_t col ) const {
		return m_ctrl[row * m_width + col];
	}
public:
	PatchEdgeIter( const PatchData& patch, Type type, int rowOrCol ) :
		m_ctrl( patch.data() ),
		m_width( patch.getWidth() ),
		m_height( patch.getHeight() ),
		m_type( type ),
		m_row( type == eColForward? 0 : type == eColBack? patch.getHeight() - 1 : rowOrCol ),
		m_col( type == eRowForward? 0 : type == eRowBack? patch.getWidth() - 1 : rowOrCol ) {
	}
	PatchEdgeIter( const PatchEdgeIter& other ) = default;
	PatchEdgeIter( const PatchEdgeIter& other, Type type ) :
		m_ctrl( other.m_ctrl ),
		m_width( other.m_width ),
		m_height( other.m_height ),
		m_type( type ),
		m_row( other.m_row ),
		m_col( other.m_col ) {
	}
	const PatchControl& operator*() const {
		return ctrlAt( m_row, m_col );
	}
	operator bool() const {
		return m_row >= 0 && m_row < m_height && m_col >= 0 && m_col < m_width;
	}
	void operator++(){
		operator+=( 1 );
	}
	void operator+=( size_t inc ){
		switch ( m_type )
		{
		case eRowForward:
			m_col += inc;
			break;
		case eRowBack:
			m_col -= inc;
			break;
		case eColForward:
			m_row += inc;
			break;
		case eColBack:
			m_row -= inc;
			break;
		}
	}
	PatchEdgeIter operator+( size_t inc ) const {
		PatchEdgeIter it( *this );
		it += inc;
		return it;
	}
	PatchEdgeIter getCrossIter() const {
		switch ( m_type )
		{
		case eRowForward:
			return PatchEdgeIter( *this, eColBack );
		case eRowBack:
			return PatchEdgeIter( *this, eColForward );
		case eColForward:
			return PatchEdgeIter( *this, eRowForward );
		case eColBack:
			return PatchEdgeIter( *this, eRowBack );
		}
	}
};

// returns 0 or 3 CW points
static std::vector<const PatchControl*> Patch_getClosestTriangle( const PatchData& patch, const Winding& w, const Plane3& plane ){
	/*
	// height = 3
	col  0  1  2  3  4
	    10 11 12 13 14  // row 2
	     5  6  7  8  9  // row 1
	     0  1  2  3  4  // row 0 // width = 5
	*/

	const auto triangle_ok = []( const PatchControl& p0, const PatchControl& p1, const PatchControl& p2 ){
		return vector3_length_squared( vector3_cross( p1.m_vertex - p0.m_vertex, p2.m_vertex - p0.m_vertex ) ) > 1.0;
	};

	const double eps = .25;

	std::vector<const PatchControl*> ret;

	const auto find_triangle = [&ret, &patch, triangle_ok, eps]( const auto& check_func ){
		for( auto& iter : {
			PatchEdgeIter( patch, PatchEdgeIter::eRowBack, 0 ),
			PatchEdgeIter( patch, PatchEdgeIter::eRowForward, patch.getHeight() - 1 ),
			PatchEdgeIter( patch, PatchEdgeIter::eColBack, patch.getWidth() - 1 ),
			PatchEdgeIter( patch, PatchEdgeIter::eColForward, 0 ) } )
		{
			for( PatchEdgeIter i0 = iter; i0; i0 += 2 ){
				const PatchControl& p0 = *i0;
				if( check_func( p0 ) ){
					for( PatchEdgeIter i1 = i0 + size_t{ 2 }; i1; i1 += 2 ){
						const PatchControl& p1 = *i1;
						if( check_func( p1 )
						 && vector3_length_squared( p1.m_vertex - p0.m_vertex ) > eps ){
							for( PatchEdgeIter i2 = i0.getCrossIter() + size_t{ 1 }, i22 = i1.getCrossIter() + size_t{ 1 }; i2 && i22; ++i2, ++i22 ){
								for( const PatchControl& p2 : { *i2, *i22 } ){
									if( triangle_ok( p0, p1, p2 ) ){
										ret = { &p0, &p1, &p2 };
										return;
									}
								}
							}
						}
					}
				}
			}
		}
	};

	/* try patchControls-on-edge */
	for ( std::size_t i = w.numpoints - 1, j = 0; j < w.numpoints && ret.empty(); i = j, ++j )
	{
		const auto line_close = [eps, line = Line( w[i].vertex, w[j].vertex )]( const PatchControl& p ){
			return vector3_length_squared( line_closest_point( line, p.m_vertex ) - p.m_vertex ) < eps;
		};
		find_triangle( line_close );
	}

	/* try patchControls-on-edgeLine */
	for ( std::size_t i = w.numpoints - 1, j = 0; j < w.numpoints && ret.empty(); i = j, ++j )
	{
		const auto ray_close = [eps, ray = ray_for_points( w[i].vertex, w[j].vertex )]( const PatchControl& p ){
			return ray_squared_distance_to_point( ray, p.m_vertex ) < eps;
		};
		find_triangle( ray_close );
	}

	/* try patchControls-on-facePlane */
	if( ret.empty() ){
		const auto plane_close = [eps, plane]( const PatchControl& p ){
			return std::pow( plane3_distance_to_point( plane, p.m_vertex ), 2 ) < eps;
		};
		find_triangle( plane_close );
	}

	return ret;
}


void Face_getTexture( Face& face, CopiedString& shader, FaceTexture& clipboard ){
	shader = face.GetShader();

	face.GetTexdef( clipboard.m_projection );
	clipboard.m_flags = face.getShader().m_flags;

	clipboard.m_plane = face.getPlane().plane3();
	clipboard.m_winding = face.getWinding();
	clipboard.m_width = face.getShader().width();
	clipboard.m_height = face.getShader().height();

	clipboard.m_colour = face.getShader().state()->getTexture().color;

	clipboard.m_pasteSource = FaceTexture::eBrush;
}
typedef Function<void(Face&, CopiedString&, FaceTexture&), Face_getTexture> FaceGetTexture;

void Face_setTexture( Face& face, const char* shader, const FaceTexture& clipboard, EPasteMode mode, bool setShader ){
	if( setShader ){
		face.SetShader( shader );
		face.SetFlags( clipboard.m_flags );
	}
	if( mode == ePasteValues ){
		face.SetTexdef( clipboard.m_projection, false );
	}
	else if( mode == ePasteProject ){
		face.ProjectTexture( clipboard.m_projection, clipboard.m_plane.normal() );
	}
	else if( mode == ePasteSeamless ){
		if( clipboard.m_pasteSource == FaceTexture::eBrush ){
			DoubleRay line = plane3_intersect_plane3( clipboard.m_plane, face.getPlane().plane3() );
			if( vector3_length_squared( line.direction ) <= 1e-10 ){
				face.ProjectTexture( clipboard.m_projection, clipboard.m_plane.normal() );
				return;
			}

			const Quaternion rotation = quaternion_for_unit_vectors( clipboard.m_plane.normal(), face.getPlane().plane3().normal() );
//			globalOutputStream() << "rotation: " << rotation.x() << ' ' << rotation.y() << ' ' << rotation.z() << ' ' << rotation.w() << ' ' << '\n';
			Matrix4 transform = g_matrix4_identity;
			matrix4_pivoted_rotate_by_quaternion( transform, rotation, line.origin );

			TextureProjection proj = clipboard.m_projection;
			proj.m_brushprimit_texdef.addScale( clipboard.m_width, clipboard.m_height );
			Texdef_transformLocked( proj, clipboard.m_width, clipboard.m_height, clipboard.m_plane, transform, line.origin );
			proj.m_brushprimit_texdef.removeScale( clipboard.m_width, clipboard.m_height );

			face.SetTexdef( proj );

			CopiedString dummy;
			Face_getTexture( face, dummy, g_faceTextureClipboard );
		}
		else if( clipboard.m_pasteSource == FaceTexture::ePatch ){
			const auto pc = Patch_getClosestTriangle( clipboard.m_patch, face.getWinding(), face.getPlane().plane3() );
			// todo in patch->brush, brush->patch shall we apply texture, if alignment part fails?
			if( pc.empty() )
				return;
			DoubleVector3 vertices[3]{ pc[0]->m_vertex, pc[1]->m_vertex, pc[2]->m_vertex };
			const DoubleVector3 sts[3]{ DoubleVector3( pc[0]->m_texcoord ),
		                                DoubleVector3( pc[1]->m_texcoord ),
		                                DoubleVector3( pc[2]->m_texcoord ) };
			{ // rotate patch points to face plane
				const Plane3 plane = plane3_for_points( vertices );
				const DoubleRay line = plane3_intersect_plane3( face.getPlane().plane3(), plane );
				if( vector3_length_squared( line.direction ) > 1e-10 ){
					const Quaternion rotation = quaternion_for_unit_vectors( plane.normal(), face.getPlane().plane3().normal() );
					Matrix4 rot( g_matrix4_identity );
					matrix4_pivoted_rotate_by_quaternion( rot, rotation, line.origin );
					for( auto& v : vertices )
						matrix4_transform_point( rot, v );
				}
			}
			TextureProjection proj;
			Texdef_from_ST( proj, vertices, sts, clipboard.m_width, clipboard.m_height );
			proj.m_brushprimit_texdef.removeScale( clipboard.m_width, clipboard.m_height );
			face.SetTexdef( proj );

			CopiedString dummy;
			Face_getTexture( face, dummy, g_faceTextureClipboard );
		}

	}
}
typedef Function<void(Face&, const char*, const FaceTexture&, EPasteMode, bool), Face_setTexture> FaceSetTexture;


void Patch_getTexture( Patch& patch, CopiedString& shader, FaceTexture& clipboard ){
	shader = patch.GetShader();
	FaceTextureClipboard_setDefault();

	clipboard.m_width = patch.getShader()->getTexture().width;
	clipboard.m_height = patch.getShader()->getTexture().height;

	clipboard.m_colour = patch.getShader()->getTexture().color;

	clipboard.m_patch.copy( patch );

	clipboard.m_pasteSource = FaceTexture::ePatch;
}
typedef Function<void(Patch&, CopiedString&, FaceTexture&), Patch_getTexture> PatchGetTexture;

void Patch_setTexture( Patch& patch, const char* shader, const FaceTexture& clipboard, EPasteMode mode, bool setShader ){
	if( setShader )
		patch.SetShader( shader );
	if( mode == ePasteProject )
		patch.ProjectTexture( clipboard.m_projection, clipboard.m_plane.normal() );
	else if( mode == ePasteSeamless ){
		PatchData patchData;
		patchData.copy( patch );
		const auto pc = Patch_getClosestTriangle( patchData, clipboard.m_winding, clipboard.m_plane );

		if( pc.empty() )
			return;

		DoubleVector3 vertices[3]{ pc[0]->m_vertex, pc[1]->m_vertex, pc[2]->m_vertex };
		const DoubleVector3 sts[3]{ DoubleVector3( pc[0]->m_texcoord ),
		                            DoubleVector3( pc[1]->m_texcoord ),
		                            DoubleVector3( pc[2]->m_texcoord ) };
		Matrix4 local2tex0; // face tex projection
		{
			TextureProjection proj0( clipboard.m_projection );
			proj0.m_brushprimit_texdef.addScale( clipboard.m_width, clipboard.m_height );
			Texdef_Construct_local2tex( proj0, clipboard.m_width, clipboard.m_height, clipboard.m_plane.normal(), local2tex0 );
		}

		{ // rotate patch points to face plane
			const Plane3 plane = plane3_for_points( vertices );
			const DoubleRay line = plane3_intersect_plane3( clipboard.m_plane, plane );
			if( vector3_length_squared( line.direction ) > 1e-10 ){
				const Quaternion rotation = quaternion_for_unit_vectors( plane.normal(), clipboard.m_plane.normal() );
				Matrix4 rot( g_matrix4_identity );
				matrix4_pivoted_rotate_by_quaternion( rot, rotation, line.origin );
				for( auto& v : vertices )
					matrix4_transform_point( rot, v );
			}
		}

		Matrix4 local2tex; // patch BP tex projection
		Texdef_Construct_local2tex_from_ST( vertices, sts, local2tex );
		Matrix4 tex2local = matrix4_affine_inverse( local2tex );
		tex2local.t().vec3() += tex2local.z().vec3() * clipboard.m_plane.dist(); // adjust t() so that st->world points get to the plane

		const Matrix4 mat = matrix4_multiplied_by_matrix4( local2tex0, tex2local ); // unproject st->world, project to new st
		patch.undoSave();
		for( auto& p : patch ){
			p.m_texcoord = matrix4_transformed_point( mat, Vector3( p.m_texcoord ) ).vec2();
		}
		patch.controlPointsChanged();
		Patch_textureChanged();

		// Patch_getTexture
		g_faceTextureClipboard.m_width = patch.getShader()->getTexture().width;
		g_faceTextureClipboard.m_height = patch.getShader()->getTexture().height;
		g_faceTextureClipboard.m_colour = patch.getShader()->getTexture().color;
		g_faceTextureClipboard.m_patch.copy( patch );
		g_faceTextureClipboard.m_pasteSource = FaceTexture::ePatch;
	}
}
typedef Function<void(Patch&, const char*, const FaceTexture&, EPasteMode, bool), Patch_setTexture> PatchSetTexture;

#include "ientity.h"
void Light_getTexture( Entity& entity, CopiedString& shader, FaceTexture& clipboard ){
	string_parse_vector3( entity.getKeyValue( "_color" ), clipboard.m_colour );
	if( !string_parse_float( entity.getKeyValue( "_light" ), clipboard.m_light ) )
		string_parse_float( entity.getKeyValue( "light" ), clipboard.m_light );

	shader = TextureBrowser_GetSelectedShader(); // preserve shader
}
typedef Function<void(Entity&, CopiedString&, FaceTexture&), Light_getTexture> LightGetTexture;

void Light_setTexture( Entity& entity, const char* shader, const FaceTexture& clipboard, EPasteMode mode, bool setShader ){
	if( mode == ePasteSeamless || mode == ePasteProject ){
		char value[64];
		sprintf( value, "%g %g %g", clipboard.m_colour[0], clipboard.m_colour[1], clipboard.m_colour[2] );
		entity.setKeyValue( "_color", value );
	}
	if( mode == ePasteValues || mode == ePasteProject ){
		/* copypaste of write_intensity() from entity plugin */
		char value[64];
		sprintf( value, "%g", clipboard.m_light );
		if( entity.hasKeyValue( "_light" ) ) //primaryIntensity //if set
			entity.setKeyValue( "_light", value );
		else //secondaryIntensity
			entity.setKeyValue( "light", value ); //otherwise default to "light", which is understood by both q3 and q1
	}
}
typedef Function<void(Entity&, const char*, const FaceTexture&, EPasteMode, bool), Light_setTexture> LightSetTexture;


typedef Callback<void(CopiedString&, FaceTexture&)> GetTextureCallback;
typedef Callback<void(const char*, const FaceTexture&, EPasteMode, bool)> SetTextureCallback;

struct Texturable
{
	GetTextureCallback getTexture;
	SetTextureCallback setTexture;
};


void Face_getClosest( Face& face, SelectionTest& test, SelectionIntersection& bestIntersection, Texturable& texturable ){
	if ( face.isFiltered() ) {
		return;
	}
	SelectionIntersection intersection;
	face.testSelect( test, intersection );
	if ( intersection.valid()
	  && SelectionIntersection_closer( intersection, bestIntersection ) ) {
		bestIntersection = intersection;
		texturable.setTexture = makeCallback( FaceSetTexture(), face );
		texturable.getTexture = makeCallback( FaceGetTexture(), face );
	}
}


class OccludeSelector : public Selector
{
	SelectionIntersection& m_bestIntersection;
	bool& m_occluded;
public:
	OccludeSelector( SelectionIntersection& bestIntersection, bool& occluded ) : m_bestIntersection( bestIntersection ), m_occluded( occluded ){
		m_occluded = false;
	}
	void pushSelectable( Selectable& selectable ){
	}
	void popSelectable(){
	}
	void addIntersection( const SelectionIntersection& intersection ){
		if ( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
			m_bestIntersection = intersection;
			m_occluded = true;
		}
	}
};

class BrushGetClosestFaceVisibleWalker : public scene::Graph::Walker
{
	SelectionTest& m_test;
	Texturable& m_texturable;
	mutable SelectionIntersection m_bestIntersection;
public:
	BrushGetClosestFaceVisibleWalker( SelectionTest& test, Texturable& texturable ) : m_test( test ), m_texturable( texturable ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( !path.top().get().visible() )
			return false;
		BrushInstance* brush = Instance_getBrush( instance );
		if ( brush != 0 ) {
			m_test.BeginMesh( brush->localToWorld() );

			for ( Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i )
			{
				Face_getClosest( *( *i ), m_test, m_bestIntersection, m_texturable );
			}
		}
		else
		{
			SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
			if ( selectionTestable ) {
				bool occluded;
				OccludeSelector selector( m_bestIntersection, occluded );
				selectionTestable->testSelect( selector, m_test );
				if ( occluded ) {
					Patch* patch = Node_getPatch( path.top() );
					if ( patch != 0 ) {
						m_texturable.setTexture = makeCallback( PatchSetTexture(), *patch );
						m_texturable.getTexture = makeCallback( PatchGetTexture(), *patch );
						return true;
					}
					Entity* entity = Node_getEntity( path.top() );
					if( entity != 0 && string_equal_n( entity->getClassName(), "light", 5 ) ){
						m_texturable.setTexture = makeCallback( LightSetTexture(), *entity );
						m_texturable.getTexture = makeCallback( LightGetTexture(), *entity );
					}
					else{
						m_texturable = Texturable();
					}
				}
			}
		}
		return true;
	}
};

Texturable Scene_getClosestTexturable( scene::Graph& graph, SelectionTest& test ){
	Texturable texturable;
	graph.traverse( BrushGetClosestFaceVisibleWalker( test, texturable ) );
	return texturable;
}

bool Scene_getClosestTexture( scene::Graph& graph, SelectionTest& test, CopiedString& shader, FaceTexture& clipboard ){
	Texturable texturable = Scene_getClosestTexturable( graph, test );
	if ( texturable.getTexture != GetTextureCallback() ) {
		texturable.getTexture( shader, clipboard );
		return true;
	}
	return false;
}

void Scene_setClosestTexture( scene::Graph& graph, SelectionTest& test, const char* shader, const FaceTexture& clipboard, EPasteMode mode, bool setShader ){
	Texturable texturable = Scene_getClosestTexturable( graph, test );
	if ( texturable.setTexture != SetTextureCallback() ) {
		texturable.setTexture( shader, clipboard, mode, setShader );
	}
}

void TextureBrowser_SetSelectedShader( const char* shader );
const char* TextureBrowser_GetSelectedShader();

void Scene_copyClosestTexture( SelectionTest& test ){
	CopiedString shader;
	if ( Scene_getClosestTexture( GlobalSceneGraph(), test, shader, g_faceTextureClipboard ) ) {
		TextureBrowser_SetSelectedShader( shader.c_str() );
	}
}

const char* Scene_applyClosestTexture_getUndoName( bool shift, bool ctrl, bool alt ){
	const EPasteMode mode = pastemode_for_modifiers( shift, ctrl );
	const bool setShader = pastemode_if_setShader( mode, alt );
	switch ( mode )
	{
	default: //case ePasteNone:
		return "paintTexture";
	case ePasteValues:
		return setShader? "paintTexture,Values,LightPower" : "paintTexDefValues";
	case ePasteSeamless:
		return setShader? "paintTextureSeamless,LightColor" : "paintTexDefValuesSeamless";
	case ePasteProject:
		return setShader? "projectTexture,LightColor&Power" : "projectTexDefValues";
	}
}

void Scene_applyClosestTexture( SelectionTest& test, bool shift, bool ctrl, bool alt, bool texturize_selected = false ){
//	UndoableCommand command( "facePaintTexture" );

	const EPasteMode mode = pastemode_for_modifiers( shift, ctrl );
	const bool setShader = pastemode_if_setShader( mode, alt );

	if( texturize_selected ){
		if( setShader && mode != ePasteSeamless )
			Select_SetShader( TextureBrowser_GetSelectedShader() );
		if( mode == ePasteValues )
			Select_SetTexdef( g_faceTextureClipboard.m_projection, false, false );
		else if( mode == ePasteProject )
			Select_ProjectTexture( g_faceTextureClipboard.m_projection, g_faceTextureClipboard.m_plane.normal() );
	}

	Scene_setClosestTexture( GlobalSceneGraph(), test, TextureBrowser_GetSelectedShader(), g_faceTextureClipboard, mode, setShader );

	SceneChangeNotify();
}





void SelectedFaces_copyTexture(){
	if ( !g_SelectedFaceInstances.empty() ) {
		Face& face = g_SelectedFaceInstances.last().getFace();
		face.GetTexdef( g_faceTextureClipboard.m_projection );
		g_faceTextureClipboard.m_flags = face.getShader().m_flags;

		TextureBrowser_SetSelectedShader( face.getShader().getShader() );
	}
}

void FaceInstance_pasteTexture( FaceInstance& faceInstance ){
	faceInstance.getFace().SetTexdef( g_faceTextureClipboard.m_projection );
	faceInstance.getFace().SetShader( TextureBrowser_GetSelectedShader() );
	faceInstance.getFace().SetFlags( g_faceTextureClipboard.m_flags );
	SceneChangeNotify();
}

bool SelectedFaces_empty(){
	return g_SelectedFaceInstances.empty();
}

void SelectedFaces_pasteTexture(){
	UndoableCommand command( "facePasteTexture" );
	g_SelectedFaceInstances.foreach( FaceInstance_pasteTexture );
}



void SurfaceInspector_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Surface Inspector Increments Match Grid", g_si_globals.m_bSnapTToGrid );
}
void SurfaceInspector_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Surface Inspector", "Surface Inspector Preferences" ) );
	SurfaceInspector_constructPreferences( page );
}
void SurfaceInspector_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( SurfaceInspector_constructPage ) );
}

void SurfaceInspector_registerCommands(){
	GlobalCommands_insert( "TextureReset/Cap", makeCallbackF( SurfaceInspector_ResetTexture ), QKeySequence( "Shift+N" ) );
	GlobalCommands_insert( "FitTexture", makeCallbackF( SurfaceInspector_FitTexture ), QKeySequence( "Ctrl+F" ) );
	GlobalCommands_insert( "FitTextureWidth", makeCallbackF( SurfaceInspector_FaceFitWidth ) );
	GlobalCommands_insert( "FitTextureHeight", makeCallbackF( SurfaceInspector_FaceFitHeight ) );
	GlobalCommands_insert( "FitTextureWidthOnly", makeCallbackF( SurfaceInspector_FaceFitWidthOnly ) );
	GlobalCommands_insert( "FitTextureHeightOnly", makeCallbackF( SurfaceInspector_FaceFitHeightOnly ) );
	GlobalCommands_insert( "TextureProjectAxial", makeCallbackF( SurfaceInspector_ProjectTexture_eProjectAxial ) );
	GlobalCommands_insert( "TextureProjectOrtho", makeCallbackF( SurfaceInspector_ProjectTexture_eProjectOrtho ) );
	GlobalCommands_insert( "TextureProjectCam", makeCallbackF( SurfaceInspector_ProjectTexture_eProjectCam ) );
	GlobalCommands_insert( "SurfaceInspector", makeCallbackF( SurfaceInspector_toggleShown ), QKeySequence( "S" ) );

//	GlobalCommands_insert( "FaceCopyTexture", makeCallbackF( SelectedFaces_copyTexture ) );
//	GlobalCommands_insert( "FacePasteTexture", makeCallbackF( SelectedFaces_pasteTexture ) );
}


#include "preferencesystem.h"


void SurfaceInspector_Construct(){
	g_SurfaceInspector = new SurfaceInspector;

	SurfaceInspector_registerCommands();

	FaceTextureClipboard_setDefault();

	GlobalPreferenceSystem().registerPreference( "SI_SurfaceTexdef_Scale1", FloatImportStringCaller( g_si_globals.scale[0] ), FloatExportStringCaller( g_si_globals.scale[0] ) );
	GlobalPreferenceSystem().registerPreference( "SI_SurfaceTexdef_Scale2", FloatImportStringCaller( g_si_globals.scale[1] ), FloatExportStringCaller( g_si_globals.scale[1] ) );
	GlobalPreferenceSystem().registerPreference( "SI_SurfaceTexdef_Shift1", FloatImportStringCaller( g_si_globals.shift[0] ), FloatExportStringCaller( g_si_globals.shift[0] ) );
	GlobalPreferenceSystem().registerPreference( "SI_SurfaceTexdef_Shift2", FloatImportStringCaller( g_si_globals.shift[1] ), FloatExportStringCaller( g_si_globals.shift[1] ) );
	GlobalPreferenceSystem().registerPreference( "SI_SurfaceTexdef_Rotate", FloatImportStringCaller( g_si_globals.rotate ), FloatExportStringCaller( g_si_globals.rotate ) );
	GlobalPreferenceSystem().registerPreference( "SnapTToGrid", BoolImportStringCaller( g_si_globals.m_bSnapTToGrid ), BoolExportStringCaller( g_si_globals.m_bSnapTToGrid ) );

	typedef FreeCaller<void(const Selectable&), SurfaceInspector_SelectionChanged> SurfaceInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( SurfaceInspectorSelectionChangedCaller() );
	typedef FreeCaller<void(), SurfaceInspector_updateSelection> SurfaceInspectorUpdateSelectionCaller;
	Brush_addTextureChangedCallback( SurfaceInspectorUpdateSelectionCaller() );
	Patch_addTextureChangedCallback( SurfaceInspectorUpdateSelectionCaller() );

	SurfaceInspector_registerPreferencesPage();
}
void SurfaceInspector_Destroy(){
	delete g_SurfaceInspector;
}

