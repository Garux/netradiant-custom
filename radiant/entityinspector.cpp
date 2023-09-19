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

#include "entityinspector.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "ifilesystem.h"
#include "imodel.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"

#include <map>
#include <set>
#include <variant>

#include <gtkutil/guisettings.h>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QCheckBox>
#include "gtkutil/lineedit.h"
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QKeyEvent>
#include <QApplication>
#include <QButtonGroup>
#include "gtkutil/combobox.h"

#include "os/path.h"
#include "eclasslib.h"
#include "scenelib.h"
#include "generic/callback.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "moduleobserver.h"
#include "convert.h"
#include "stringio.h"

#include "gtkutil/accelerator.h"
#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/entry.h"

#include "qe3.h"
#include "gtkmisc.h"
#include "gtkdlgs.h"
#include "entity.h"
#include "mainframe.h"
#include "textureentry.h"
#include "groupdialog.h"

#include "select.h"

namespace
{
typedef std::map<CopiedString, CopiedString> KeyValues;
KeyValues g_selectedKeyValues;
KeyValues g_selectedDefaultKeyValues;
}

const char* SelectedEntity_getValueForKey( const char* key ){
	{
		KeyValues::const_iterator i = g_selectedKeyValues.find( key );
		if ( i != g_selectedKeyValues.end() ) {
			return ( *i ).second.c_str();
		}
	}
	{
		KeyValues::const_iterator i = g_selectedDefaultKeyValues.find( key );
		if ( i != g_selectedDefaultKeyValues.end() ) {
			return ( *i ).second.c_str();
		}
	}
	return "";
}

void Scene_EntitySetKeyValue_Selected_Undoable( const char* key, const char* value ){
	StringOutputStream command( 256 );
	command << "entitySetKeyValue -key " << makeQuoted( key ) << " -value " << makeQuoted( value );
	UndoableCommand undo( command.c_str() );
	Scene_EntitySetKeyValue_Selected( key, value );
}

class EntityAttribute
{
public:
	virtual QWidget* getWidget() const = 0;
	virtual void update() = 0;
	virtual void release() = 0;
};

class BooleanAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	QCheckBox* m_check;
public:
	BooleanAttribute( const char* key ) :
		m_key( key ),
		m_check( new QCheckBox )
	{
		QObject::connect( m_check, &QAbstractButton::clicked, [this](){ apply(); } );
		update();
	}
	QWidget* getWidget() const override {
		return m_check;
	}
	void release() override {
		delete this;
	}
	void apply(){
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_check->isChecked() ? "1" : "" );
	}
	typedef MemberCaller<BooleanAttribute, &BooleanAttribute::apply> ApplyCaller;

	void update() override {
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		m_check->setChecked( atoi( value ) != 0 ); // atoi( empty ) is also 0
	}
	typedef MemberCaller<BooleanAttribute, &BooleanAttribute::update> UpdateCaller;
};


class StringAttribute : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry *m_entry;
public:
	StringAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ){
	}
	virtual ~StringAttribute() = default;
	QWidget* getWidget() const override {
		return m_entry;
	}
	QLineEdit* getEntry() const {
		return m_entry;
	}

	void release() override {
		delete this;
	}
	void apply(){
		const auto value = m_entry->text().toLatin1();
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), value.constData() );
	}
	typedef MemberCaller<StringAttribute, &StringAttribute::apply> ApplyCaller;

	void update() override {
		m_entry->setText( SelectedEntity_getValueForKey( m_key.c_str() ) );
	}
	typedef MemberCaller<StringAttribute, &StringAttribute::update> UpdateCaller;
};

class ShaderAttribute : public StringAttribute
{
public:
	ShaderAttribute( const char* key ) : StringAttribute( key ){
		GlobalShaderEntryCompletion::instance().connect( StringAttribute::getEntry() );
	}
};

class TextureAttribute : public StringAttribute
{
public:
	TextureAttribute( const char* key ) : StringAttribute( key ){
		if( string_empty( GlobalRadiant().getGameDescriptionKeyValue( "show_wads" ) ) )
			GlobalAllShadersEntryCompletion::instance().connect( StringAttribute::getEntry() );	// with textures/
		else
			GlobalTextureEntryCompletion::instance().connect( StringAttribute::getEntry() );	// w/o
	}
};


class ColorAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry *m_entry;
public:
	ColorAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ){
		auto button = m_entry->addAction( QApplication::style()->standardIcon( QStyle::SP_DialogOkButton ), QLineEdit::ActionPosition::TrailingPosition );
		QObject::connect( button, &QAction::triggered, [this](){ browse(); } );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_entry;
	}
	void apply(){
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_entry->text().toLatin1().constData() );
	}
	typedef MemberCaller<ColorAttribute, &ColorAttribute::apply> ApplyCaller;
	void update() override {
		m_entry->setText( SelectedEntity_getValueForKey( m_key.c_str() ) );
	}
	typedef MemberCaller<ColorAttribute, &ColorAttribute::update> UpdateCaller;
	void browse(){
		Vector3 color( 1, 1, 1 );
		string_parse_vector3( m_entry->text().toLatin1().constData(), color );
		if( color_dialog( m_entry->window(), color ) ){
			char buffer[64];
			sprintf( buffer, "%g %g %g", color[0], color[1], color[2] );
			m_entry->setText( buffer );
			apply();
		}
	}
};


class ModelAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry *m_entry;
public:
	ModelAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ){
		auto button = m_entry->addAction( QApplication::style()->standardIcon( QStyle::SP_DialogOpenButton ), QLineEdit::ActionPosition::TrailingPosition );
		QObject::connect( button, &QAction::triggered, [this](){ browse(); } );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_entry;
	}
	void apply(){
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_entry->text().toLatin1().constData() );
	}
	typedef MemberCaller<ModelAttribute, &ModelAttribute::apply> ApplyCaller;
	void update() override {
		m_entry->setText( SelectedEntity_getValueForKey( m_key.c_str() ) );
	}
	typedef MemberCaller<ModelAttribute, &ModelAttribute::update> UpdateCaller;
	void browse(){
		const char *filename = misc_model_dialog( m_entry->window(), m_entry->text().toLatin1().constData() );

		if ( filename != 0 ) {
			m_entry->setText( filename );
			apply();
		}
	}
};

const char* browse_sound( QWidget* parent, const char* filepath ){
	StringOutputStream buffer( 1024 );

	if( !string_empty( filepath ) ){
		const char* root = GlobalFileSystem().findFile( filepath );
		if( !string_empty( root ) && file_is_directory( root ) )
			buffer << root << filepath;
	}
	if( buffer.empty() ){
		buffer << g_qeglobals.m_userGamePath << "sound/";

		if ( !file_readable( buffer.c_str() ) ) {
			// just go to fsmain
			buffer.clear();
			buffer << g_qeglobals.m_userGamePath;
		}
	}

	const char* filename = file_dialog( parent, true, "Open Sound File", buffer.c_str(), "sound" );
	if ( filename != 0 ) {
		const char* relative = path_make_relative( filename, GlobalFileSystem().findRoot( filename ) );
		if ( relative == filename ) {
			globalWarningStream() << "WARNING: could not extract the relative path, using full path instead\n";
		}
		return relative;
	}
	return filename;
}

class SoundAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry *m_entry;
public:
	SoundAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ){
		auto button = m_entry->addAction( QApplication::style()->standardIcon( QStyle::SP_MediaVolume ), QLineEdit::ActionPosition::TrailingPosition );
		QObject::connect( button, &QAction::triggered, [this](){ browse(); } );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_entry;
	}
	void apply(){
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_entry->text().toLatin1().constData() );
	}
	typedef MemberCaller<SoundAttribute, &SoundAttribute::apply> ApplyCaller;
	void update() override {
		m_entry->setText( SelectedEntity_getValueForKey( m_key.c_str() ) );
	}
	typedef MemberCaller<SoundAttribute, &SoundAttribute::update> UpdateCaller;
	void browse(){
		const char *filename = browse_sound( m_entry->window(), m_entry->text().toLatin1().constData() );

		if ( filename != 0 ) {
			m_entry->setText( filename );
			apply();
		}
	}
};


inline double angle_normalised( double angle ){
	return float_mod( angle, 360.0 );
}
#include "camwindow.h"
class CamAnglesButton
{
	typedef Callback1<const Vector3&> ApplyCallback;
	ApplyCallback m_apply;
public:
	QPushButton* m_button;
	CamAnglesButton( const ApplyCallback& apply ) : m_apply( apply ), m_button( new QPushButton( "<-cam" ) ){
		QObject::connect( m_button, &QAbstractButton::clicked, [this](){
			Vector3 angles( Camera_getAngles( *g_pParentWnd->GetCamWnd() ) );
			if( !string_equal( GlobalRadiant().getRequiredGameDescriptionKeyValue( "entities" ), "quake" ) ) /* stupid quake bug */
				angles[0] = -angles[0];
			m_apply( angles );
		} );
	}
};

inline QWidget *new_container_widget(){
	QWidget *w = new QWidget;
	auto l = new QHBoxLayout( w );
	l->setContentsMargins( 0, 0, 0, 0 );
	return w;
}

class AngleAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry* m_entry;
	CamAnglesButton m_butt;
	QWidget *m_hbox;
public:
	AngleAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ),
		m_butt( ApplyVecCaller( *this ) ),
		m_hbox( new_container_widget() ){
		m_hbox->layout()->addWidget( m_entry );
		m_hbox->layout()->addWidget( m_butt.m_button );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_hbox;
	}
	void apply(){
		StringOutputStream angle( 32 );
		angle << angle_normalised( entry_get_float( m_entry ) );
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angle.c_str() );
	}
	typedef MemberCaller<AngleAttribute, &AngleAttribute::apply> ApplyCaller;

	void update() override {
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		if ( !string_empty( value ) ) {
			StringOutputStream angle( 32 );
			angle << angle_normalised( atof( value ) );
			m_entry->setText( angle.c_str() );
		}
		else
		{
			m_entry->setText( "0" );
		}
	}
	typedef MemberCaller<AngleAttribute, &AngleAttribute::update> UpdateCaller;

	void apply( const Vector3& angles ){
		entry_set_float( m_entry, angles[1] );
		apply();
	}
	typedef MemberCaller1<AngleAttribute, const Vector3&, &AngleAttribute::apply> ApplyVecCaller;
};

class DirectionAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	NonModalEntry* m_entry;
	RadioHBox m_radio;
	CamAnglesButton m_butt;
	QWidget* m_hbox;
	static constexpr const char *const buttons[] = { "up", "down", "yaw" };
public:
	DirectionAttribute( const char* key ) :
		m_key( key ),
		m_entry( new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) ),
		m_radio( RadioHBox_new( StringArrayRange( buttons ) ) ),
		m_butt( ApplyVecCaller( *this ) ),
		m_hbox( new_container_widget() ){
		static_cast<QHBoxLayout*>( m_hbox->layout() )->addLayout( m_radio.m_hbox );
		m_hbox->layout()->addWidget( m_entry );
		m_hbox->layout()->addWidget( m_butt.m_button );
		QObject::connect( m_radio.m_radio, QOverload<int>::of( &QButtonGroup::buttonClicked ), ApplyRadioCaller( *this ) );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_hbox;
	}
	void apply(){
		StringOutputStream angle( 32 );
		angle << angle_normalised( entry_get_float( m_entry ) );
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angle.c_str() );
	}
	typedef MemberCaller<DirectionAttribute, &DirectionAttribute::apply> ApplyCaller;

	void update() override {
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		if ( !string_empty( value ) ) {
			const float f = atof( value );
			if ( f == -1 ) {
				m_entry->setEnabled( false );
				m_radio.m_radio->button( 0 )->setChecked( true );
				m_entry->clear();
			}
			else if ( f == -2 ) {
				m_entry->setEnabled( false );
				m_radio.m_radio->button( 1 )->setChecked( true );
				m_entry->clear();
			}
			else
			{
				m_entry->setEnabled( true );
				m_radio.m_radio->button( 2 )->setChecked( true );
				StringOutputStream angle( 32 );
				angle << angle_normalised( f );
				m_entry->setText( angle.c_str() );
			}
		}
		else
		{
			m_radio.m_radio->button( 2 )->setChecked( true );
			m_entry->setText( "0" );
		}
	}
	typedef MemberCaller<DirectionAttribute, &DirectionAttribute::update> UpdateCaller;

	void applyRadio( int id ){
		if ( id == 0 ) {
			Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), "-1" );
		}
		else if ( id == 1 ) {
			Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), "-2" );
		}
		else if ( id == 2 ) {
			apply();
		}
	}
	typedef MemberCaller1<DirectionAttribute, int, &DirectionAttribute::applyRadio> ApplyRadioCaller;

	void apply( const Vector3& angles ){
		entry_set_float( m_entry, angles[1] );
		apply();
	}
	typedef MemberCaller1<DirectionAttribute, const Vector3&, &DirectionAttribute::apply> ApplyVecCaller;
};


class AnglesEntry
{
public:
	QLineEdit* m_roll;
	QLineEdit* m_pitch;
	QLineEdit* m_yaw;
	AnglesEntry() : m_roll( 0 ), m_pitch( 0 ), m_yaw( 0 ){
	}
};

class AnglesAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	AnglesEntry m_angles;
	CamAnglesButton m_butt;
	QWidget* m_hbox;
public:
	AnglesAttribute( const char* key ) :
		m_key( key ),
		m_butt( ApplyVecCaller( *this ) ),
		m_hbox( new_container_widget() ){
		m_hbox->layout()->addWidget( m_angles.m_pitch = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
		m_hbox->layout()->addWidget( m_angles.m_yaw = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
		m_hbox->layout()->addWidget( m_angles.m_roll = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
		m_hbox->layout()->addWidget( m_butt.m_button );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_hbox;
	}
	void apply(){
		StringOutputStream angles( 64 );
		angles << angle_normalised( entry_get_float( m_angles.m_pitch ) )
		       << " " << angle_normalised( entry_get_float( m_angles.m_yaw ) )
		       << " " << angle_normalised( entry_get_float( m_angles.m_roll ) );
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angles.c_str() );
	}
	typedef MemberCaller<AnglesAttribute, &AnglesAttribute::apply> ApplyCaller;

	void update() override {
		StringOutputStream angle( 32 );
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		if ( !string_empty( value ) ) {
			DoubleVector3 pitch_yaw_roll;
			if ( !string_parse_vector3( value, pitch_yaw_roll ) ) {
				pitch_yaw_roll = DoubleVector3( 0, 0, 0 );
			}

			angle( angle_normalised( pitch_yaw_roll.x() ) );
			m_angles.m_pitch->setText( angle.c_str() );

			angle( angle_normalised( pitch_yaw_roll.y() ) );
			m_angles.m_yaw->setText( angle.c_str() );

			angle( angle_normalised( pitch_yaw_roll.z() ) );
			m_angles.m_roll->setText( angle.c_str() );
		}
		else
		{
			m_angles.m_pitch->setText( "0" );
			m_angles.m_yaw->setText( "0" );
			m_angles.m_roll->setText( "0" );
		}
	}
	typedef MemberCaller<AnglesAttribute, &AnglesAttribute::update> UpdateCaller;

	void apply( const Vector3& angles ){
		entry_set_float( m_angles.m_pitch, angles[0] );
		entry_set_float( m_angles.m_yaw, angles[1] );
		entry_set_float( m_angles.m_roll, 0 );
		apply();
	}
	typedef MemberCaller1<AnglesAttribute, const Vector3&, &AnglesAttribute::apply> ApplyVecCaller;
};

class Vector3Entry
{
public:
	QLineEdit* m_x;
	QLineEdit* m_y;
	QLineEdit* m_z;
	Vector3Entry() : m_x( 0 ), m_y( 0 ), m_z( 0 ){
	}
};

class Vector3Attribute final : public EntityAttribute
{
	const CopiedString m_key;
	Vector3Entry m_vector3;
	QWidget* m_hbox;
public:
	Vector3Attribute( const char* key ) :
		m_key( key ),
		m_hbox( new_container_widget() ){
		m_hbox->layout()->addWidget( m_vector3.m_x = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
		m_hbox->layout()->addWidget( m_vector3.m_y = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
		m_hbox->layout()->addWidget( m_vector3.m_z = new NonModalEntry( ApplyCaller( *this ), UpdateCaller( *this ) ) );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_hbox;
	}
	void apply(){
		StringOutputStream vector3( 64 );
		vector3 << entry_get_float( m_vector3.m_x )
		        << " " << entry_get_float( m_vector3.m_y )
		        << " " << entry_get_float( m_vector3.m_z );
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), vector3.c_str() );
	}
	typedef MemberCaller<Vector3Attribute, &Vector3Attribute::apply> ApplyCaller;

	void update() override {
		StringOutputStream buffer( 32 );
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		if ( !string_empty( value ) ) {
			DoubleVector3 x_y_z;
			if ( !string_parse_vector3( value, x_y_z ) ) {
				x_y_z = DoubleVector3( 0, 0, 0 );
			}

			buffer( x_y_z.x() );
			m_vector3.m_x->setText( buffer.c_str() );

			buffer( x_y_z.y() );
			m_vector3.m_y->setText( buffer.c_str() );

			buffer( x_y_z.z() );
			m_vector3.m_z->setText( buffer.c_str() );
		}
		else
		{
			m_vector3.m_x->setText( "0" );
			m_vector3.m_y->setText( "0" );
			m_vector3.m_z->setText( "0" );
		}
	}
	typedef MemberCaller<Vector3Attribute, &Vector3Attribute::update> UpdateCaller;
};

class ListAttribute final : public EntityAttribute
{
	const CopiedString m_key;
	QComboBox* m_combo;
	const ListAttributeType& m_type;
public:
	ListAttribute( const char* key, const ListAttributeType& type ) :
		m_key( key ),
		m_combo( new ComboBox ),
		m_type( type ){
		for ( const auto&[ name, value ] : type )
		{
			m_combo->addItem( name.c_str() );
		}
		QObject::connect( m_combo, QOverload<int>::of( &QComboBox::activated ), ApplyCaller( *this ) );
	}
	void release() override {
		delete this;
	}
	QWidget* getWidget() const override {
		return m_combo;
	}
	void apply(){
		// looks safe to assume that user actions wont make m_combo->currentIndex() -1
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_type[m_combo->currentIndex()].second.c_str() );
	}
	typedef MemberCaller<ListAttribute, &ListAttribute::apply> ApplyCaller;

	void update() override {
		const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
		ListAttributeType::const_iterator i = m_type.findValue( value );
		if ( i != m_type.end() ) {
			m_combo->setCurrentIndex( static_cast<int>( std::distance( m_type.begin(), i ) ) );
		}
		else
		{
			m_combo->setCurrentIndex( 0 );
		}
	}
	typedef MemberCaller<ListAttribute, &ListAttribute::update> UpdateCaller;
};


namespace
{
bool g_entityInspector_windowConstructed = false;

QTreeWidget* g_entityClassList;
QPlainTextEdit* g_entityClassComment;

QCheckBox* g_entitySpawnflagsCheck[MAX_FLAGS];

QLineEdit* g_entityKeyEntry;
QLineEdit* g_entityValueEntry;

QToolButton* g_focusToggleButton;

QTreeWidget* g_entprops_store;
const EntityClass* g_current_flags = 0;
const EntityClass* g_current_comment = 0;
const EntityClass* g_current_attributes = 0;

// the number of active spawnflags
int g_spawnflag_count;
// table: index, match spawnflag item to the spawnflag index (i.e. which bit)
int spawn_table[MAX_FLAGS];
// we change the layout depending on how many spawn flags we need to display
// the table is a 4x4 in which we need to put the comment box g_entityClassComment and the spawn flags..
QGridLayout* g_spawnflagsTable;

QGridLayout* g_attributeBox = nullptr;
typedef std::vector<EntityAttribute*> EntityAttributes;
EntityAttributes g_entityAttributes;
}

void GlobalEntityAttributes_clear(){
	for ( EntityAttribute* attr : g_entityAttributes )
	{
		attr->release();
	}
	g_entityAttributes.clear();
}

class GetKeyValueVisitor : public Entity::Visitor
{
	KeyValues& m_keyvalues;
public:
	GetKeyValueVisitor( KeyValues& keyvalues )
		: m_keyvalues( keyvalues ){
	}

	void visit( const char* key, const char* value ){
		m_keyvalues.insert( KeyValues::value_type( CopiedString( key ), CopiedString( value ) ) );
	}

};

void Entity_GetKeyValues( const Entity& entity, KeyValues& keyvalues, KeyValues& defaultValues ){
	GetKeyValueVisitor visitor( keyvalues );

	entity.forEachKeyValue( visitor );

	const EntityClassAttributes& attributes = entity.getEntityClass().m_attributes;

	for ( EntityClassAttributes::const_iterator i = attributes.begin(); i != attributes.end(); ++i )
	{
		defaultValues.insert( KeyValues::value_type( ( *i ).first, ( *i ).second.m_value ) );
	}
}

void Entity_GetKeyValues_Selected( KeyValues& keyvalues, KeyValues& defaultValues ){
	class EntityGetKeyValues : public SelectionSystem::Visitor
	{
		KeyValues& m_keyvalues;
		KeyValues& m_defaultValues;
		mutable std::set<Entity*> m_visited;
	public:
		EntityGetKeyValues( KeyValues& keyvalues, KeyValues& defaultValues )
			: m_keyvalues( keyvalues ), m_defaultValues( defaultValues ){
		}
		void visit( scene::Instance& instance ) const {
			Entity* entity = Node_getEntity( instance.path().top() );
			if ( entity == 0 && instance.path().size() != 1 ) {
				entity = Node_getEntity( instance.path().parent() );
			}
			if ( entity != 0 && m_visited.insert( entity ).second ) {
				Entity_GetKeyValues( *entity, m_keyvalues, m_defaultValues );
			}
		}
	} visitor( keyvalues, defaultValues );
	GlobalSelectionSystem().foreachSelected( visitor );
}

const char* keyvalues_valueforkey( KeyValues& keyvalues, const char* key ){
	KeyValues::iterator i = keyvalues.find( CopiedString( key ) );
	if ( i != keyvalues.end() ) {
		return ( *i ).second.c_str();
	}
	return "";
}

// required to store EntityClass* in QVariant
Q_DECLARE_METATYPE( EntityClass* )
class EntityClassListStoreAppend : public EntityClassVisitor
{
	QTreeWidget* tree;
public:
	EntityClassListStoreAppend( QTreeWidget* tree_ ) : tree( tree_ ){
	}
	void visit( EntityClass* e ){
		auto item = new QTreeWidgetItem( tree );
		item->setData( 0, Qt::ItemDataRole::DisplayRole, e->name() );
		item->setData( 0, Qt::ItemDataRole::UserRole, QVariant::fromValue( e ) );
	}
};

void EntityClassList_fill(){
	EntityClassListStoreAppend append( g_entityClassList );
	GlobalEntityClassManager().forEach( append );
}

void EntityClassList_clear(){
	g_entityClassList->clear();
}

void SetComment( EntityClass* eclass ){
	if ( eclass == g_current_comment ) {
		return;
	}

	g_current_comment = eclass;

	if( eclass == nullptr ){
		g_entityClassComment->clear();
		return;
	}

	g_entityClassComment->setPlainText( eclass->comments() );

	{	// Catch patterns like "\nstuff :" used to describe keys and spawnflags, and make them bold for readability.
		QTextCharFormat format;
		format.setFontWeight( QFont::Weight::Bold );

		QTextDocument *document = g_entityClassComment->document();
		const QRegularExpression rx( "^\\s*\\w+(?=\\s*:)", QRegularExpression::PatternOption::MultilineOption );
		for( QTextCursor cursor( document ); cursor = document->find( rx, cursor ), !cursor.isNull(); )
			cursor.mergeCharFormat( format );
	}
}

void EntityAttribute_setTooltip( QWidget* widget, const char* name, const char* description ){
	StringOutputStream stream( 256 );
	if( string_not_empty( name ) )
		stream << "<b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" << name << "</b>&nbsp;&nbsp;&nbsp;&nbsp;";
	if( string_not_empty( description ) ){
		stream << "<br>" << description;
	}
	if( !stream.empty() )
		widget->setToolTip( stream.c_str() );
}

void SpawnFlags_setEntityClass( EntityClass* eclass ){
	if ( eclass == g_current_flags ) {
		return;
	}

	g_current_flags = eclass;

	g_spawnflag_count = 0;

	// do a first pass to count the spawn flags, don't touch the widgets, we don't know in what state they are
	for ( int i = 0; i < MAX_FLAGS; i++ )
	{
		if ( !string_empty( eclass->flagnames[i] ) ) {
			spawn_table[g_spawnflag_count++] = i;
		}
		// hide all boxes
		g_entitySpawnflagsCheck[i]->hide();
	}

	for ( int i = 0; i < g_spawnflag_count; ++i )
	{
		const auto str = StringOutputStream( 16 )( LowerCase( eclass->flagnames[spawn_table[i]] ) );

		QCheckBox *check = g_entitySpawnflagsCheck[i];
		check->setText( str.c_str() );
		check->show();

		if( const EntityClassAttribute* attribute = eclass->flagAttributes[spawn_table[i]] ){
			EntityAttribute_setTooltip( check, attribute->m_name.c_str(), attribute->m_description.c_str() );
		}
	}
}

void EntityClassList_selectEntityClass( EntityClass* eclass ){
	const auto list = g_entityClassList->findItems( eclass->name(), Qt::MatchFlag::MatchFixedString );
	g_entityClassList->setCurrentItem( !list.isEmpty()
	                                   ? list.first()
	                                   : nullptr );
	// g_entityClassComment is only updated via g_entityClassList selection change
	// using special nullprt case to also update it on selection of unknown entity added during runtime
	// hence this->EntityClassList_selection_changed()->SetComment() must handle nullptr
}

void EntityInspector_appendAttribute( const EntityClassAttributePair& attributePair, EntityAttribute& attribute ){
	const char* keyname = attributePair.first.c_str(); //EntityClassAttributePair_getName( attributePair );
	auto label = new QLabel( keyname );
	EntityAttribute_setTooltip( label, attributePair.second.m_name.c_str(), attributePair.second.m_description.c_str() );
	DialogGrid_packRow( g_attributeBox, attribute.getWidget(), label );
}


template<typename Attribute>
class StatelessAttributeCreator
{
public:
	static EntityAttribute* create( const char* name ){
		return new Attribute( name );
	}
};

class EntityAttributeFactory
{
	typedef EntityAttribute* ( *CreateFunc )( const char* name );
	typedef std::map<const char*, CreateFunc, RawStringLess> Creators;
	Creators m_creators;
public:
	EntityAttributeFactory(){
		m_creators.insert( Creators::value_type( "string", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "array", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "integer", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "boolean", &StatelessAttributeCreator<BooleanAttribute>::create ) );
		m_creators.insert( Creators::value_type( "real", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "angle", &StatelessAttributeCreator<AngleAttribute>::create ) );
		m_creators.insert( Creators::value_type( "direction", &StatelessAttributeCreator<DirectionAttribute>::create ) );
		m_creators.insert( Creators::value_type( "vector3", &StatelessAttributeCreator<Vector3Attribute>::create ) );
		m_creators.insert( Creators::value_type( "real3", &StatelessAttributeCreator<Vector3Attribute>::create ) );
		m_creators.insert( Creators::value_type( "angles", &StatelessAttributeCreator<AnglesAttribute>::create ) );
		m_creators.insert( Creators::value_type( "color", &StatelessAttributeCreator<ColorAttribute>::create ) );
		m_creators.insert( Creators::value_type( "target", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "targetname", &StatelessAttributeCreator<StringAttribute>::create ) );
		m_creators.insert( Creators::value_type( "sound", &StatelessAttributeCreator<SoundAttribute>::create ) );
		m_creators.insert( Creators::value_type( "shader", &StatelessAttributeCreator<ShaderAttribute>::create ) );
		m_creators.insert( Creators::value_type( "texture", &StatelessAttributeCreator<TextureAttribute>::create ) );
		m_creators.insert( Creators::value_type( "model", &StatelessAttributeCreator<ModelAttribute>::create ) );
		m_creators.insert( Creators::value_type( "skin", &StatelessAttributeCreator<StringAttribute>::create ) );
	}
	EntityAttribute* create( const char* type, const char* name ){
		Creators::iterator i = m_creators.find( type );
		if ( i != m_creators.end() ) {
			return ( *i ).second( name );
		}
		const ListAttributeType* listType = GlobalEntityClassManager().findListType( type );
		if ( listType != 0 ) {
			return new ListAttribute( name, *listType );
		}
		return 0;
	}
};

typedef Static<EntityAttributeFactory> GlobalEntityAttributeFactory;

void EntityInspector_setEntityClass( EntityClass *eclass ){
	EntityClassList_selectEntityClass( eclass );
	SpawnFlags_setEntityClass( eclass );

	if ( eclass != g_current_attributes ) {
		g_current_attributes = eclass;

		while( QLayoutItem *item = g_attributeBox->takeAt( 0 ) ){
			delete item->widget();
			delete item;
		}
		g_attributeBox->update(); // trigger scrollbar update
		GlobalEntityAttributes_clear();

		for ( const EntityClassAttributePair &pair : eclass->m_attributes )
		{
			EntityAttribute* attribute = GlobalEntityAttributeFactory::instance().create( pair.second.m_type.c_str(), pair.first.c_str() );
			if ( attribute != 0 ) {
				g_entityAttributes.push_back( attribute );
				EntityInspector_appendAttribute( pair, *g_entityAttributes.back() );
			}
		}
	}
}

void EntityInspector_updateSpawnflags(){
	{
		const int f = atoi( SelectedEntity_getValueForKey( "spawnflags" ) );
		for ( int i = 0; i < g_spawnflag_count; ++i )
		{
			const bool v = !!( f & ( 1 << spawn_table[i] ) );

			g_entitySpawnflagsCheck[i]->setChecked( v );
		}
	}
}

void EntityInspector_applySpawnflags(){
	int f = 0;

	for ( int i = 0; i < g_spawnflag_count; ++i )
	{
		const int v = g_entitySpawnflagsCheck[i]->isChecked();
		f |= v << spawn_table[i];
	}

	char sz[32];
	sprintf( sz, "%i", f );
	const char* value = ( f == 0 ) ? "" : sz;

	{
		const auto command = StringOutputStream( 64 )( "entitySetSpawnflags -flags ", f );
		UndoableCommand undo( command );

		Scene_EntitySetKeyValue_Selected( "spawnflags", value );
	}
}


void EntityInspector_updateKeyValues(){
	g_selectedKeyValues.clear();
	g_selectedDefaultKeyValues.clear();
	Entity_GetKeyValues_Selected( g_selectedKeyValues, g_selectedDefaultKeyValues );

	EntityInspector_setEntityClass( GlobalEntityClassManager().findOrInsert( keyvalues_valueforkey( g_selectedKeyValues, "classname" ), false ) );

	EntityInspector_updateSpawnflags();

	g_entprops_store->clear();
	// Walk through list and add pairs
	for ( const auto&[ key, value ] : g_selectedKeyValues )
	{
		g_entprops_store->addTopLevelItem( new QTreeWidgetItem( { key.c_str(), value.c_str() } ) );
	}

	for ( EntityAttribute *attr : g_entityAttributes )
	{
		attr->update();
	}
}

class EntityInspectorDraw
{
	IdleDraw m_idleDraw;
public:
	EntityInspectorDraw() : m_idleDraw( FreeCaller<EntityInspector_updateKeyValues>( ) ){
	}
	void queueDraw(){
		m_idleDraw.queueDraw();
	}
};

EntityInspectorDraw g_EntityInspectorDraw;


void EntityInspector_keyValueChanged(){
	g_EntityInspectorDraw.queueDraw();
}
void EntityInspector_selectionChanged( const Selectable& ){
	EntityInspector_keyValueChanged();
}

void EntityInspector_applyKeyValue(){
	// Get current selection text
	const auto key = g_entityKeyEntry->text().toLatin1();
	const auto value = g_entityValueEntry->text().toLatin1();

	// TTimo: if you change the classname to worldspawn you won't merge back in the structural brushes but create a parasite entity
//	if ( !strcmp( key.c_str(), "classname" ) && !strcmp( value.c_str(), "worldspawn" ) ) {
//		qt_MessageBox( g_entityKeyEntry->window(), "Cannot change \"classname\" key back to worldspawn." );
//		return;
//	}

	// RR2DO2: we don't want spaces and special symbols in entity keys
	if ( std::any_of( key.cbegin(), key.cend(), []( const char c ){ return strchr( " \n\r\t\v\"", c ) != nullptr; } ) ) {
		qt_MessageBox( g_entityKeyEntry->window(), "No spaces, newlines, tabs, quotes are allowed in entity key names." );
		return;
	}
	if ( std::any_of( value.cbegin(), value.cend(), []( const char c ){ return strchr( "\n\r\"", c ) != nullptr; } ) ) {
		qt_MessageBox( g_entityKeyEntry->window(), "No newlines & quotes are allowed in entity key values." );
		return;
	}
	// avoid empty key name; empty value is okay: deletes key
	if( key.isEmpty() )
		return;

	if ( string_equal( key.constData(), "classname" ) ) {
		Scene_EntitySetClassname_Selected( value.constData() );
	}
	else
	{
		Scene_EntitySetKeyValue_Selected_Undoable( key.constData(), value.constData() );
	}
}

void EntityInspector_clearKeyValue(){
	// Get current selection text
	if( const auto item = g_entprops_store->currentItem() ){
		const auto key = item->text( 0 ).toLatin1();

		if ( !string_equal( key.constData(), "classname" ) ) {
			const auto command = StringOutputStream( 64 )( "entityDeleteKey -key ", key.constData() );
			UndoableCommand undo( command.c_str() );
			Scene_EntitySetKeyValue_Selected( key.constData(), "" );
		}
	}
}

class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Delete ){
				EntityInspector_clearKeyValue();
				event->accept();
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_EntityProperties_keypress;

void EntityInspector_clearAllKeyValues(){
	UndoableCommand undo( "entityClear" );

	// remove all keys except classname and origin
	for ( const auto&[ key, value ] : g_selectedKeyValues )
	{
		if ( !string_equal( key.c_str(), "classname" ) && !string_equal( key.c_str(), "origin" ) ) {
			Scene_EntitySetKeyValue_Selected( key.c_str(), "" );
		}
	}
}

// =============================================================================
// callbacks

static void EntityClassList_selection_changed( QTreeWidgetItem *current, QTreeWidgetItem *previous ){
	SetComment( current != nullptr
	            ? current->data( 0, Qt::ItemDataRole::UserRole ).value<EntityClass*>()
	            : nullptr );
}

static void EntityProperties_selection_changed( QTreeWidgetItem *item, int column ){
	if( item != nullptr ){
		g_entityKeyEntry->setText( item->text( 0 ) );
		g_entityValueEntry->setText( item->text( 1 ) );
	}
}


class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Return
			 || keyEvent->key() == Qt::Key_Enter
			 || keyEvent->key() == Qt::Key_Tab
			 || keyEvent->key() == Qt::Key_Up
			 || keyEvent->key() == Qt::Key_Down
			 || keyEvent->key() == Qt::Key_PageUp
			 || keyEvent->key() == Qt::Key_PageDown ){
				event->accept();
			}
		}
		// clear focus widget while showing to keep global shortcuts working
		else if( event->type() == QEvent::Show ) {
			QTimer::singleShot( 0, [obj](){
				if( static_cast<QWidget*>( obj )->focusWidget() != nullptr )
					static_cast<QWidget*>( obj )->focusWidget()->clearFocus();
			} );
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_pressedKeysFilter;


void EntityInspector_destroyWindow(){
	g_entityInspector_windowConstructed = false;
	GlobalEntityAttributes_clear();
}

QWidget* EntityInspector_constructWindow( QWidget* toplevel ){
	auto splitter = new QSplitter( Qt::Vertical );

	QObject::connect( splitter, &QObject::destroyed, EntityInspector_destroyWindow );
	splitter->installEventFilter( &g_pressedKeysFilter );

	{
		// class list
		auto tree = g_entityClassList = new QTreeWidget;
		tree->setColumnCount( 1 );
		tree->setSortingEnabled( true );
		tree->sortByColumn( 0, Qt::SortOrder::AscendingOrder );
		tree->setUniformRowHeights( true ); // optimization
		tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
		tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
		tree->header()->setStretchLastSection( false ); // non greedy column sizing
		tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
		tree->setHeaderHidden( true );
		tree->setRootIsDecorated( false );
		tree->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
		tree->setAutoScroll( true );

		QObject::connect( tree, &QTreeWidget::itemActivated, []( QTreeWidgetItem *item, int column ){
			Scene_EntitySetClassname_Selected( item->text( 0 ).toLatin1().constData() );
		} );
		QObject::connect( tree, &QTreeWidget::currentItemChanged, EntityClassList_selection_changed );

		splitter->addWidget( tree );
	}
	{
		auto text = g_entityClassComment = new QPlainTextEdit;
		text->setReadOnly( true );
		text->setUndoRedoEnabled( false );

		splitter->addWidget( text );
	}
	{
		QWidget *containerWidget = new QWidget; // Adding a QLayout to a QSplitter is not supported, use proxy widget
		splitter->addWidget( containerWidget );
		auto vbox = new QVBoxLayout( containerWidget );
		vbox->setContentsMargins( 0, 0, 0, 0 );
		{
			// Spawnflags (4 colums wide max, or window gets too wide.)
			auto grid = g_spawnflagsTable = new QGridLayout;
			grid->setAlignment( Qt::AlignmentFlag::AlignLeft );
			vbox->addLayout( grid );
			for ( int i = 0; i < MAX_FLAGS; i++ )
			{
				auto check = g_entitySpawnflagsCheck[i] = new QCheckBox;
				grid->addWidget( check, i / 4, i % 4 );
				check->hide();
				QObject::connect( check, &QAbstractButton::clicked, EntityInspector_applySpawnflags );
			}
		}
		{
			// key/value list
			auto tree = g_entprops_store = new QTreeWidget;
			tree->setColumnCount( 2 );
			tree->setUniformRowHeights( true ); // optimization
			tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
			tree->header()->setSectionResizeMode( 0, QHeaderView::ResizeMode::ResizeToContents ); // no text elision
			tree->setHeaderHidden( true );
			tree->setRootIsDecorated( false );
			tree->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );

			QObject::connect( tree, &QTreeWidget::itemPressed, EntityProperties_selection_changed );
			tree->installEventFilter( &g_EntityProperties_keypress );

			vbox->addWidget( tree );
		}

		{
			// key/value entry
			auto grid = new QGridLayout;
			vbox->addLayout( grid );
			{
				grid->addWidget( new QLabel( "Key" ), 0, 0 );
				grid->addWidget( new QLabel( "Value" ), 1, 0 );
			}
			{
				auto line = g_entityKeyEntry = new LineEdit;
				grid->addWidget( line, 0, 1 );
				QObject::connect( line, &QLineEdit::returnPressed, [](){ g_entityValueEntry->setFocus(); g_entityValueEntry->selectAll(); } );
			}

			{
				auto line = g_entityValueEntry = new LineEdit;
				grid->addWidget( line, 1, 1 );
				QObject::connect( line, &QLineEdit::returnPressed, [](){ EntityInspector_applyKeyValue(); } );
			}
			/* select by key/value buttons */
			{
				auto b = new QToolButton;
				b->setText( "+" );
				b->setToolTip( "Select by key" );
				grid->addWidget( b, 0, 2 );
				QObject::connect( b, &QAbstractButton::clicked, [](){
					Select_EntitiesByKeyValue( g_entityKeyEntry->text().toLatin1().constData(), nullptr );
				} );
			}
			{
				auto b = new QToolButton;
				b->setText( "+" );
				b->setToolTip( "Select by value" );
				grid->addWidget( b, 1, 2 );
				QObject::connect( b, &QAbstractButton::clicked, [](){
					Select_EntitiesByKeyValue( nullptr, g_entityValueEntry->text().toLatin1().constData() );
				} );
			}
			{
				auto b = new QToolButton;
				b->setText( "+" );
				b->setToolTip( "Select by key + value" );
				grid->addWidget( b, 0, 3, 2, 1 );
				QObject::connect( b, &QAbstractButton::clicked, [](){
					Select_EntitiesByKeyValue( g_entityKeyEntry->text().toLatin1().constData(), g_entityValueEntry->text().toLatin1().constData() );
				} );
			}
		}
		{
			auto hbox = new QHBoxLayout;
			vbox->addLayout( hbox );
			{
				auto b = new QPushButton( "Clear All" );
				hbox->addWidget( b );
				QObject::connect( b, &QAbstractButton::clicked, EntityInspector_clearAllKeyValues );
			}
			{
				auto b = new QPushButton( "Delete Key" );
				hbox->addWidget( b );
				QObject::connect( b, &QAbstractButton::clicked, EntityInspector_clearKeyValue );
			}
			{
				auto b = new QToolButton;
				hbox->addWidget( b );
				b->setText( "<" );
				b->setToolTip( "Select targeting entities" );
				QObject::connect( b, &QAbstractButton::clicked, [](){ Select_ConnectedEntities( true, false, g_focusToggleButton->isChecked() ); } );
			}
			{
				auto b = new QToolButton;
				hbox->addWidget( b );
				b->setText( ">" );
				b->setToolTip( "Select targets" );
				QObject::connect( b, &QAbstractButton::clicked, [](){ Select_ConnectedEntities( false, true, g_focusToggleButton->isChecked() ); } );
			}
			{
				auto b = new QToolButton;
				hbox->addWidget( b );
				b->setText( "<->" );
				b->setToolTip( "Select connected entities" );
				QObject::connect( b, &QAbstractButton::clicked, [](){ Select_ConnectedEntities( true, true, g_focusToggleButton->isChecked() ); } );
			}
			{
				auto b = g_focusToggleButton = new QToolButton;
				hbox->addWidget( b );
				b->setText( u8"👀" );
				b->setToolTip( "AutoFocus on Selection" );
				b->setCheckable( true );
				QObject::connect( b, &QAbstractButton::clicked, []( bool checked ){ if( checked ) FocusAllViews(); } );
			}
		}
	}
	{
		auto scroll = new QScrollArea;
		scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
		scroll->setWidgetResizable( true );
		splitter->addWidget( scroll );

		QWidget *containerWidget = new QWidget; // Adding a QLayout to a QScrollArea is not supported, use proxy widget
		g_attributeBox = new QGridLayout( containerWidget );
		g_attributeBox->setAlignment( Qt::AlignmentFlag::AlignTop );
		g_attributeBox->setColumnStretch( 0, 111 );
		g_attributeBox->setColumnStretch( 1, 333 );
		scroll->setWidget( containerWidget ); // widget's layout must be set b4 this!
	}

	g_entityInspector_windowConstructed = true;
	EntityClassList_fill();

	typedef FreeCaller1<const Selectable&, EntityInspector_selectionChanged> EntityInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( EntityInspectorSelectionChangedCaller() );
	GlobalEntityCreator().setKeyValueChangedFunc( EntityInspector_keyValueChanged );

	g_guiSettings.addSplitter( splitter, "EntityInspector/splitter", { 55, 175, 255, 255 } );

	return splitter;
}

class EntityInspector : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	EntityInspector() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			if ( g_entityInspector_windowConstructed ) {
				//globalOutputStream() << "Entity Inspector: realise\n";
				EntityClassList_fill();
			}
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			if ( g_entityInspector_windowConstructed ) {
				//globalOutputStream() << "Entity Inspector: unrealise\n";
				EntityClassList_clear();
			}
		}
	}
};

EntityInspector g_EntityInspector;

#include "preferencesystem.h"
#include "stringio.h"

void EntityInspector_construct(){
	GlobalEntityClassManager().attach( g_EntityInspector );
}

void EntityInspector_destroy(){
	GlobalEntityClassManager().detach( g_EntityInspector );
}
