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
// Base dialog class, provides a way to run modal dialogs and
// set/get the widget values in member variables.
//
// Leonardo Zide (leo@lokigames.com)
//

#include "dialog.h"

#include "debugging/debugging.h"


#include "mainframe.h"

#include <cstdlib>

#include "stream/stringstream.h"
#include "gtkutil/dialog.h"
#include "gtkutil/entry.h"
#include "gtkutil/image.h"
#include "gtkutil/spinbox.h"

#include "gtkmisc.h"

#include <QCheckBox>
#include "gtkutil/combobox.h"
#include <QSlider>
#include <QRadioButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QString>


struct DialogSliderRow
{
	QSlider *m_slider;
	QHBoxLayout *m_layout;
	DialogSliderRow( int lower, int upper, int step_increment, int page_increment ){
		m_slider = new QSlider( Qt::Orientation::Horizontal );
		m_slider->setRange( lower, upper );
		m_slider->setSingleStep( step_increment );
		m_slider->setPageStep( page_increment );

		auto label = new QLabel;
		label->setMinimumWidth( label->fontMetrics().horizontalAdvance( QString::number( upper ) ) );
		QObject::connect( m_slider, &QSlider::valueChanged,
			[label]( int value ){
				label->setText( QString::number( value ) );
			} );

		m_layout = new QHBoxLayout;
		m_layout->addWidget( label );
		m_layout->addWidget( m_slider );
	}

	DialogSliderRow( double lower, double upper, double step_increment, double page_increment ){
		m_slider = new QSlider( Qt::Orientation::Horizontal );
		m_slider->setRange( lower * 10, upper * 10 );
		m_slider->setSingleStep( step_increment * 10 );
		m_slider->setPageStep( page_increment * 10 );

		auto label = new QLabel;
		label->setMinimumWidth( label->fontMetrics().horizontalAdvance( QString::number( upper, 'f', 1 ) ) );
		QObject::connect( m_slider, &QSlider::valueChanged,
			[label]( int value ){
				label->setText( QString::number( value / 10.0, 'f', 1 ) );
			} );

		m_layout = new QHBoxLayout;
		m_layout->addWidget( label );
		m_layout->addWidget( m_slider );
	}
};


template<
    typename Type_,
    typename Other_,
    void( *Import ) ( Type_&, Other_ ),
    void( *Export ) ( Type_&, const Callback<void(Other_)>& )
    >
class ImportExport
{
public:
	typedef Type_ Type;
	typedef Other_ Other;

	typedef ReferenceCaller<Type, void(Other), Import> ImportCaller;
	typedef ReferenceCaller<Type, void(const Callback<void(Other)>&), Export> ExportCaller;
};

typedef ImportExport<bool, bool, BoolImport, BoolExport> BoolImportExport;
typedef ImportExport<int, int, IntImport, IntExport> IntImportExport;
typedef ImportExport<std::size_t, std::size_t, SizeImport, SizeExport> SizeImportExport;
typedef ImportExport<float, float, FloatImport, FloatExport> FloatImportExport;
typedef ImportExport<CopiedString, const char*, StringImport, StringExport> StringImportExport;



void BoolToggleImport( QCheckBox& widget, bool value ){
	widget.setChecked( value );
}
void BoolToggleExport( QCheckBox& widget, const BoolImportCallback& importCallback ){
	importCallback( widget.isChecked() );
}
typedef ImportExport<QCheckBox, bool, BoolToggleImport, BoolToggleExport> BoolToggleImportExport;


void IntRadioImport( QButtonGroup& widget, int index ){
	widget.button( index )->setChecked( true );
}
void IntRadioExport( QButtonGroup& widget, const IntImportCallback& importCallback ){
	importCallback( widget.checkedId() );
}
typedef ImportExport<QButtonGroup, int, IntRadioImport, IntRadioExport> IntRadioImportExport;


void TextEntryImport( QLineEdit& widget, const char* text ){
	widget.setText( text );
}
void TextEntryExport( QLineEdit& widget, const StringImportCallback& importCallback ){
	importCallback( widget.text().toLatin1().constData() );
}
typedef ImportExport<QLineEdit, const char*, TextEntryImport, TextEntryExport> TextEntryImportExport;


void FloatSpinnerImport( QDoubleSpinBox& widget, float value ){
	widget.setValue( value );
}
void FloatSpinnerExport( QDoubleSpinBox& widget, const FloatImportCallback& importCallback ){
	importCallback( widget.value() );
}
typedef ImportExport<QDoubleSpinBox, float, FloatSpinnerImport, FloatSpinnerExport> FloatSpinnerImportExport;


void IntSpinnerImport( QSpinBox& widget, int value ){
	widget.setValue( value );
}
void IntSpinnerExport( QSpinBox& widget, const IntImportCallback& importCallback ){
	importCallback( widget.value() );
}
typedef ImportExport<QSpinBox, int, IntSpinnerImport, IntSpinnerExport> IntSpinnerImportExport;


void IntSliderImport( QSlider& widget, int value ){
	widget.setValue( value );
}
void IntSliderExport( QSlider& widget, const IntImportCallback& importCallback ){
	importCallback( widget.value() );
}
typedef ImportExport<QSlider, int, IntSliderImport, IntSliderExport> IntSliderImportExport;

// QSlider operates on int values only, so using 10x range for floats
void FloatSliderImport( QSlider& widget, float value ){
	widget.setValue( value * 10.0 );
}
void FloatSliderExport( QSlider& widget, const FloatImportCallback& importCallback ){
	importCallback( widget.value() / 10.0 );
}
typedef ImportExport<QSlider, float, FloatSliderImport, FloatSliderExport> FloatSliderImportExport;


void IntComboImport( QComboBox& widget, int value ){
	widget.setCurrentIndex( value );
}
void IntComboExport( QComboBox& widget, const IntImportCallback& importCallback ){
	importCallback( widget.currentIndex() );
}
typedef ImportExport<QComboBox, int, IntComboImport, IntComboExport> IntComboImportExport;


template<typename FirstArgument>
class CallbackDialogData final : public DLG_DATA
{
public:
	typedef Callback<void(FirstArgument)> ImportCallback;
	typedef Callback<void(const ImportCallback&)> ExportCallback;

private:
	ImportCallback m_importWidget;
	ExportCallback m_exportWidget;
	ImportCallback m_importViewer;
	ExportCallback m_exportViewer;

public:
	CallbackDialogData( const ImportCallback& importWidget, const ExportCallback& exportWidget, const ImportCallback& importViewer, const ExportCallback& exportViewer )
		: m_importWidget( importWidget ), m_exportWidget( exportWidget ), m_importViewer( importViewer ), m_exportViewer( exportViewer ){
	}
	void release(){
		delete this;
	}
	void importData() const {
		m_exportViewer( m_importWidget );
	}
	void exportData() const {
		m_exportWidget( m_importViewer );
	}
};

template<typename Widget, typename Viewer>
class AddData
{
	DialogDataList& m_data;
public:
	AddData( DialogDataList& data ) : m_data( data ){
	}
	void apply( typename Widget::Type& widget, typename Viewer::Type& viewer ) const {
		m_data.push_back(
		    new CallbackDialogData<typename Widget::Other>(
		        typename Widget::ImportCaller( widget ),
		        typename Widget::ExportCaller( widget ),
		        typename Viewer::ImportCaller( viewer ),
		        typename Viewer::ExportCaller( viewer )
		    )
		);
	}
};

template<typename Widget>
class AddCustomData
{
	DialogDataList& m_data;
public:
	AddCustomData( DialogDataList& data ) : m_data( data ){
	}
	void apply(
	    typename Widget::Type& widget,
	    const Callback<void(typename Widget::Other)>& importViewer,
	    const Callback<void(const Callback<void(typename Widget::Other)>&)>& exportViewer
	) const {
		m_data.push_back(
		    new CallbackDialogData<typename Widget::Other>(
		        typename Widget::ImportCaller( widget ),
		        typename Widget::ExportCaller( widget ),
		        importViewer,
		        exportViewer
		    )
		);
	}
};

// =============================================================================
// Dialog class

Dialog::~Dialog(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->release();
	}

	ASSERT_MESSAGE( m_window == 0, "dialog window not destroyed" );
}

void Dialog::ShowDlg(){
	ASSERT_MESSAGE( m_window != 0, "dialog was not constructed" );
	importData();
	m_window->show();
	m_window->raise();
	m_window->activateWindow();
}

void Dialog::HideDlg(){
	ASSERT_MESSAGE( m_window != 0, "dialog was not constructed" );
	exportData();
	m_window->hide();
}

void Dialog::Create( QWidget *parent ){
	ASSERT_MESSAGE( m_window == 0, "dialog cannot be constructed" );

	m_window = new QDialog( parent, Qt::Dialog | Qt::WindowCloseButtonHint );
	BuildDialog();
}

void Dialog::Destroy(){
	ASSERT_MESSAGE( m_window != 0, "dialog cannot be destroyed" );

	delete m_window;
	m_window = 0;
}


void Dialog::AddBoolToggleData( QCheckBox& widget, const BoolImportCallback& importViewer, const BoolExportCallback& exportViewer ){
	AddCustomData<BoolToggleImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddIntRadioData( QButtonGroup& widget, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	AddCustomData<IntRadioImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddTextEntryData( QLineEdit& widget, const StringImportCallback& importViewer, const StringExportCallback& exportViewer ){
	AddCustomData<TextEntryImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddFloatSpinnerData( QDoubleSpinBox& widget, const FloatImportCallback& importViewer, const FloatExportCallback& exportViewer ){
	AddCustomData<FloatSpinnerImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddIntSpinnerData( QSpinBox& widget, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	AddCustomData<IntSpinnerImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddIntSliderData( QSlider& widget, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	AddCustomData<IntSliderImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddFloatSliderData( QSlider& widget, const FloatImportCallback& importViewer, const FloatExportCallback& exportViewer ){
	AddCustomData<FloatSliderImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}

void Dialog::AddIntComboData( QComboBox& widget, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	AddCustomData<IntComboImportExport>( m_data ).apply( widget, importViewer, exportViewer );
}


void Dialog::AddDialogData( QCheckBox& widget, bool& data ){
	AddData<BoolToggleImportExport, BoolImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QButtonGroup& widget, int& data ){
	AddData<IntRadioImportExport, IntImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QLineEdit& widget, CopiedString& data ){
	AddData<TextEntryImportExport, StringImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QDoubleSpinBox& widget, float& data ){
	AddData<FloatSpinnerImportExport, FloatImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QSpinBox& widget, int& data ){
	AddData<IntSpinnerImportExport, IntImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QSlider& widget, int& data ){
	AddData<IntSliderImportExport, IntImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QSlider& widget, float& data ){
	AddData<FloatSliderImportExport, FloatImportExport>( m_data ).apply( widget, data );
}
void Dialog::AddDialogData( QComboBox& widget, int& data ){
	AddData<IntComboImportExport, IntImportExport>( m_data ).apply( widget, data );
}

void Dialog::exportData(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->exportData();
	}
}

void Dialog::importData(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->importData();
	}
}

void Dialog::EndModal( QDialog::DialogCode code ){
	m_window->done( code );
}

QDialog::DialogCode Dialog::DoModal(){
	importData();

	PreModal();

	ASSERT_NOTNULL( m_window );
	const QDialog::DialogCode ret = static_cast<QDialog::DialogCode>( m_window->exec() );
	if ( ret == QDialog::DialogCode::Accepted ) {
		exportData();
	}

	m_window->hide();

	PostModal( ret );

	return ret;
}


QCheckBox* Dialog::addCheckBox( QGridLayout* grid, const char* name, const char* flag, const BoolImportCallback& importViewer, const BoolExportCallback& exportViewer ){
	auto check = new QCheckBox( flag );
	AddBoolToggleData( *check, importViewer, exportViewer );
	DialogGrid_packRow( grid, check, name );
	return check;
}

QCheckBox* Dialog::addCheckBox( QGridLayout* grid, const char* name, const char* flag, bool& data ){
	return addCheckBox( grid, name, flag, BoolImportCaller( data ), BoolExportCaller( data ) );
}

QComboBox* Dialog::addCombo( QGridLayout* grid, const char* name, StringArrayRange values, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	auto combo = new ComboBox;

	for ( const char *value : values )
		combo->addItem( value );

	AddIntComboData( *combo, importViewer, exportViewer );

	DialogGrid_packRow( grid, combo, name );

	return combo;
}

QComboBox* Dialog::addCombo( QGridLayout* grid, const char* name, int& data, StringArrayRange values ){
	return addCombo( grid, name, values, IntImportCaller( data ), IntExportCaller( data ) );
}

void Dialog::addSlider( QGridLayout* grid, const char* name, int& data, int lower, int upper, int step_increment, int page_increment ){
	DialogSliderRow row( lower, upper, step_increment, page_increment );

	AddIntSliderData( *row.m_slider, IntImportCaller( data ), IntExportCaller( data ) );

	DialogGrid_packRow( grid, row.m_layout, name );
}

void Dialog::addSlider( QGridLayout* grid, const char* name, float& data, double lower, double upper, double step_increment, double page_increment ){
	DialogSliderRow row( lower, upper, step_increment, page_increment );

	AddFloatSliderData( *row.m_slider, FloatImportCaller( data ), FloatExportCaller( data ) );

	DialogGrid_packRow( grid, row.m_layout, name );
}

void Dialog::addRadio( QGridLayout* grid, const char* name, StringArrayRange names, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	RadioHBox radioBox = RadioHBox_new( names );
	AddIntRadioData( *radioBox.m_radio, importViewer, exportViewer );

	DialogGrid_packRow( grid, radioBox.m_hbox, name );
}

void Dialog::addRadio( QGridLayout* grid, const char* name, int& data, StringArrayRange names ){
	addRadio( grid, name, names, IntImportCaller( data ), IntExportCaller( data ) );
}

void Dialog::addRadioIcons( QGridLayout* grid, const char* name, StringArrayRange icons, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	auto subgrid = new QGridLayout;
	auto buttons = new QButtonGroup( subgrid );

	for ( size_t i = 0; i < icons.size(); ++i )
	{
		auto label = new QLabel;
		label->setPixmap( new_local_image( icons[i] ) );
		subgrid->addWidget( label, 0, i, Qt::AlignmentFlag::AlignHCenter );

		auto button = new QRadioButton;
		buttons->addButton( button, i ); // set ids 0+, default ones are negative
		subgrid->addWidget( button, 1, i, Qt::AlignmentFlag::AlignHCenter );
	}

	AddIntRadioData( *buttons, importViewer, exportViewer );

	DialogGrid_packRow( grid, subgrid, name );
}

void Dialog::addRadioIcons( QGridLayout* grid, const char* name, int& data, StringArrayRange icons ){
	addRadioIcons( grid, name, icons, IntImportCaller( data ), IntExportCaller( data ) );
}

QWidget* Dialog::addTextEntry( QGridLayout* grid, const char* name, const StringImportCallback& importViewer, const StringExportCallback& exportViewer ){
	auto entry = new QLineEdit;
	AddTextEntryData( *entry, importViewer, exportViewer );

	DialogGrid_packRow( grid, entry, name );
	return entry;
}

void Dialog::addPathEntry( QGridLayout* grid, const char* name, bool browse_directory, const StringImportCallback& importViewer, const StringExportCallback& exportViewer ){
	PathEntry pathEntry = PathEntry_new();

	if( browse_directory )
		QObject::connect( pathEntry.m_button, &QAction::triggered, [entry = pathEntry.m_entry](){ button_clicked_entry_browse_directory( entry ); } );
	else
		QObject::connect( pathEntry.m_button, &QAction::triggered, [entry = pathEntry.m_entry](){ button_clicked_entry_browse_file( entry ); } );

	AddTextEntryData( *pathEntry.m_entry, importViewer, exportViewer );

	DialogGrid_packRow( grid, pathEntry.m_entry, name );
}

void Dialog::addPathEntry( QGridLayout* grid, const char* name, CopiedString& data, bool browse_directory ){
	addPathEntry( grid, name, browse_directory, StringImportCallback( StringImportCaller( data ) ), StringExportCallback( StringExportCaller( data ) ) );
}

QWidget* Dialog::addSpinner( QGridLayout* grid, const char* name, int lower, int upper, const IntImportCallback& importViewer, const IntExportCallback& exportViewer ){
	auto spin = new SpinBox( lower, upper );
	spin->setStepType( QAbstractSpinBox::StepType::AdaptiveDecimalStepType );
	AddIntSpinnerData( *spin, importViewer, exportViewer );
	DialogGrid_packRow( grid, spin, new SpinBoxLabel( name, spin ) );
	return spin;
}

QWidget* Dialog::addSpinner( QGridLayout* grid, const char* name, int& data, int lower, int upper ){
	return addSpinner( grid, name, lower, upper, IntImportCallback( IntImportCaller( data ) ), IntExportCallback( IntExportCaller( data ) ) );
}

QWidget* Dialog::addSpinner( QGridLayout* grid, const char* name, double lower, double upper, const FloatImportCallback& importViewer, const FloatExportCallback& exportViewer, int decimals ){
	auto spin = new DoubleSpinBox( lower, upper, 0, decimals );
	spin->setStepType( QAbstractSpinBox::StepType::AdaptiveDecimalStepType );
	AddFloatSpinnerData( *spin, importViewer, exportViewer );
	DialogGrid_packRow( grid, spin, new SpinBoxLabel( name, spin ) );
	return spin;
}

QWidget* Dialog::addSpinner( QGridLayout* grid, const char* name, float& data, double lower, double upper, int decimals ){
	return addSpinner( grid, name, lower, upper, FloatImportCallback( FloatImportCaller( data ) ), FloatExportCallback( FloatExportCaller( data ) ), decimals );
}
