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

#include <list>

#include "generic/callback.h"
#include "gtkutil/dialog.h"
#include "generic/callback.h"
#include "string/string.h"

inline void BoolImport( bool& self, bool value ){
	self = value;
}
typedef ReferenceCaller<bool, void(bool), BoolImport> BoolImportCaller;

inline void BoolExport( bool& self, const BoolImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller<bool, void(const BoolImportCallback&), BoolExport> BoolExportCaller;


inline void IntImport( int& self, int value ){
	self = value;
}
typedef ReferenceCaller<int, void(int), IntImport> IntImportCaller;

inline void IntExport( int& self, const IntImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller<int, void(const IntImportCallback&), IntExport> IntExportCaller;


inline void SizeImport( std::size_t& self, std::size_t value ){
	self = value;
}
typedef ReferenceCaller<std::size_t, void(std::size_t), SizeImport> SizeImportCaller;

inline void SizeExport( std::size_t& self, const SizeImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller<std::size_t, void(const SizeImportCallback&), SizeExport> SizeExportCaller;


inline void FloatImport( float& self, float value ){
	self = value;
}
typedef ReferenceCaller<float, void(float), FloatImport> FloatImportCaller;

inline void FloatExport( float& self, const FloatImportCallback& importCallback ){
	importCallback( self );
}
typedef ReferenceCaller<float, void(const FloatImportCallback&), FloatExport> FloatExportCaller;


inline void StringImport( CopiedString& self, const char* value ){
	self = value;
}
typedef ReferenceCaller<CopiedString, void(const char*), StringImport> StringImportCaller;
inline void StringExport( CopiedString& self, const StringImportCallback& importCallback ){
	importCallback( self.c_str() );
}
typedef ReferenceCaller<CopiedString, void(const StringImportCallback&), StringExport> StringExportCaller;


struct DLG_DATA
{
	virtual void release() = 0;
	virtual void importData() const = 0;
	virtual void exportData() const = 0;
};

template<typename FirstArgument>
class CallbackDialogData;

typedef std::list<DLG_DATA*> DialogDataList;

#include <QDialog>
class QGridLayout;
class QCheckBox;
class QComboBox;
class QSlider;
class QButtonGroup;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

class Dialog
{
	QDialog* m_window{};
	DialogDataList m_data;
public:
	virtual ~Dialog();

	/*!
	   start modal dialog box
	   you need to use AddModalButton to select eIDOK eIDCANCEL buttons
	 */
	QDialog::DialogCode DoModal();
	void EndModal( QDialog::DialogCode code );
	virtual void BuildDialog() = 0;
	virtual void exportData();
	virtual void importData();
	virtual void PreModal() { };
	virtual void PostModal( QDialog::DialogCode code ) { };
	virtual void ShowDlg();
	virtual void HideDlg();
	void Create( QWidget *parent );
	void Destroy();
	QDialog* GetWidget(){
		return m_window;
	}
	const QDialog* GetWidget() const {
		return m_window;
	}

	QCheckBox* addCheckBox( QGridLayout *grid, const char* name, const char* flag, const BoolImportCallback& importCallback, const BoolExportCallback& exportCallback );
	QCheckBox* addCheckBox( QGridLayout *grid, const char* name, const char* flag, bool& data );
	QComboBox* addCombo( QGridLayout *grid, const char* name, StringArrayRange values, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	QComboBox* addCombo( QGridLayout *grid, const char* name, int& data, StringArrayRange values );
	void addSlider( QGridLayout *grid, const char* name, int& data, int lower, int upper, int step_increment, int page_increment );
	void addSlider( QGridLayout *grid, const char* name, float& data, double lower, double upper, double step_increment, double page_increment );
	void addRadio( QGridLayout *grid, const char* name, StringArrayRange names, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	void addRadio( QGridLayout *grid, const char* name, int& data, StringArrayRange names );
	void addRadioIcons( QGridLayout *grid, const char* name, StringArrayRange icons, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	void addRadioIcons( QGridLayout *grid, const char* name, int& data, StringArrayRange icons );
	QWidget* addTextEntry( QGridLayout *grid, const char* name, const StringImportCallback& importCallback, const StringExportCallback& exportCallback );
	QWidget* addEntry( QGridLayout *grid, const char* name, CopiedString& data ){
		return addTextEntry( grid, name, StringImportCallback( StringImportCaller( data ) ), StringExportCallback( StringExportCaller( data ) ) );
	}
	void addPathEntry( QGridLayout *grid, const char* name, bool browse_directory, const StringImportCallback& importCallback, const StringExportCallback& exportCallback );
	void addPathEntry( QGridLayout *grid, const char* name, CopiedString& data, bool directory );
	QWidget* addSpinner( QGridLayout *grid, const char* name, int& data, int lower, int upper );
	QWidget* addSpinner( QGridLayout *grid, const char* name, int lower, int upper, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	QWidget* addSpinner( QGridLayout *grid, const char* name, double lower, double upper, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback, int decimals );
	QWidget* addSpinner( QGridLayout* grid, const char* name, float& data, double lower, double upper, int decimals );

protected:

	void AddBoolToggleData( QCheckBox& object, const BoolImportCallback& importCallback, const BoolExportCallback& exportCallback );
	void AddIntRadioData( QButtonGroup& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	void AddTextEntryData( QLineEdit& object, const StringImportCallback& importCallback, const StringExportCallback& exportCallback );
	void AddFloatSpinnerData( QDoubleSpinBox& object, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );
	void AddIntSpinnerData( QSpinBox& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	void AddIntSliderData( QSlider& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );
	void AddFloatSliderData( QSlider& object, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback );
	void AddIntComboData( QComboBox& object, const IntImportCallback& importCallback, const IntExportCallback& exportCallback );

	void AddDialogData( QCheckBox& object, bool& data );
	void AddDialogData( QButtonGroup& object, int& data );
	void AddDialogData( QLineEdit& object, CopiedString& data );
	void AddDialogData( QDoubleSpinBox& object, float& data );
	void AddDialogData( QSpinBox& object, int& data );
	void AddDialogData( QSlider& object, int& data );
	void AddDialogData( QSlider& object, float& data );
	void AddDialogData( QComboBox& object, int& data );
};
