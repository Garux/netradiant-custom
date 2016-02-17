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

#if !defined( INCLUDED_DIALOG_H )
#define INCLUDED_DIALOG_H

#include <list>
#include <uilib/uilib.h>

#include "gtkutil/dialog.h"
#include "generic/callback.h"
#include "string/string.h"

template<typename Environment, typename FirstArgument, void( *func ) ( Environment&, FirstArgument )>
class ReferenceCaller1;

inline void BoolImport( bool& self, bool value ){
	self = value;
}
typedef ReferenceCaller1<bool, bool, BoolImport> BoolImportCaller;

inline void BoolExport( bool& self, const BoolImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller1<bool, const BoolImportCallback&, BoolExport> BoolExportCaller;


inline void IntImport( int& self, int value ){
	self = value;
}
typedef ReferenceCaller1<int, int, IntImport> IntImportCaller;

inline void IntExport( int& self, const IntImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller1<int, const IntImportCallback&, IntExport> IntExportCaller;


inline void SizeImport( std::size_t& self, std::size_t value ){
	self = value;
}
typedef ReferenceCaller1<std::size_t, std::size_t, SizeImport> SizeImportCaller;

inline void SizeExport( std::size_t& self, const SizeImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller1<std::size_t, const SizeImportCallback&, SizeExport> SizeExportCaller;


inline void FloatImport( float& self, float value ){
	self = value;
}
typedef ReferenceCaller1<float, float, FloatImport> FloatImportCaller;

inline void FloatExport( float& self, const FloatImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller1<float, const FloatImportCallback&, FloatExport> FloatExportCaller;


inline void StringImport( std::string& self, const char* value ){
	self = value;
}
typedef ReferenceCaller1<std::string, const char*, StringImport> StringImportCaller;
inline void StringExport( std::string& self, const StringImportCallback& importCallback ){
	importCallback( self.c_str() );
}
typedef ReferenceCaller1<std::string, const StringImportCallback&, StringExport> StringExportCaller;


class DLG_DATA
{
public:
	virtual ~DLG_DATA(){}
	virtual void release() = 0;
	virtual void importData() const = 0;
	virtual void exportData() const = 0;
};

typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkRadioButton GtkRadioButton;
typedef struct _GtkSpinButton GtkSpinButton;
typedef struct _GtkComboBox GtkComboBox;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkAdjustment GtkAdjustment;

template<typename FirstArgument>
class CallbackDialogData;

typedef std::list<DLG_DATA*> DialogDataList;

class Dialog
{
ui::Window m_window;
DialogDataList m_data;
public:
ModalDialog m_modal;
ui::Window m_parent;

Dialog();
virtual ~Dialog();

/*!
   start modal dialog box
   you need to use AddModalButton to select eIDOK eIDCANCEL buttons
 */
EMessageBoxReturn DoModal();
void EndModal( EMessageBoxReturn code );
virtual ui::Window BuildDialog() = 0;
virtual void exportData();
virtual void importData();
virtual void PreModal() { };
virtual void PostModal( EMessageBoxReturn code ) { };
virtual void ShowDlg();
virtual void HideDlg();
void Create();
void Destroy();
ui::Window GetWidget(){
	return m_window;
}
const ui::Window GetWidget() const {
	return m_window;
}

ui::CheckButton addCheckBox( ui::Widget vbox, const char* name, const char* flag, const BoolImportCallback& importCallback, const BoolExportCallback& exportCallback );
ui::CheckButton addCheckBox( ui::Widget vbox, const char* name, const char* flag, bool& data );
void addCombo( ui::Widget vbox, const char* name, StringArrayRange values, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void addCombo( ui::Widget vbox, const char* name, int& data, StringArrayRange values );
void addSlider( ui::Widget vbox, const char* name, int& data, gboolean draw_value, const char* low, const char* high, double value, double lower, double upper, double step_increment, double page_increment );
void addRadio( ui::Widget vbox, const char* name, StringArrayRange names, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void addRadio( ui::Widget vbox, const char* name, int& data, StringArrayRange names );
void addRadioIcons( ui::Widget vbox, const char* name, StringArrayRange icons, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void addRadioIcons( ui::Widget vbox, const char* name, int& data, StringArrayRange icons );
ui::Widget addIntEntry( ui::Widget vbox, const char* name, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
ui::Widget addEntry( ui::Widget vbox, const char* name, int& data ){
	return addIntEntry( vbox, name, IntImportCaller( data ), IntExportCaller( data ) );
}
ui::Widget addSizeEntry( ui::Widget vbox, const char* name, const SizeImportCallback& importCallback, const SizeExportCallback& exportCallback );
ui::Widget addEntry( ui::Widget vbox, const char* name, std::size_t& data ){
	return addSizeEntry( vbox, name, SizeImportCaller( data ), SizeExportCaller( data ) );
}
ui::Widget addFloatEntry( ui::Widget vbox, const char* name, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );
ui::Widget addEntry( ui::Widget vbox, const char* name, float& data ){
	return addFloatEntry( vbox, name, FloatImportCaller( data ), FloatExportCaller( data ) );
}
ui::Widget addPathEntry( ui::Widget vbox, const char* name, bool browse_directory, const StringImportCallback& importCallback, const StringExportCallback& exportCallback );
ui::Widget addPathEntry( ui::Widget vbox, const char* name, std::string& data, bool directory );
ui::SpinButton addSpinner( ui::Widget vbox, const char* name, int& data, double value, double lower, double upper );
ui::SpinButton addSpinner( ui::Widget vbox, const char* name, double value, double lower, double upper, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
ui::SpinButton addSpinner( ui::Widget vbox, const char* name, double value, double lower, double upper, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );

protected:

void AddBoolToggleData( GtkToggleButton& object, const BoolImportCallback& importCallback, const BoolExportCallback& exportCallback );
void AddIntRadioData( GtkRadioButton& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void AddTextEntryData( GtkEntry& object, const StringImportCallback& importCallback, const StringExportCallback& exportCallback );
void AddIntEntryData( GtkEntry& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void AddSizeEntryData( GtkEntry& object, const SizeImportCallback& importCallback, const SizeExportCallback& exportCallback );
void AddFloatEntryData( GtkEntry& object, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );
void AddFloatSpinnerData( GtkSpinButton& object, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );
void AddIntSpinnerData( GtkSpinButton& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void AddIntAdjustmentData( GtkAdjustment& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
void AddIntComboData( GtkComboBox& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );

void AddDialogData( GtkToggleButton& object, bool& data );
void AddDialogData( GtkRadioButton& object, int& data );
void AddDialogData( GtkEntry& object, std::string& data );
void AddDialogData( GtkEntry& object, int& data );
void AddDialogData( GtkEntry& object, std::size_t& data );
void AddDialogData( GtkEntry& object, float& data );
void AddDialogData( GtkSpinButton& object, float& data );
void AddDialogData( GtkSpinButton& object, int& data );
void AddDialogData( GtkAdjustment& object, int& data );
void AddDialogData( GtkComboBox& object, int& data );
};

#endif
