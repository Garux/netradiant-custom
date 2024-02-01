/*
   Sunplug plugin for GtkRadiant
   Copyright (C) 2004 Topsun
   Thanks to SPoG for help!

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sunplug.h"

#include "debugging/debugging.h"

#include "iplugin.h"

#include "string/string.h"
#include "stream/stringstream.h"
#include "modulesystem/singletonmodule.h"

#include "iundo.h"       // declaration of undo system
#include "ientity.h"     // declaration of entity system
#include "iscenegraph.h" // declaration of datastructure of the map

#include "scenelib.h"    // declaration of datastructure of the map
#include "qerplugin.h"   // declaration to use other interfaces as a plugin

#include "generic/vector.h"
#include "gtkutil/spinbox.h"
#include <QDialog>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>



void about_plugin_window();
void MapCoordinator();


class SunPlugPluginDependencies :
	public GlobalRadiantModuleRef,  // basic class for all other module refs
	public GlobalUndoModuleRef,     // used to say radiant that something has changed and to undo that
	public GlobalSceneGraphModuleRef, // necessary to handle data in the mapfile (change, retrieve data)
	public GlobalEntityModuleRef    // to access and modify the entities
{
public:
	SunPlugPluginDependencies() :
		GlobalEntityModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "entities" ) ){ //,
	}
};

//  *************************
// ** standard plugin stuff **
//  *************************
namespace SunPlug
{
QWidget* main_window;
char MenuList[100] = "";

const char* init( void* hApp, void* pMainWidget ){
	main_window = static_cast<QWidget*>( pMainWidget );
	return "Initializing SunPlug for GTKRadiant";
}
const char* getName(){
	return "SunPlug"; // name that is shown in the menue
}
const char* getCommandList(){
	const char about[] = "About...";
	const char etMapCoordinator[] = ";ET-MapCoordinator";

	strcat( MenuList, about );
//.	if ( strncmp( GlobalRadiant().getGameName(), "etmain", 6 ) == 0 ) {
		strcat( MenuList, etMapCoordinator );
//.	}
	return (const char*)MenuList;
}
const char* getCommandTitleList(){
	return "";
}
void dispatch( const char* command, float* vMin, float* vMax, bool bSingleBrush ){ // message processing
	if ( string_equal( command, "About..." ) ) {
		about_plugin_window();
	}
	if ( string_equal( command, "ET-MapCoordinator" ) ) {
		MapCoordinator();
	}
}
} // namespace

class SunPlugModule : public TypeSystemRef
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "SunPlug" );

	SunPlugModule(){
		m_plugin.m_pfnQERPlug_Init = &SunPlug::init;
		m_plugin.m_pfnQERPlug_GetName = &SunPlug::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &SunPlug::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &SunPlug::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &SunPlug::dispatch;
	}
	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<SunPlugModule, SunPlugPluginDependencies> SingletonSunPlugModule;

SingletonSunPlugModule g_SunPlugModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	g_SunPlugModule.selfRegister();
}



//  **************************
// ** find entities by class **  from radiant/map.cpp
//  **************************
class EntityFindByClassname : public scene::Graph::Walker
{
	const char* m_name;
	Entity*& m_entity;
public:
	EntityFindByClassname( const char* name, Entity*& entity ) : m_name( name ), m_entity( entity ){
		m_entity = 0;
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( m_entity == 0 ) {
			Entity* entity = Node_getEntity( path.top() );
			if ( entity != 0
			     && string_equal( m_name, entity->getClassName() ) ) {
				m_entity = entity;
			}
		}
		return true;
	}
};

Entity* Scene_FindEntityByClass( const char* name ){
	Entity* entity;
	GlobalSceneGraph().traverse( EntityFindByClassname( name, entity ) );
	return entity;
}

//  ************
// ** my stuff **
//  ************

// About dialog
void about_plugin_window(){
	GlobalRadiant().m_pfnMessageBox( SunPlug::main_window, "SunPlug v1.0 for NetRadiant 1.5\nby Topsun", "About SunPlug", EMessageBoxType::Info, 0 );
}


AABB GetMapBounds(){
	scene::Path path = makeReference( GlobalSceneGraph().root() ); // get the path to the root element of the graph
	scene::Instance* instance = GlobalSceneGraph().find( path ); // find the instance to the given path
	return instance->worldAABB(); // get the bounding box of the level
}

// get the current bounding box and return the optimal coordinates
auto GetOptimalCoordinates(){
	const AABB bounds = GetMapBounds();
	const float max = std::max( { bounds.extents.x(), bounds.extents.y(), 175.f } ); // the square must be at least 350x350 units
	return std::pair( BasicVector2<int>( bounds.origin.vec2() - Vector2( max, max ) ),
	                  BasicVector2<int>( bounds.origin.vec2() + Vector2( max, max ) ) );
}


// MapCoordinator dialog window
void MapCoordinator(){
	// find the entity worldspawn
	if ( Entity *theWorldspawn = Scene_FindEntityByClass( "worldspawn" ) ) { // need to have a worldspawn otherwise setting a value crashes the radiant
		// get the current values of the mapcoords
		BasicVector2<int> min( 0, 0 ), max( 0, 0 );
		{
			StringTokeniser tokeniser( theWorldspawn->getKeyValue( "mapcoordsmins" ) ); // upper left corner
			min.x() = atoi( tokeniser.getToken() ); // minimum of x value
			min.y() = atoi( tokeniser.getToken() ); // maximum of y value
		}
		{
			StringTokeniser tokeniser( theWorldspawn->getKeyValue( "mapcoordsmaxs" ) ); // lower right corner
			max.x() = atoi( tokeniser.getToken() ); // maximum of x value
			max.y() = atoi( tokeniser.getToken() ); // minimum of y value
		}

		globalOutputStream() << "SunPlug: calculating optimal coordinates\n"; // write to console that we are calculating the coordinates
		const auto [ calc_min, calc_max ] = GetOptimalCoordinates(); // calculate optimal mapcoords with the dimensions of the level bounding box
		globalOutputStream() << "SunPlug: advised mapcoordsmins=" << calc_min.x() << ' ' << calc_max.y() << '\n'; // console info about mapcoordsmins
		globalOutputStream() << "SunPlug: advised mapcoordsmaxs=" << calc_max.x() << ' ' << calc_min.y() << '\n'; // console info about mapcoordsmaxs

		{
			QDialog dialog( SunPlug::main_window, Qt::Dialog | Qt::WindowCloseButtonHint );
			dialog.setWindowTitle( "ET-MapCoordinator" );
			{
				auto form = new QFormLayout( &dialog );
				form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
				{
					auto spin_minX = new SpinBox( -65536, 65536, min.x() );
					auto spin_minY = new SpinBox( -65536, 65536, min.y() );
					auto spin_maxX = new SpinBox( -65536, 65536, max.x() );
					auto spin_maxY = new SpinBox( -65536, 65536, max.y() );
					spin_minX->setPrefix( "X: " );
					spin_minY->setPrefix( "Y: " );
					spin_maxX->setPrefix( "X: " );
					spin_maxY->setPrefix( "Y: " );
					{
						auto button = new QPushButton( "Get optimal mapcoords" );
						form->addWidget( button );
						QObject::connect( button, &QPushButton::clicked, [&, calc_min = calc_min, calc_max = calc_max](){
							spin_minX->setValue( calc_min.x() );
							spin_minY->setValue( calc_max.y() );
							spin_maxX->setValue( calc_max.x() );
							spin_maxY->setValue( calc_min.y() );
						} );
					}
					{
						auto line = new QFrame;
						line->setFrameShape( QFrame::Shape::HLine );
						line->setFrameShadow( QFrame::Shadow::Raised );
						form->addRow( line );
					}
					{
						auto hbox = new QHBoxLayout;
						hbox->addWidget( spin_minX );
						hbox->addWidget( spin_minY );
						form->addRow( new QLabel( "MapCoordsMins" ), hbox );
					}
					{
						auto hbox = new QHBoxLayout;
						hbox->addWidget( spin_maxX );
						hbox->addWidget( spin_maxY );
						form->addRow( new QLabel( "MapCoordsMaxs" ), hbox );
					}
					{
						auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
						form->addWidget( buttons );
						QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
						QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
						buttons->button( QDialogButtonBox::StandardButton::Ok )->setText( "Set" );
					}

					if( dialog.exec() ){
						UndoableCommand undo( "SunPlug.entitySetMapcoords" );
						theWorldspawn->setKeyValue( "mapcoordsmins", StringStream<64>( spin_minX->value(), ' ', spin_minY->value() ) );
						theWorldspawn->setKeyValue( "mapcoordsmaxs", StringStream<64>( spin_maxX->value(), ' ', spin_maxY->value() ) );
					}
				}
			}
		}
	}
	else {
		globalErrorStream() << "SunPlug: no worldspawn found!\n"; // output error to console

		GlobalRadiant().m_pfnMessageBox( SunPlug::main_window,
			"No worldspawn was found in the map!\nIn order to use this tool the map must have at least one brush in the worldspawn.",
			"ET-MapCoordinator", EMessageBoxType::Error, 0 );
	}
}
