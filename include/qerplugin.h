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

// QERadiant PlugIns
//
//

#pragma once

#include "generic/constant.h"
#include "generic/vector.h"


// ========================================
// GTK+ helper functions

// NOTE: parent can be 0 in all functions but it's best to set them

// this API does not depend on gtk+ or glib
class QWidget;
class QString;

enum class EMessageBoxType
{
	Info,
	Question,	/* eIDYES | eIDNO */
	Warning,
	Error,
};

enum EMessageBoxReturn
{
	eIDOK = 1,
	eIDCANCEL = 2,
	eIDYES = 4,
	eIDNO = 8,
};

// simple Message Box, see above for the 'type' flags
//! \p buttons is combination of \enum EMessageBoxReturn flags or 0 for buttons, respecting \enum class EMessageBoxType
typedef EMessageBoxReturn ( *PFN_QERAPP_MESSAGEBOX )( QWidget *parent, const char* text, const char* caption /* = "NetRadiant"*/, EMessageBoxType type /* = Info*/, int buttons /* = 0*/ );

// file and directory selection functions return null if the user hits cancel
// - 'title' is the dialog title (can be null)
// - 'path' is used to set the initial directory (can be null)
// - 'pattern': the first pattern is for the win32 mode, then comes the Gtk pattern list, see Radiant source for samples
typedef const char* ( *PFN_QERAPP_FILEDIALOG )( QWidget *parent, bool open, const char* title, const char* path /* = 0*/, const char* pattern /* = 0*/, bool want_load /* = false*/, bool want_import /* = false*/, bool want_save /* = false*/ );

typedef QString ( *PFN_QERAPP_DIRDIALOG )( QWidget *parent, const QString& path /* = {} */ );

// return true if the user closed the dialog with 'Ok'
// 'color' is used to set the initial value and store the selected value
typedef bool ( *PFN_QERAPP_COLORDIALOG )( QWidget *parent, Vector3& color,
                                          const char* title /* = "Choose Color"*/ );

// load an image file and create QIcon from it
// NOTE: 'filename' is relative to <radiant_path>/plugins/bitmaps/
class QIcon;
typedef QIcon ( *PFN_QERAPP_NEWICON )( const char* filename );

// ========================================

namespace scene
{
class Node;
}

class ModuleObserver;

#include "signal/signalfwd.h"
#include "windowobserver.h"

typedef SignalHandler3<const WindowVector&, ButtonIdentifier, ModifierFlags> MouseEventHandler;
typedef SignalFwd<MouseEventHandler>::handler_id_type MouseEventHandlerId;

enum VIEWTYPE
{
	YZ = 0,
	XZ = 1,
	XY = 2
};

#define NDIM1NDIM2( viewtype ) const int nDim1 = ( viewtype == YZ ) ? 1 : 0, \
										nDim2 = ( viewtype == XY ) ? 1 : 2;

// the radiant core API
struct _QERFuncTable_1
{
	INTEGER_CONSTANT( Version, 1 );
	STRING_CONSTANT( Name, "radiant" );

	const char* ( *getEnginePath )( );
	const char* ( *getLocalRcPath )( );
	const char* ( *getGameToolsPath )( );
	const char* ( *getAppPath )( );
	const char* ( *getSettingsPath )( );
	const char* ( *getMapsPath )( );

	const char* ( *getGameName )( );
	const char* ( *getGameMode )( );

	const char* ( *getMapName )( );
	scene::Node& ( *getMapWorldEntity )( );
	float ( *getGridSize )();

	const char* ( *getGameDescriptionKeyValue )( const char* key );
	const char* ( *getRequiredGameDescriptionKeyValue )( const char* key );

	void ( *attachGameToolsPathObserver )( ModuleObserver& observer );
	void ( *detachGameToolsPathObserver )( ModuleObserver& observer );
	void ( *attachEnginePathObserver )( ModuleObserver& observer );
	void ( *detachEnginePathObserver )( ModuleObserver& observer );
	void ( *attachGameNameObserver )( ModuleObserver& observer );
	void ( *detachGameNameObserver )( ModuleObserver& observer );
	void ( *attachGameModeObserver )( ModuleObserver& observer );
	void ( *detachGameModeObserver )( ModuleObserver& observer );

	SignalHandlerId ( *XYWindowDestroyed_connect )( const SignalHandler& handler );
	void ( *XYWindowDestroyed_disconnect )( SignalHandlerId id );
	MouseEventHandlerId ( *XYWindowMouseDown_connect )( const MouseEventHandler& handler );
	void ( *XYWindowMouseDown_disconnect )( MouseEventHandlerId id );
	VIEWTYPE ( *XYWindow_getViewType )();
	Vector3 ( *XYWindow_windowToWorld )( const WindowVector& position );

	Vector3 ( *Camera_getOrigin )();

	const char* ( *TextureBrowser_getSelectedShader )( );

	// Qt functions
	PFN_QERAPP_MESSAGEBOX m_pfnMessageBox;
	PFN_QERAPP_FILEDIALOG m_pfnFileDialog;
	PFN_QERAPP_DIRDIALOG m_pfnDirDialog;
	PFN_QERAPP_COLORDIALOG m_pfnColorDialog;
	PFN_QERAPP_NEWICON m_pfnNewIcon;

};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<_QERFuncTable_1> GlobalRadiantModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<_QERFuncTable_1> GlobalRadiantModuleRef;

inline _QERFuncTable_1& GlobalRadiant(){
	return GlobalRadiantModule::getTable();
}
