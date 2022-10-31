

#include "debugging/debugging.h"
#include "callbacks.h"
#include "plugin.h"

#include <QEvent>
inline void qt_connect_shortcut_override( QWidget *widget ){
	class Filter : public QObject
	{
		using QObject::QObject;
	protected:
		bool eventFilter( QObject *obj, QEvent *event ) override {
			if( event->type() == QEvent::ShortcutOverride ) {
				event->accept();
				return true;
			}
			return QObject::eventFilter( obj, event ); // standard event processing
		}
	};
	widget->installEventFilter( new Filter( widget ) );
}

#include <QKeyEvent>
class Del_QListWidget : public QListWidget
{
	using QListWidget::QListWidget;
protected:
	void keyPressEvent( QKeyEvent *event ) override {
		if( event->matches( QKeySequence::StandardKey::Delete ) )
			callbacks::OnRemoveMaterial();
		QListWidget::keyPressEvent( event );
	}
};

QWidget* create_w_plugplug2(){
	auto window = g_dialog.window = new QWidget( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	window->setWindowTitle( "BrushExport-Plugin 3.0 by namespace" );
	qt_connect_shortcut_override( window );

	{
		auto grid = new QGridLayout( window );
		{
			auto r_collapse = g_dialog.r_collapse = new QRadioButton( "Collapse mesh" );
			r_collapse->setToolTip( "Collapse all brushes into a single group" );
			grid->addWidget( r_collapse, 0, 0 );

			auto r_collapsebymaterial = g_dialog.r_collapsebymaterial = new QRadioButton( "Collapse by material" );
			r_collapsebymaterial->setToolTip( "Collapse into groups by material" );
			grid->addWidget( r_collapsebymaterial, 1, 0 );

			auto r_nocollapse = g_dialog.r_nocollapse = new QRadioButton( "Don't collapse" );
			r_nocollapse->setToolTip( "Every brush is stored in its own group" );
			grid->addWidget( r_nocollapse, 2, 0 );
			r_nocollapse->setChecked( true );
		}
		{
			auto b_export = g_dialog.b_export = new QPushButton( "Save" );
			grid->addWidget( b_export, 0, 1 );
			b_export->setDisabled( true );
			QObject::connect( b_export, &QAbstractButton::clicked, callbacks::OnExportClicked );

			auto b_exportAs = new QPushButton( "Save As" );
			grid->addWidget( b_exportAs, 1, 1 );
			QObject::connect( b_exportAs, &QAbstractButton::clicked, [](){
				callbacks::OnExportClicked( true );
			} );

			auto b_close = new QPushButton( "Cancel" );
			grid->addWidget( b_close, 2, 1 );
			QObject::connect( b_close, &QAbstractButton::clicked, window, &QWidget::hide );
		}
		{
			grid->addWidget( new QLabel( "Ignored materials:" ), 3, 0, 1, 2, Qt::AlignmentFlag::AlignHCenter );
		}
		{
			auto t_materialist = g_dialog.t_materialist = new Del_QListWidget;
			grid->addWidget( t_materialist, 4, 0, 1, 2 );
			t_materialist->setEditTriggers( QAbstractItemView::EditTrigger::DoubleClicked | QAbstractItemView::EditTrigger::EditKeyPressed );
		}
		{
			auto ed_materialname = g_dialog.ed_materialname = new QLineEdit;
			grid->addWidget( ed_materialname, 5, 0, 1, 2 );
			QObject::connect( ed_materialname, &QLineEdit::returnPressed, callbacks::OnAddMaterial );
		}
		{
			auto b_addmaterial = new QPushButton( "Add" );
			grid->addWidget( b_addmaterial, 6, 0 );
			QObject::connect( b_addmaterial, &QAbstractButton::clicked, callbacks::OnAddMaterial );

			auto b_removematerial = new QPushButton( "Remove" );
			grid->addWidget( b_removematerial, 6, 1 );
			QObject::connect( b_removematerial, &QAbstractButton::clicked, callbacks::OnRemoveMaterial );
		}
		{
			auto t_limitmatnames = g_dialog.t_limitmatnames = new QCheckBox( "Use short material names (max. 20 chars)" );
			grid->addWidget( t_limitmatnames, 7, 0, 1, 2 );

			auto t_objects = g_dialog.t_objects = new QCheckBox( "Create (o)bjects instead of (g)roups" );
			grid->addWidget( t_objects, 8, 0, 1, 2 );

			auto t_weld = g_dialog.t_weld = new QCheckBox( "Weld vertices" );
			grid->addWidget( t_weld, 9, 0, 1, 2 );
			t_weld->setToolTip( "inside groups/objects" );
			t_weld->setChecked( true );

			auto t_exportmaterials = g_dialog.t_exportmaterials = new QCheckBox( "Create material information (.mtl file)" );
			grid->addWidget( t_exportmaterials, 10, 0, 1, 2 );
			t_exportmaterials->setChecked( true );
		}
	}

	return window;
}

// global main window, is 0 when not created
// spawn or unhide plugin window
void CreateWindow(){
	if( g_dialog.window == nullptr )
		g_dialog.window = create_w_plugplug2();
	g_dialog.window->show();
}
