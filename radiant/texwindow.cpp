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
// Texture Window
//
// Leonardo Zide (leo@lokigames.com)
//

#include "texwindow.h"

#include "debugging/debugging.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"

#include <set>
#include <vector>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QApplication>
#include <QStyle>
#include <QTreeView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QItemDelegate>
#include <QMetaProperty>
#include <QScrollBar>
#include <QSplitter>
#include <QOpenGLWidget>
#include <QTabWidget>

#include "signal/signal.h"
#include "math/vector.h"
#include "texturelib.h"
#include "string/string.h"
#include "shaderlib.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "commandlib.h"
#include "texmanip.h"
#include "textures.h"

#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/cursor.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/mousepresses.h"
#include "gtkutil/guisettings.h"

#include "error.h"
#include "map.h"
#include "qgl.h"
#include "select.h"
#include "brush_primit.h"
#include "brushmanip.h"
#include "patchmanip.h"
#include "plugin.h"
#include "qe3.h"
#include "gtkdlgs.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "findtexturedialog.h"
#include "surfacedialog.h"
#include "patchdialog.h"
#include "groupdialog.h"
#include "preferences.h"
#include "commands.h"

bool string_equal_start( const char* string, StringRange start ){
	return string_equal_n( string, start.data(), start.size() );
}

// sort case insensitively, as it is user friendly
// still preserve unequal names, as it is needed for linux case sensitive FS
struct TextureGroups_compare
{
	bool operator()( const CopiedString& a, const CopiedString& b ) const {
		const int cmp_nocase = string_compare_nocase( a.c_str(), b.c_str() );
		if( cmp_nocase != 0 )
			return cmp_nocase < 0;
		else
			return string_less( a.c_str(), b.c_str() );
	}
};

typedef std::set<CopiedString, TextureGroups_compare> TextureGroups;

void TextureGroups_addWad( TextureGroups& groups, const char* archive ){
	if ( path_extension_is( archive, "wad" ) ) {
#if 1
		groups.insert( archive );
#else
		CopiedString archiveBaseName( PathFilename( archive ) );
		groups.insert( archiveBaseName );
#endif
	}
}
typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addWad> TextureGroupsAddWadCaller;

void TextureGroups_addShader( TextureGroups& groups, const char* shaderName ){
	const char* texture = path_make_relative( shaderName, "textures/" );
	if ( texture != shaderName ) {
		const char* last = path_remove_directory( texture );
		if ( !string_empty( last ) ) {
			groups.insert( CopiedString( StringRange( texture, --last ) ) );
		}
	}
}
typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addShader> TextureGroupsAddShaderCaller;

void TextureGroups_addDirectory( TextureGroups& groups, const char* directory ){
	groups.insert( directory );
}
typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addDirectory> TextureGroupsAddDirectoryCaller;

namespace
{
bool g_TextureBrowser_shaderlistOnly = false;
bool g_TextureBrowser_fixedSize = true;
bool g_TextureBrowser_filterNotex = false;
bool g_TextureBrowser_enableAlpha = false;
bool g_TextureBrowser_filter_searchFromStart = false;
}


enum StartupShaders
{
	STARTUPSHADERS_NONE = 0,
	STARTUPSHADERS_COMMON,
};


class TextureBrowser
{
	int m_originy;
	int m_nTotalHeight;
public:
	int m_width, m_height;

	CopiedString m_shader;    // current shader

	QWidget* m_parent;
	QOpenGLWidget* m_gl_widget;
	QScrollBar* m_texture_scroll;
	QTabWidget* m_tabs;
	QTreeView* m_treeView;
	QStandardItemModel* m_treeViewModel;
	QListWidget* m_tagsListWidget;
	QMenu* m_tagsMenu;
	QAction* m_shader_info_item{};
	QLineEdit* m_filter_entry;
	QAction* m_filter_action;
	CopiedString m_filter_string;

	std::set<CopiedString> m_all_tags;
	std::vector<CopiedString> m_copied_tags;
	std::set<CopiedString> m_found_shaders;

	ToggleItem m_hideunused_item;
	ToggleItem m_showshaders_item;
	ToggleItem m_showtextures_item;
	ToggleItem m_showshaderlistonly_item;
	ToggleItem m_fixedsize_item;
	ToggleItem m_filternotex_item;
	ToggleItem m_enablealpha_item;
	ToggleItem m_tags_item;
	ToggleItem m_filter_searchFromStart_item;

	bool m_heightChanged;
	bool m_originInvalid;

	DeferredAdjustment m_scrollAdjustment;
	FreezePointer m_freezePointer;

	Vector3 m_color_textureback;
	// the increment step we use against the wheel mouse
	int m_mouseWheelScrollIncrement;
	std::size_t m_textureScale;

	bool m_showShaders;
	bool m_showTextures;
	bool m_showTextureScrollbar;
	StartupShaders m_startupShaders;
	// if true, the texture window will only display in-use shaders
	// if false, all the shaders in memory are displayed
	bool m_hideUnused;
	bool m_searchedTags;    // flag to show m_found_shaders
	bool m_tags;            // whether to show tags gui
	bool m_move_started;
	// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
	int m_uniformTextureSize;
	int m_uniformTextureMinSize;

	bool m_hideNonShadersInCommon;

	static bool wads;

	TextureBrowser() :
		m_texture_scroll( 0 ),
		m_hideunused_item( BoolExportCaller( m_hideUnused ) ),
		m_showshaders_item( BoolExportCaller( m_showShaders ) ),
		m_showtextures_item( BoolExportCaller( m_showTextures ) ),
		m_showshaderlistonly_item( BoolExportCaller( g_TextureBrowser_shaderlistOnly ) ),
		m_fixedsize_item( BoolExportCaller( g_TextureBrowser_fixedSize ) ),
		m_filternotex_item( BoolExportCaller( g_TextureBrowser_filterNotex ) ),
		m_enablealpha_item( BoolExportCaller( g_TextureBrowser_enableAlpha ) ),
		m_tags_item( BoolExportCaller( m_tags ) ),
		m_filter_searchFromStart_item( BoolExportCaller( g_TextureBrowser_filter_searchFromStart ) ),
		m_heightChanged( true ),
		m_originInvalid( true ),
		m_scrollAdjustment( [this]( int value ){
			//globalOutputStream() << "vertical scroll\n";
			setOriginY( -value );
		} ),
		m_color_textureback( 0.25f, 0.25f, 0.25f ),
		m_mouseWheelScrollIncrement( 64 ),
		m_textureScale( 50 ),
		m_showShaders( true ),
		m_showTextures( true ),
		m_showTextureScrollbar( true ),
		m_startupShaders( STARTUPSHADERS_NONE ),
		m_hideUnused( false ),
		m_searchedTags( false ),
		m_tags( false ),
		m_move_started( false ),
		m_uniformTextureSize( 160 ),
		m_uniformTextureMinSize( 48 ),
		m_hideNonShadersInCommon( true ){
	}
	void queueDraw() const {
		if ( m_gl_widget != nullptr )
			widget_queue_draw( *m_gl_widget );
	}
	void draw();
	void setOriginY( int originy ){
		m_originy = originy;
		clampOriginY();
		updateScroll();
		queueDraw();
	}
private:
	void clampOriginY(){
		const int minOrigin = std::min( m_height - totalHeight(), 0 );
		m_originy = std::clamp( m_originy, minOrigin, 0 );
	}
	void evaluateHeight();
	int totalHeight(){
		evaluateHeight();
		return m_nTotalHeight;
	}
public:
	int getOriginY(){
		if ( m_originInvalid ) {
			m_originInvalid = false;
			clampOriginY();
			updateScroll();
		}
		return m_originy;
	}
	void heightChanged(){
		m_heightChanged = true;
		updateScroll();
		queueDraw();
	}
	void updateScroll(){
		if ( m_showTextureScrollbar ) {
			const int total_height = std::max( totalHeight(), m_height );

			QScrollBar* s = m_texture_scroll;
			s->setMinimum( 0 );
			s->setMaximum( total_height - m_height );
			s->setValue( - getOriginY() );
			s->setPageStep( m_height );
			s->setSingleStep( 20 );
		}
	}
	// Return the display width of a texture in the texture browser
	auto getTextureWH( const qtexture_t* tex ) const {
		// Don't use uniform size
		int W = std::max( std::size_t( 1 ), tex->width * m_textureScale / 100 );
		int H = std::max( std::size_t( 1 ), tex->height * m_textureScale / 100 );

		if ( g_TextureBrowser_fixedSize ){
			if	( W >= H ) {
				// Texture is square, or wider than it is tall
				if ( W > m_uniformTextureSize ){
					H = m_uniformTextureSize * H / W;
					W = m_uniformTextureSize;
				}
				else if ( W < m_uniformTextureMinSize ){
					H = m_uniformTextureMinSize * H / W;
					W = m_uniformTextureMinSize;
				}
			}
			else {
				// Texture taller than it is wide
				if ( H > m_uniformTextureSize ){
					W = m_uniformTextureSize * W / H;
					H = m_uniformTextureSize;
				}
				else if ( H < m_uniformTextureMinSize ){
					W = m_uniformTextureMinSize * W / H;
					H = m_uniformTextureMinSize;
				}
			}
		}
		return std::pair( W, H );
	}
};
bool TextureBrowser::wads = false;

static TextureBrowser g_TexBro;

void ( *TextureBrowser_textureSelected )( const char* shader );


inline const char* TextureBrowser_getCommonShadersName(){
	static const char* const value = string_empty( g_pGameDescription->getKeyValue( "common_shaders_name" ) )
	                                             ? "Common"
								                 : g_pGameDescription->getKeyValue( "common_shaders_name" );
	return value;
}

inline const char* TextureBrowser_getCommonShadersDir(){
	static const char* const value = string_empty( g_pGameDescription->getKeyValue( "common_shaders_dir" ) )
	                                             ? "common/"
								                 : g_pGameDescription->getKeyValue( "common_shaders_dir" );
	return value;
}

inline int TextureBrowser_fontHeight(){
	return GlobalOpenGL().m_font->getPixelHeight();
}

const char* TextureBrowser_GetSelectedShader(){
	return g_TexBro.m_shader.c_str();
}

void TextureBrowser_SetStatus( const char* name ){
	IShader* shader = QERApp_Shader_ForName( name );
	qtexture_t* q = shader->getTexture();
	const auto strTex = StringStream( ( string_equal_prefix_nocase( name, "textures/" )? name + 9 : name ),
	                                  " W: ", q->width,
	                                  " H: ", q->height );
	shader->DecRef();
	g_pParentWnd->SetStatusText( c_status_texture, strTex );
}

void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name );
void TextureBrowser_tagsSetCheckboxesForShader( const char *shader );

void TextureBrowser_SetSelectedShader( TextureBrowser& textureBrowser, const char* shader ){
	textureBrowser.m_shader = shader;
	TextureBrowser_SetStatus( shader );
	TextureBrowser_Focus( textureBrowser, shader );

	if ( FindTextureDialog_isOpen() ) {
		FindTextureDialog_selectTexture( shader );
	}

	// disable the menu item "shader info" if no shader was selected
	if ( textureBrowser.m_shader_info_item != nullptr ){
		IShader* ishader = QERApp_Shader_ForName( shader );
		CopiedString filename = ishader->getShaderFileName();

		textureBrowser.m_shader_info_item->setDisabled( filename.empty() );

		ishader->DecRef();
	}

	if( textureBrowser.m_tabs->currentIndex() == 1 )
		TextureBrowser_tagsSetCheckboxesForShader( shader );
}

void TextureBrowser_SetSelectedShader( const char* shader ){
	TextureBrowser_SetSelectedShader( g_TexBro, shader );
}


CopiedString g_TextureBrowser_currentDirectory;

/*
   ============================================================================

   TEXTURE LAYOUT

   TTimo: now based on a rundown through all the shaders
   NOTE: we expect the Active shaders count doesn't change during a Texture_StartPos .. Texture_NextPos cycle
   otherwise we may need to rely on a list instead of an array storage
   ============================================================================
 */

struct TextureLayout
{
// texture layout functions
// TTimo: now based on shaders
	int current_x = 8;
	int current_y = -4;
	int current_row = 0;

	auto nextPos( const TextureBrowser& textureBrowser, qtexture_t* current_texture ){
		const auto [nWidth, nHeight] = textureBrowser.getTextureWH( current_texture );
		if ( current_x + nWidth > textureBrowser.m_width - 8 && current_row ) { // go to the next row unless the texture is the first on the row
			current_x = 8;
			current_y -= current_row + TextureBrowser_fontHeight() + 1;//+4
			current_row = 0;
		}

		const int x = current_x;
		const int y = current_y;

		// Is our texture larger than the row? If so, grow the
		// row height to match it

		if ( current_row < nHeight ) {
			current_row = nHeight;
		}

		// never go less than 96, or the names get all crunched up
		current_x += std::max( nWidth, 96 ) + 8;

		return std::pair( x, y );
	}
};

bool Texture_filtered( const char* name, const TextureBrowser& textureBrowser ){
	const char* filter = textureBrowser.m_filter_string.c_str();
	if( string_empty( filter ) ){
		return false;
	}
	if( g_TextureBrowser_filter_searchFromStart ){
		if( string_equal_prefix_nocase( name, filter ) ){
			return false;
		}
	}
	else{
		if( string_in_string_nocase( name, filter ) != 0 ){
			return false;
		}
	}
	return true;
}

CopiedString g_notex;
CopiedString g_shadernotex;

// if texture_showinuse jump over non in-use textures
/*
bool show_shaders, bool show_textures, bool hideUnused, bool hideNonShadersInCommon
textureBrowser.m_showShaders, textureBrowser.m_showTextures, textureBrowser.m_hideUnused, textureBrowser.m_hideNonShadersInCommon
*/
bool Texture_IsShown( IShader* shader, const TextureBrowser& textureBrowser ){
	// filter notex / shadernotex images
	if ( g_TextureBrowser_filterNotex && ( string_equal( g_notex.c_str(), shader->getTexture()->name ) || string_equal( g_shadernotex.c_str(), shader->getTexture()->name ) ) ) {
		return false;
	}

	if ( !shader_equal_prefix( shader->getName(), "textures/" ) ) {
		return false;
	}

	if ( !textureBrowser.m_showShaders && !shader->IsDefault() ) {
		return false;
	}

	if ( !textureBrowser.m_showTextures && shader->IsDefault() ) {
		return false;
	}

	if ( textureBrowser.m_hideUnused && !shader->IsInUse() ) {
		return false;
	}

	if( textureBrowser.m_hideNonShadersInCommon && shader->IsDefault() && !shader->IsInUse() //&& g_TextureBrowser_currentDirectory != ""
	 && shader_equal_prefix( shader_get_textureName( shader->getName() ), TextureBrowser_getCommonShadersDir() ) ){
		return false;
	}

	if ( textureBrowser.m_searchedTags ) {
		return textureBrowser.m_found_shaders.contains( shader->getName() );
	}
	else {
		if ( !shader_equal_prefix( shader_get_textureName( shader->getName() ), g_TextureBrowser_currentDirectory.c_str() ) ) {
			return false;
		}
	}

	if( Texture_filtered( path_get_filename_start( shader->getName() ), textureBrowser ) ){
		return false;
	}

	return true;
}

void TextureBrowser::evaluateHeight(){
	if ( m_heightChanged ) {
		m_heightChanged = false;

		m_nTotalHeight = 0;

		TextureLayout layout;
		for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
		{
			IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

			if ( Texture_IsShown( shader, *this ) ) {
				layout.nextPos( *this, shader->getTexture() );
				const auto [nWidth, nHeight] = getTextureWH( shader->getTexture() );
				m_nTotalHeight = std::max( m_nTotalHeight, abs( layout.current_y ) + TextureBrowser_fontHeight() + nHeight + 4 );
			}
		}
	}
}


Signal0 g_activeShadersChangedCallbacks;

void TextureBrowser_addActiveShadersChangedCallback( const SignalHandler& handler ){
	g_activeShadersChangedCallbacks.connectLast( handler );
}

class ShadersObserver : public ModuleObserver
{
	Signal0 m_realiseCallbacks;
public:
	void realise(){
		m_realiseCallbacks();
	}
	void unrealise(){
	}
	void insert( const SignalHandler& handler ){
		m_realiseCallbacks.connectLast( handler );
	}
};

namespace
{
ShadersObserver g_ShadersObserver;
}

void TextureBrowser_addShadersRealiseCallback( const SignalHandler& handler ){
	g_ShadersObserver.insert( handler );
}

void TextureBrowser_activeShadersChanged( TextureBrowser& textureBrowser ){
	textureBrowser.heightChanged();
	textureBrowser.m_originInvalid = true;

	g_activeShadersChangedCallbacks();
}

void TextureBrowser_importShowScrollbar( TextureBrowser& textureBrowser, bool value ){
	textureBrowser.m_showTextureScrollbar = value;
	if ( textureBrowser.m_texture_scroll != 0 ) {
		textureBrowser.m_texture_scroll->setVisible( textureBrowser.m_showTextureScrollbar );
		textureBrowser.updateScroll();
	}
}
typedef ReferenceCaller<TextureBrowser, void(bool), TextureBrowser_importShowScrollbar> TextureBrowserImportShowScrollbarCaller;


/*
   ==============
   TextureBrowser_ShowDirectory
   relies on texture_directory global for the directory to use
   1) Load the shaders for the given directory
   2) Scan the remaining texture, load them and assign them a default shader (the "noshader" shader)
   NOTE: when writing a texture plugin, or some texture extensions, this function may need to be overridden, and made
   available through the IShaders interface
   NOTE: for texture window layout:
   all shaders are stored with alphabetical order after load
   previously loaded and displayed stuff is hidden, only in-use and newly loaded is shown
   ( the GL textures are not flushed though)
   ==============
 */

inline bool texture_name_ignore( const char* name ){
	const auto temp = StringStream<64>( LowerCase( name ) );

	return
	    string_equal_suffix( temp, ".specular" ) ||
	    string_equal_suffix( temp, ".glow" ) ||
	    string_equal_suffix( temp, ".bump" ) ||
	    string_equal_suffix( temp, ".diffuse" ) ||
	    string_equal_suffix( temp, ".blend" ) ||
	    string_equal_suffix( temp, ".alpha" ) ||
	    string_equal_suffix( temp, "_norm" ) ||
	    string_equal_suffix( temp, "_bump" ) ||
	    string_equal_suffix( temp, "_glow" ) ||
	    string_equal_suffix( temp, "_gloss" ) ||
	    string_equal_suffix( temp, "_pants" ) ||
	    string_equal_suffix( temp, "_shirt" ) ||
	    string_equal_suffix( temp, "_reflect" ) ||
	    string_equal_suffix( temp, "_alpha" ) ||
	    0;
}

class LoadShaderVisitor : public Archive::Visitor
{
public:
	void visit( const char* name ){
		IShader* shader = QERApp_Shader_ForName( CopiedString( PathExtensionless( name ) ).c_str() );
		shader->DecRef();
	}
};

void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused );

QWidget* g_page_textures;

void TextureBrowser_toggleShow(){
	GroupDialog_showPage( g_page_textures );
}


void TextureBrowser_updateTitle(){
	GroupDialog_updatePageTitle( g_page_textures );
}



class TextureCategoryLoadShader
{
	const char* m_directory;
	std::size_t& m_count;
public:
	using func = void(const char *);

	TextureCategoryLoadShader( const char* directory, std::size_t& count )
		: m_directory( directory ), m_count( count ){
		m_count = 0;
	}
	void operator()( const char* name ) const {
		if ( shader_equal_prefix( name, "textures/" )
		  && shader_equal_prefix( name + string_length( "textures/" ), m_directory ) ) {
			++m_count;
			// request the shader, this will load the texture if needed
			// this Shader_ForName call is a kind of hack
			IShader *pFoo = QERApp_Shader_ForName( name );
			pFoo->DecRef();
		}
	}
};

void TexturePath_loadTexture( const char* name ){
	if ( texture_name_ignore( name ) ) {
		return;
	}

	if ( !shader_valid( name ) ) {
		globalWarningStream() << "Skipping invalid texture name: [" << name << "]\n";
		return;
	}

	// if a texture is already in use to represent a shader, ignore it
	IShader* shader = QERApp_Shader_ForName( name );
	shader->DecRef();
}
void TextureDirectory_loadTexture( const char* directory, const char* texture ){
	TexturePath_loadTexture( StringStream<64>( directory, PathExtensionless( texture ) ) );
}
typedef ConstPointerCaller<char, void(const char*), TextureDirectory_loadTexture> TextureDirectoryLoadTextureCaller;

class LoadTexturesByTypeVisitor : public ImageModules::Visitor
{
	const char* m_dirstring;
public:
	LoadTexturesByTypeVisitor( const char* dirstring )
		: m_dirstring( dirstring ){
	}
	void visit( const char* minor, const _QERPlugImageTable& table ) const {
		GlobalFileSystem().forEachFile( m_dirstring, minor, TextureDirectoryLoadTextureCaller( m_dirstring ) );
	}
};

void TextureBrowser_ShowDirectory( const char* directory ){
	g_TexBro.m_searchedTags = false;
	if ( TextureBrowser::wads ) {
		Archive* archive = GlobalFileSystem().getArchive( directory );
		//ASSERT_NOTNULL( archive );
		if( archive ){
			globalOutputStream() << "Loading " << makeQuoted( directory ) << " wad file.\n";
			LoadShaderVisitor visitor;
			archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, 0 ), "textures/" );
		}
		else{
			globalErrorStream() << "Attempted to load " << makeQuoted( directory ) << " wad file.\n";
		}
	}
	else
	{
		g_TextureBrowser_currentDirectory = directory;

		std::size_t shaders_count;
		GlobalShaderSystem().foreachShaderName( makeCallback( TextureCategoryLoadShader( directory, shaders_count ) ) );
		globalOutputStream() << "Showing " << shaders_count << " shaders.\n";

		if ( g_pGameDescription->mGameType != "doom3" ) {
			// load remaining texture files
			Radiant_getImageModules().foreachModule( LoadTexturesByTypeVisitor( StringStream<64>( "textures/", directory ) ) );
		}
	}

	TextureBrowser_SetHideUnused( g_TexBro, false );
	g_TexBro.setOriginY( 0 );
	TextureBrowser_updateTitle();
}


void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused ){
	textureBrowser.m_hideUnused = hideUnused;

	textureBrowser.m_hideunused_item.update();

	textureBrowser.heightChanged();
	textureBrowser.m_originInvalid = true;
}

void TextureBrowser_ShowStartupShaders(){
	if ( g_TexBro.m_startupShaders == STARTUPSHADERS_COMMON ) {
		TextureBrowser_ShowDirectory( TextureBrowser_getCommonShadersDir() );
	}
}


//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name ){
	TextureLayout layout;
	// scroll origin so the texture is completely on screen

	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser ) ) {
			continue;
		}

		const auto [ x, y ] = layout.nextPos( textureBrowser, shader->getTexture() );
		qtexture_t* q = shader->getTexture();
		if ( !q ) {
			break;
		}

		// we have found when texdef->name and the shader name match
		// NOTE: as everywhere else for our comparisons, we are not case sensitive
		if ( shader_equal( name, shader->getName() ) ) {
			//int textureHeight = (int)( q->height * ( (float)textureBrowser.m_textureScale / 100 ) ) + 2 * TextureBrowser_fontHeight();
			auto [textureWidth, textureHeight] = textureBrowser.getTextureWH( q );
			textureHeight += 2 * TextureBrowser_fontHeight();


			int originy = textureBrowser.getOriginY();
			if ( y > originy ) {
				originy = y + 4;
			}

			if ( y - textureHeight < originy - textureBrowser.m_height ) {
				originy = ( y - textureHeight ) + textureBrowser.m_height;
			}

			textureBrowser.setOriginY( originy );
			return;
		}
	}
}

IShader* Texture_At( TextureBrowser& textureBrowser, int mx, int my ){
	my = textureBrowser.getOriginY() - my - 1;

	TextureLayout layout;
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser ) ) {
			continue;
		}

		const auto [ x, y ] = layout.nextPos( textureBrowser, shader->getTexture() );
		qtexture_t  *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		const auto [nWidth, nHeight] = textureBrowser.getTextureWH( q );
		if ( mx > x && mx - x < nWidth
		  && my < y && y - my < nHeight + TextureBrowser_fontHeight() ) {
			return shader;
		}
	}

	return 0;
}

/*
   ==============
   SelectTexture

   By mouse click
   ==============
 */
void SelectTexture( TextureBrowser& textureBrowser, int mx, int my, bool texturizeSelection ){
	IShader* shader = Texture_At( textureBrowser, mx, my );
	if ( shader != 0 ) {
		TextureBrowser_SetSelectedShader( textureBrowser, shader->getName() );
		TextureBrowser_textureSelected( shader->getName() );

		if ( !FindTextureDialog_isOpen() && !texturizeSelection ) {
			Select_SetShader_Undo( shader->getName() );
		}
	}
}

/*
   ============================================================================

   MOUSE ACTIONS

   ============================================================================
 */

void TextureBrowser_Tracking_MouseUp( TextureBrowser& textureBrowser ){
	if( textureBrowser.m_move_started ){
		textureBrowser.m_move_started = false;
		textureBrowser.m_freezePointer.unfreeze_pointer( false );
	}
}

void TextureBrowser_Tracking_MouseDown( TextureBrowser& textureBrowser ){
	TextureBrowser_Tracking_MouseUp( textureBrowser );
	textureBrowser.m_move_started = true;
	textureBrowser.m_freezePointer.freeze_pointer( textureBrowser.m_gl_widget,
		[&textureBrowser]( int x, int y, const QMouseEvent *event ){
			if ( y != 0 ) {
				const int scale = event->modifiers().testFlag( Qt::KeyboardModifier::ShiftModifier )? 4 : 1;
				const int originy = textureBrowser.getOriginY() + y * scale;
				textureBrowser.setOriginY( originy );
			}
		},
		[&textureBrowser](){
			TextureBrowser_Tracking_MouseUp( textureBrowser );
		} );
}

void TextureBrowser_ViewShader( TextureBrowser& textureBrowser, Qt::KeyboardModifiers modifiers, int pointx, int pointy ){
	IShader* shader = Texture_At( textureBrowser, pointx, pointy );
	if ( shader != 0 ) {
		if ( shader->IsDefault() ) {
			globalWarningStream() << shader->getName() << " is not a shader, it's a texture.\n";
		}
		else{
			DoShaderView( shader->getShaderFileName(), shader->getName(), modifiers.testFlag( Qt::KeyboardModifier::ControlModifier ) );
		}
	}
}

/*
   ============================================================================

   DRAWING

   ============================================================================
 */

/*
   ============
   TTimo: relying on the shaders list to display the textures
   we must query all qtexture_t* to manage and display through the IShaders interface
   this allows a plugin to completely override the texture system
   ============
 */
void TextureBrowser::draw(){
	evaluateHeight();
	const int fontHeight = TextureBrowser_fontHeight();
	const int fontDescent = GlobalOpenGL().m_font->getPixelDescent();
	const int originy = getOriginY();

	gl().glClearColor( m_color_textureback[0],
	                   m_color_textureback[1],
	                   m_color_textureback[2],
	                   0 );
	gl().glViewport( 0, 0, m_width, m_height );
	gl().glMatrixMode( GL_PROJECTION );
	gl().glLoadIdentity();

	gl().glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	gl().glDisable( GL_DEPTH_TEST );
	gl().glDisable( GL_MULTISAMPLE );
	if ( g_TextureBrowser_enableAlpha ) {
		gl().glEnable( GL_BLEND );
		gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else {
		gl().glDisable( GL_BLEND );
	}

	gl().glOrtho( 0, m_width, originy - m_height, originy, -100, 100 );
	gl().glEnable( GL_TEXTURE_2D );

	gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	TextureLayout layout;
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, *this ) ) {
			continue;
		}

		const auto [ x, y ] = layout.nextPos( *this, shader->getTexture() );
		qtexture_t *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		const auto [nWidth, nHeight] = getTextureWH( q );

		// Is this texture visible?
		if ( ( y - nHeight - fontHeight < originy )
		  && ( y > originy - m_height ) ) {
			gl().glLineWidth( 1 );
			gl().glDisable( GL_TEXTURE_2D );
			const float xf = x;
			const float yf = y - fontHeight;
			float xfMax = xf + 1.5 + nWidth;
			float xfMin = xf - 1.5;
			float yfMax = yf + 1.5;
			float yfMin = yf - nHeight - 1.5;
			#define TEXBRO_RENDER_BORDER \
				gl().glBegin( GL_LINE_LOOP ); \
				gl().glVertex2f( xfMin, yfMax ); \
				gl().glVertex2f( xfMin, yfMin ); \
				gl().glVertex2f( xfMax, yfMin ); \
				gl().glVertex2f( xfMax, yfMax ); \
				gl().glEnd();

			//selected texture
			if ( shader_equal( m_shader.c_str(), shader->getName() ) ) {
				gl().glLineWidth( 2 );
				gl().glColor3f( 1, 0, 0 );
				xfMax += .5;
				xfMin -= .5;
				yfMax += .5;
				yfMin -= .5;
				TEXBRO_RENDER_BORDER
			}
			// highlight in-use textures
			else if ( !m_hideUnused && shader->IsInUse() ) {
				gl().glColor3f( 0.5, 1, 0.5 );
				TEXBRO_RENDER_BORDER
			}
			// shader white border:
			else if ( !shader->IsDefault() ) {
				gl().glColor3f( 1, 1, 1 );
				TEXBRO_RENDER_BORDER
			}

			// shader stipple:
			if ( !shader->IsDefault() ) {
				gl().glEnable( GL_LINE_STIPPLE );
				gl().glLineStipple( 1, 0xF000 );
				gl().glColor3f( 0, 0, 0 );
				TEXBRO_RENDER_BORDER
				gl().glDisable( GL_LINE_STIPPLE );
			}

			// draw checkerboard for transparent textures
			if ( g_TextureBrowser_enableAlpha )
			{
				gl().glBegin( GL_QUADS );
				for ( int i = 0; i < nHeight; i += 8 )
					for ( int j = 0; j < nWidth; j += 8 )
					{
						const unsigned char color = ( i + j ) / 8 % 2 ? 0x66 : 0x99;
						gl().glColor3ub( color, color, color );
						const int left = j;
						const int right = std::min( j + 8, nWidth );
						const int top = i;
						const int bottom = std::min( i + 8, nHeight );
						gl().glVertex2i( x + right, y - nHeight - fontHeight + top );
						gl().glVertex2i( x + left,  y - nHeight - fontHeight + top );
						gl().glVertex2i( x + left,  y - nHeight - fontHeight + bottom );
						gl().glVertex2i( x + right, y - nHeight - fontHeight + bottom );
					}
				gl().glEnd();
			}

			// Draw the texture
			gl().glEnable( GL_TEXTURE_2D );
			gl().glBindTexture( GL_TEXTURE_2D, q->texture_number );
			GlobalOpenGL_debugAssertNoErrors();
			gl().glColor3f( 1, 1, 1 );
			gl().glBegin( GL_QUADS );
			gl().glTexCoord2i( 0, 0 );
			gl().glVertex2i( x, y - fontHeight );
			gl().glTexCoord2i( 1, 0 );
			gl().glVertex2i( x + nWidth, y - fontHeight );
			gl().glTexCoord2i( 1, 1 );
			gl().glVertex2i( x + nWidth, y - fontHeight - nHeight );
			gl().glTexCoord2i( 0, 1 );
			gl().glVertex2i( x, y - fontHeight - nHeight );
			gl().glEnd();

			// draw the texture name
//			gl().glDisable( GL_TEXTURE_2D );
//			gl().glColor3f( 1, 1, 1 ); //already set

			gl().glRasterPos2i( x, y - fontHeight - fontDescent + 3 );//+5

			// don't draw the directory name
			const char* name = shader->getName();
			name += strlen( name );
			while ( name != shader->getName() && *( name - 1 ) != '/' && *( name - 1 ) != '\\' )
				name--;

			GlobalOpenGL().drawString( name );
		}
	}

	// reset the current texture
	gl().glBindTexture( GL_TEXTURE_2D, 0 );
	gl().glDisable( GL_BLEND );
	//qglFinish();
}


void TextureBrowser_setScale( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_textureScale = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	textureBrowser.queueDraw();
}

void TextureBrowser_setUniformSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	textureBrowser.queueDraw();
}

void TextureBrowser_setUniformMinSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureMinSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	textureBrowser.queueDraw();
}

void TextureBrowser_ToggleHideUnused(){
	TextureBrowser_SetHideUnused( g_TexBro, !g_TexBro.m_hideUnused );
}

void TextureGroups_constructTreeModel( TextureGroups groups, QStandardItemModel* model ){
	auto root = model->invisibleRootItem();

	TextureGroups::const_iterator i = groups.begin();
	while ( i != groups.end() )
	{
		const char* dirName = ( *i ).c_str();
		const char* firstUnderscore = strchr( dirName, '_' );
		StringRange dirRoot( dirName, ( firstUnderscore == 0 ) ? dirName : firstUnderscore + 1 );

		TextureGroups::const_iterator next = std::next( i );
		if ( firstUnderscore != 0
		  && next != groups.end()
		  && string_equal_start( ( *next ).c_str(), dirRoot ) ) {
			auto subroot = new QStandardItem( CopiedString( StringRange( dirName, firstUnderscore ) ).c_str() );
			root->appendRow( subroot );

			// keep going...
			while ( i != groups.end() && string_equal_start( ( *i ).c_str(), dirRoot ) )
			{
				auto item = new QStandardItem( ( *i ).c_str() );
				item->setData( ( *i ).c_str(), Qt::ItemDataRole::ToolTipRole );
				subroot->appendRow( item );
				++i;
			}
		}
		else
		{
			auto item = new QStandardItem( dirName );
			item->setData( dirName, Qt::ItemDataRole::ToolTipRole );
			root->appendRow( item );
			++i;
		}
	}
}

void TextureGroups_constructTreeModel_childless( TextureGroups groups, QStandardItemModel* model ){
	auto root = model->invisibleRootItem();

	TextureGroups::const_iterator i = groups.begin();
	while ( i != groups.end() )
	{
		const char* dirName = ( *i ).c_str();
		const char* pakName = strrchr( dirName, '/' );
		const char* pakNameEnd = strrchr( dirName, '.' );
		ASSERT_MESSAGE( pakName != 0 && pakNameEnd != 0 && pakNameEnd > pakName, "interesting wad path" );
		{
			auto item = new QStandardItem( CopiedString( StringRange( pakName + 1, pakNameEnd ) ).c_str() );
			item->setData( dirName, Qt::ItemDataRole::ToolTipRole );
			root->appendRow( item );
			++i;
		}
	}
}

void TextureGroups_constructTreeView( TextureGroups& groups ){
	if ( TextureBrowser::wads ) {
		GlobalFileSystem().forEachArchive( TextureGroupsAddWadCaller( groups ) );
	}
	else
	{
		// scan texture dirs and pak files only if not restricting to shaderlist
		if ( g_pGameDescription->mGameType != "doom3" && !g_TextureBrowser_shaderlistOnly ) {
			GlobalFileSystem().forEachDirectory( "textures/", TextureGroupsAddDirectoryCaller( groups ) );
		}

		GlobalShaderSystem().foreachShaderName( TextureGroupsAddShaderCaller( groups ) );
	}
}

void TextureBrowser_constructTreeStore(){
	TextureGroups groups;
	TextureGroups_constructTreeView( groups );

	auto model = new QStandardItemModel( g_TexBro.m_treeView ); //. ? delete old or clear() & reuse

	// store display name in column #0 and load path in data( Qt::ItemDataRole::ToolTipRole )
	// tooltips are only wanted for TextureBrowser::wads, but well
	if( !TextureBrowser::wads )
		TextureGroups_constructTreeModel( groups, model );
	else
		TextureGroups_constructTreeModel_childless( groups, model );

	g_TexBro.m_treeView->setModel( model );
}

void TreeView_onRowActivated( const QModelIndex& index ){
	auto dirName = index.data( Qt::ItemDataRole::ToolTipRole ).toByteArray();
	if( !dirName.isEmpty() ){ // empty = directory group root
		g_TexBro.m_searchedTags = false;

		if ( !TextureBrowser::wads ) {
			dirName.append( '/' );
		}

		ScopeDisableScreenUpdates disableScreenUpdates( dirName, "Loading Textures" );
		TextureBrowser_ShowDirectory( dirName.data() );
		g_TexBro.queueDraw();
		//deactivate, so SPACE and RETURN wont be broken for 2d
		g_TexBro.m_treeView->clearFocus();
	}
}

class TexBro_QTreeView : public QTreeView
{
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			event->accept();
			return true;
		}
		return QTreeView::event( event );
	}
};

void TextureBrowser_createTreeViewTree(){
	g_TexBro.m_treeView = new TexBro_QTreeView;
	g_TexBro.m_treeView->setHeaderHidden( true );
	g_TexBro.m_treeView->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
	g_TexBro.m_treeView->setUniformRowHeights( true ); // optimization
	g_TexBro.m_treeView->setFocusPolicy( Qt::FocusPolicy::ClickFocus );
	g_TexBro.m_treeView->setExpandsOnDoubleClick( false );
	g_TexBro.m_treeView->header()->setStretchLastSection( false ); // non greedy column sizing; + QHeaderView::ResizeMode::ResizeToContents = no text elision 🤷‍♀️
	g_TexBro.m_treeView->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );

	QObject::connect( g_TexBro.m_treeView, &QAbstractItemView::activated, TreeView_onRowActivated );

	TextureBrowser_constructTreeStore();
}

static QMenu* TextureBrowser_constructViewMenu(){
	QMenu *menu = new QMenu( "View" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_check_menu_item_with_mnemonic( menu, "Hide Unused", "ShowInUse" );
	create_menu_item_with_mnemonic( menu, "Show All", "ShowAllTextures" );
	menu->addSeparator();


	// we always want to show shaders but don't want a "Show Shaders" menu for doom3 and .wad file games
	if ( g_pGameDescription->mGameType == "doom3" || TextureBrowser::wads ) {
		g_TexBro.m_showShaders = true;
	}
	else
	{
		create_check_menu_item_with_mnemonic( menu, "Show Shaders", "ToggleShowShaders" );
		create_check_menu_item_with_mnemonic( menu, "Show Textures", "ToggleShowTextures" );
		menu->addSeparator();
	}

	if ( g_pGameDescription->mGameType != "doom3" && !TextureBrowser::wads ) {
		create_check_menu_item_with_mnemonic( menu, "ShaderList Only", "ToggleShowShaderlistOnly" );
	}
	if ( !TextureBrowser::wads ) {
		create_check_menu_item_with_mnemonic( menu, "Hide Image Missing", "FilterNotex" );
		menu->addSeparator();
	}

	create_check_menu_item_with_mnemonic( menu, "Fixed Size", "FixedSize" );
	create_check_menu_item_with_mnemonic( menu, "Transparency", "EnableAlpha" );

	menu->addSeparator();
	create_check_menu_item_with_mnemonic( menu, "Tags GUI", "TagsToggleGui" );

	if ( !TextureBrowser::wads ) {
		menu->addSeparator();
		g_TexBro.m_shader_info_item = create_menu_item_with_mnemonic( menu, "Shader Info", "ShaderInfo" );
		g_TexBro.m_shader_info_item->setDisabled( true );
	}

	return menu;
}


#include "xml/xmltextags.h"
XmlTagBuilder TagBuilder;


static QMenu* TextureBrowser_constructTagsMenu(){
	auto menu = new QMenu( "Tags" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Add tag", "TagAdd" );
	create_menu_item_with_mnemonic( menu, "Rename tag", "TagRename" );
	create_menu_item_with_mnemonic( menu, "Delete tag", "TagDelete" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Copy tags from selected", "TagCopy" );
	create_menu_item_with_mnemonic( menu, "Paste tags to selected", "TagPaste" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Search tag", "TagSearch" );
	create_menu_item_with_mnemonic( menu, "Search Untagged", "TagSearchUntagged" );

	return menu;
}

inline void TextureBrowser_tagsEnableGui( bool enable ){
	if( enable )
		g_TexBro.m_tabs->addTab( g_TexBro.m_tagsListWidget, "Tags" );
	else
		g_TexBro.m_tabs->removeTab( 1 );
}

class Tags_QListWidget : public QListWidget
{
	using QListWidget::QListWidget;
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			event->accept();
			return true;
		}
		return QListWidget::event( event );
	}
	void contextMenuEvent( QContextMenuEvent *event ) override {
		g_TexBro.m_tagsMenu->popup( event->globalPos() );
	}
};

void TextureBrowser_tagsSetCheckboxesForShader( const char *shader ){
	std::vector<CopiedString> assigned_tags;
	TagBuilder.GetShaderTags( shader, assigned_tags );

	const auto contains = [&assigned_tags]( const char *tag )->bool {
		for( auto it = assigned_tags.cbegin(); it != assigned_tags.cend(); ++it )
			if( string_equal( tag, it->c_str() ) ){
				assigned_tags.erase( it ); // assuming / hoping that tag names are unique, thus can run faster
				return true;
			}
		return false;
	};

	for( int i = 0; i < g_TexBro.m_tagsListWidget->count(); ++i )
	{
		auto item = g_TexBro.m_tagsListWidget->item( i );
		item->setCheckState( contains( item->data( Qt::ItemDataRole::DisplayRole ).toByteArray() )? Qt::CheckState::Checked : Qt::CheckState::Unchecked );
	}
}

void TextureBrowser_tagAssignmentChanged( const QModelIndex &index ){
	const bool assigned = Qt::CheckState::Checked == static_cast<Qt::CheckState>( index.data( Qt::ItemDataRole::CheckStateRole ).toInt() );
	const auto tag = index.data( Qt::ItemDataRole::DisplayRole ).toByteArray();

	if( assigned ){
		if ( !TagBuilder.CheckShaderTag( g_TexBro.m_shader.c_str() ) ) {
			// create a custom shader/texture entry
			IShader* ishader = QERApp_Shader_ForName( g_TexBro.m_shader.c_str() );

			if ( ishader->IsDefault() ) {
				// it's a texture
				TagBuilder.AddShaderNode( g_TexBro.m_shader.c_str(), TextureType::CUSTOM, NodeShaderType::TEXTURE );
			}
			else {
				// it's a shader
				TagBuilder.AddShaderNode( g_TexBro.m_shader.c_str(), TextureType::CUSTOM, NodeShaderType::SHADER );
			}
			ishader->DecRef();
		}
		TagBuilder.AddShaderTag( g_TexBro.m_shader.c_str(), tag, NodeTagType::TAG );
	}
	else{
		TagBuilder.DeleteShaderTag( g_TexBro.m_shader.c_str(), tag );
	}
}

class Tags_QItemDelegate : public QItemDelegate
{
	using QItemDelegate::QItemDelegate;
protected:
	/* track user edit of tag name */
	void setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex &index ) const override {
		const QByteArray propname = editor->metaObject()->userProperty().name();
		const auto newName = propname.isEmpty()? QByteArray() : editor->property( propname ).toByteArray();
		if ( newName.isEmpty() ){
			qt_MessageBox( g_TexBro.m_parent, "New tag name is empty :0", ":o", EMessageBoxType::Error );
		}
		else if( const auto oldName = index.data( Qt::ItemDataRole::DisplayRole ).toByteArray(); oldName != newName ){ // is changed
			if( g_TexBro.m_all_tags.contains( newName.constData() ) ){	// found in existing names
				qt_MessageBox( g_TexBro.m_parent, "New tag name is already taken :0", newName.constData(), EMessageBoxType::Error );
			}
			else{
				TagBuilder.RenameShaderTag( oldName, newName.constData() );

				g_TexBro.m_all_tags.erase( oldName.constData() );
				g_TexBro.m_all_tags.insert( newName.constData() );

				QItemDelegate::setModelData( editor, model, index ); // normal processing
			}
		}
	}
	bool editorEvent( QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index ) override {
		/* let's do some infamous juggling to track user unduced CheckState change */
		if( event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyPress ){
			if( const QVariant value = index.data( Qt::ItemDataRole::CheckStateRole ); value.isValid() ){
				const bool ret = QItemDelegate::editorEvent( event, model, option, index );
				if( ret && value != index.data( Qt::ItemDataRole::CheckStateRole ) ){
					TextureBrowser_tagAssignmentChanged( index );
				}
				return ret;
			}
		}
		return QItemDelegate::editorEvent( event, model, option, index );
	}
};

void TextureBrowser_searchTags(){
	const auto selected = g_TexBro.m_tagsListWidget->selectedItems();

	if ( !selected.empty() ) {
		auto buffer = StringStream( "/root/*/*[tag='" );
		auto tags_searched = StringStream( "[TAGS] " );

		for ( auto it = selected.begin(); it != selected.end(); ++it )
		{
			const auto tag = ( *it )->text().toLatin1();
			buffer << tag.constData();
			tags_searched << tag.constData();
			if ( it + 1 != selected.end() ) {
				buffer << "' and tag='";
				tags_searched << ", ";
			}
		}

		buffer << "']";

		g_TexBro.m_found_shaders.clear(); // delete old list
		TagBuilder.TagSearch( buffer, g_TexBro.m_found_shaders );

		if ( !g_TexBro.m_found_shaders.empty() ) { // found something
			globalOutputStream() << "Found " << g_TexBro.m_found_shaders.size() << " textures and shaders with " << tags_searched << '\n';
			ScopeDisableScreenUpdates disableScreenUpdates( "Searching...", "Loading Textures" );

			for ( const CopiedString& shader : g_TexBro.m_found_shaders )
			{
				TexturePath_loadTexture( shader.c_str() );
			}
		}
		TextureBrowser_SetHideUnused( g_TexBro, false );
		g_TexBro.m_searchedTags = true;
		g_TextureBrowser_currentDirectory = tags_searched;

		g_TexBro.heightChanged();
		g_TexBro.m_originInvalid = true;
		TextureBrowser_updateTitle();

		//deactivate, so SPACE and RETURN wont be broken for 2d
		g_TexBro.m_tagsListWidget->clearFocus();
	}
}

void TextureBrowser_showUntagged(){
	EMessageBoxReturn result = qt_MessageBox( g_TexBro.m_parent,
		"WARNING! This function might need <b>a lot</b> of memory and time.<br>"
		"It shows all textures & shaders indexed by ShaderPlug plugin, but having no tag.<br>"
		"Are you sure you want to use it?",
		"Show Untagged", EMessageBoxType::Warning, eIDYES | eIDNO );

	if ( result == eIDYES ) {
		g_TexBro.m_found_shaders.clear();
		TagBuilder.GetUntagged( g_TexBro.m_found_shaders );

		ScopeDisableScreenUpdates disableScreenUpdates( "Searching untagged textures...", "Loading Textures" );

		for ( const CopiedString& shader : g_TexBro.m_found_shaders )
		{
			TexturePath_loadTexture( shader.c_str() );
		}

		TextureBrowser_SetHideUnused( g_TexBro, false );
		g_TexBro.m_searchedTags = true;
		g_TextureBrowser_currentDirectory = "Untagged";
		g_TexBro.heightChanged();
		g_TexBro.m_originInvalid = true;
		TextureBrowser_updateTitle();
	}
}

void TextureBrowser_checkTagFile(){
	const auto rc_filename = StringStream( LocalRcPath_get(), SHADERTAG_FILE );
	const auto default_filename = StringStream( g_pGameDescription->mGameToolsPath, SHADERTAG_FILE );

	if ( file_exists( rc_filename ) && TagBuilder.OpenXmlDoc( rc_filename ) )
	{
		globalOutputStream() << "Loaded tag file " << rc_filename << ".\n";
	}
	else if ( file_exists( default_filename ) && TagBuilder.OpenXmlDoc( default_filename, rc_filename ) ) // load default tagfile
	{
		globalOutputStream() << "Loaded default tag file " << default_filename << ".\n";
	}
	else
	{
		// globalWarningStream() << "Unable to find default tag file " << default_filename << ". No tag support. Plugins -> ShaderPlug -> Create tag file: to start using texture tags\n";
		const bool ok = TagBuilder.CreateXmlDocument( rc_filename );
		ASSERT_MESSAGE( ok, "empty tag document was not created" );
		globalOutputStream() << "Created empty tag file " << rc_filename << ". Plugins -> ShaderPlug -> Create tag file: to index all textures and shaders, if needed.\n";
	}
}

void TextureBrowser_addTag(){
	auto tag = StringStream<64>( "NewTag" );
	int index = 0;
	while( g_TexBro.m_all_tags.contains( tag.c_str() ) )
		tag( "NewTag", ++index );

	auto item = new QListWidgetItem( tag.c_str() );
	item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren );
	item->setCheckState( Qt::CheckState::Unchecked ); // is needed to see checkbox
	g_TexBro.m_tagsListWidget->addItem( item );

	g_TexBro.m_all_tags.insert( tag.c_str() );

	g_TexBro.m_tagsListWidget->scrollToItem( item );
	g_TexBro.m_tagsListWidget->editItem( item );
}

void TextureBrowser_renameTag(){
	const auto selected = g_TexBro.m_tagsListWidget->selectedItems();

	if ( !selected.empty() ) {
		g_TexBro.m_tagsListWidget->editItem( selected.front() );
	}
}

void TextureBrowser_deleteTag(){
	const auto selected = g_TexBro.m_tagsListWidget->selectedItems();
	if ( !selected.empty() ) {
		if ( eIDYES == qt_MessageBox( g_TexBro.m_parent, "Are you sure you want to delete the selected tags?", "Delete Tag", EMessageBoxType::Question ) ) {
			for( auto item : selected ){
				auto tag = item->text().toLatin1();
				delete item;
				TagBuilder.DeleteTag( tag.constData() );
				g_TexBro.m_all_tags.erase( tag.constData() );
			}
		}
	}
}

void TextureBrowser_copyTag(){
	g_TexBro.m_copied_tags.clear();
	TagBuilder.GetShaderTags( g_TexBro.m_shader.c_str(), g_TexBro.m_copied_tags );
}

void TextureBrowser_pasteTag(){
	const CopiedString shader = g_TexBro.m_shader;

	if ( !TagBuilder.CheckShaderTag( shader.c_str() ) ) {
		IShader* ishader = QERApp_Shader_ForName( shader.c_str() );
		if ( ishader->IsDefault() ) {
			// it's a texture
			TagBuilder.AddShaderNode( shader.c_str(), TextureType::CUSTOM, NodeShaderType::TEXTURE );
		}
		else
		{
			// it's a shader
			TagBuilder.AddShaderNode( shader.c_str(), TextureType::CUSTOM, NodeShaderType::SHADER );
		}
		ishader->DecRef();

		for ( const CopiedString& tag : g_TexBro.m_copied_tags )
		{
			TagBuilder.AddShaderTag( shader.c_str(), tag.c_str(), NodeTagType::TAG );
		}
	}
	else
	{
		for ( const CopiedString& tag : g_TexBro.m_copied_tags )
		{
			if ( !TagBuilder.CheckShaderTag( shader.c_str(), tag.c_str() ) ) {
				// the tag doesn't exist - let's add it
				TagBuilder.AddShaderTag( shader.c_str(), tag.c_str(), NodeTagType::TAG );
			}
		}
	}

	TextureBrowser_tagsSetCheckboxesForShader( shader.c_str() );
}


void TextureBrowser_SetNotex(){
	g_notex = StringStream( GlobalRadiant().getAppPath(), "bitmaps/notex.png" );
	g_shadernotex = StringStream( GlobalRadiant().getAppPath(), "bitmaps/shadernotex.png" );
}


class Filter_QLineEdit : public QLineEdit
{
protected:
	void enterEvent( QEvent *event ) override {
		setFocus();
	}
	void leaveEvent( QEvent *event ) override {
		clearFocus();
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				clear();
				event->accept();
			}
		}
		return QLineEdit::event( event );
	}
};

void TextureBrowser_filterSetModeIcon( QAction *action ){
	action->setIcon( QApplication::style()->standardIcon(
		g_TextureBrowser_filter_searchFromStart
		? QStyle::StandardPixmap::SP_CommandLink
		: QStyle::StandardPixmap::SP_FileDialogContentsView ) );
}

#include "timer.h"

class TexWndGLWidget : public QOpenGLWidget
{
	TextureBrowser& m_texBro;
	qreal m_scale;
	MousePresses m_mouse;
public:
	TexWndGLWidget( TextureBrowser& textureBrowser ) : QOpenGLWidget(), m_texBro( textureBrowser )
	{
	}

	~TexWndGLWidget() override {
		glwidget_context_destroyed();
	}
protected:
	void initializeGL() override
	{
		glwidget_context_created( *this );
		// show definitely after gl init, otherwise crash
		TextureBrowser_ShowStartupShaders();
	}
	void resizeGL( int w, int h ) override
	{
		m_scale = devicePixelRatioF();
		m_texBro.m_width = float_to_integer( w * m_scale );
		m_texBro.m_height = float_to_integer( h * m_scale );
		m_texBro.heightChanged();
		m_texBro.m_originInvalid = true;
	}
	void paintGL() override
	{
		GlobalOpenGL_debugAssertNoErrors();
		m_texBro.draw();
		GlobalOpenGL_debugAssertNoErrors();
	}

	void mousePressEvent( QMouseEvent *event ) override {
		setFocus();
		const auto press = m_mouse.press( event );
		if( press == MousePresses::Left2x || press == MousePresses::Right2x ){
			mouseDoubleClick( press );
		}
		else if ( press == MousePresses::Right ) {
			TextureBrowser_Tracking_MouseDown( m_texBro );
		}
		else if ( press == MousePresses::Left || press == MousePresses::Middle ) {
			if ( !event->modifiers().testFlag( Qt::KeyboardModifier::ShiftModifier ) )
				SelectTexture( m_texBro, event->x() * m_scale, event->y() * m_scale, press == MousePresses::Middle );
		}
	}
	void mouseDoubleClick( MousePresses::Result press ){
		/* loads directory, containing active shader + focuses on it */
		if ( press == MousePresses::Left2x && !TextureBrowser::wads ) {
			const StringRange range( strchr( m_texBro.m_shader.c_str(), '/' ) + 1, strrchr( m_texBro.m_shader.c_str(), '/' ) + 1 );
			if( !range.empty() ){
				const CopiedString dir = range;
				ScopeDisableScreenUpdates disableScreenUpdates( dir.c_str(), "Loading Textures" );
				TextureBrowser_ShowDirectory( dir.c_str() );
				TextureBrowser_Focus( m_texBro, m_texBro.m_shader.c_str() );
				m_texBro.queueDraw();
			}
		}
		else if ( press == MousePresses::Right2x ) {
			ScopeDisableScreenUpdates disableScreenUpdates( TextureBrowser_getCommonShadersDir(), "Loading Textures" );
			TextureBrowser_ShowDirectory( TextureBrowser_getCommonShadersDir() );
			m_texBro.queueDraw();
		}
	}
	void mouseReleaseEvent( QMouseEvent *event ) override {
		const auto release = m_mouse.release( event );
		if ( release == MousePresses::Right ) {
			TextureBrowser_Tracking_MouseUp( m_texBro );
		}
		else if ( release == MousePresses::Left && event->modifiers().testFlag( Qt::KeyboardModifier::ShiftModifier ) ) {
			TextureBrowser_ViewShader( m_texBro, event->modifiers(), event->x() * m_scale, event->y() * m_scale );
		}
	}
	void wheelEvent( QWheelEvent *event ) override {
		setFocus();

		if( !m_texBro.m_parent->isActiveWindow() ){
			m_texBro.m_parent->activateWindow();
			m_texBro.m_parent->raise();
		}

		const int originy = m_texBro.getOriginY() + std::copysign( m_texBro.m_mouseWheelScrollIncrement, event->angleDelta().y() );
		m_texBro.setOriginY( originy );
	}
};


QWidget* TextureBrowser_constructWindow( QWidget* toplevel ){
	TextureBrowser_checkTagFile();
	TextureBrowser_SetNotex();

	GlobalShaderSystem().setActiveShadersChangedNotify( ReferenceCaller<TextureBrowser, void(), TextureBrowser_activeShadersChanged>( g_TexBro ) );

	g_TexBro.m_parent = toplevel;

	QSplitter *splitter = new QSplitter;
	QWidget *containerWidgetLeft = new QWidget; // Adding a QLayout to a QSplitter is not supported, use proxy widget
	QWidget *containerWidgetRight = new QWidget; // Adding a QLayout to a QSplitter is not supported, use proxy widget
	splitter->addWidget( containerWidgetLeft );
	splitter->addWidget( containerWidgetRight );
	QVBoxLayout *vbox = new QVBoxLayout( containerWidgetLeft );
	QHBoxLayout *hbox = new QHBoxLayout( containerWidgetRight );

	hbox->setContentsMargins( 0, 0, 0, 0 );
	vbox->setContentsMargins( 0, 0, 0, 0 );
	hbox->setSpacing( 0 );
	vbox->setSpacing( 0 );


	{	// menu bar
		QToolBar *toolbar = new QToolBar;
		vbox->addWidget( toolbar );

		QMenu* menu_view = TextureBrowser_constructViewMenu();

		//show detached menu over floating tex bro and main wnd...
		menu_view->setParent( toolbar, menu_view->windowFlags() ); //don't reset windowFlags

		//view menu button
		toolbar_append_button( toolbar, "View", "texbro_view.png", PointerCaller<QMenu, void(), +[]( QMenu *menu ){ menu->popup( QCursor::pos() ); }>( menu_view ) );

		toolbar_append_button( toolbar, "Find / Replace...", "texbro_gtk-find-and-replace.png", "FindReplaceTextures" );

		toolbar_append_button( toolbar, "Flush & Reload Shaders", "texbro_refresh.png", "RefreshShaders" );
	}
	{	// filter entry
		QLineEdit *entry = g_TexBro.m_filter_entry = new Filter_QLineEdit;
		vbox->addWidget( entry );
		entry->setClearButtonEnabled( true );
		entry->setFocusPolicy( Qt::FocusPolicy::ClickFocus );

		QAction *action = g_TexBro.m_filter_action = entry->addAction( QApplication::style()->standardIcon( QStyle::StandardPixmap::SP_CommandLink ), QLineEdit::LeadingPosition );
		TextureBrowser_filterSetModeIcon( action );
		action->setToolTip( "toggle match mode ( start / any position )" );

		QObject::connect( entry, &QLineEdit::textChanged, []( const QString& text ){
			g_TexBro.m_filter_string = text.toLatin1().constData();
			g_TexBro.heightChanged();
			g_TexBro.m_originInvalid = true;
		} );
		QObject::connect( action, &QAction::triggered, GlobalToggles_find( "SearchFromStart" ).m_command.m_callback );
	}
	{	// Texture TreeView
		TextureBrowser_createTreeViewTree();
	}
	{	// gl_widget
		g_TexBro.m_gl_widget = new TexWndGLWidget( g_TexBro );

		hbox->addWidget( g_TexBro.m_gl_widget );
	}
	{	// gl_widget scrollbar
		auto scroll = g_TexBro.m_texture_scroll = new QScrollBar;
		hbox->addWidget( scroll );

		QObject::connect( scroll, &QAbstractSlider::valueChanged, []( int value ){
			g_TexBro.m_scrollAdjustment.value_changed( value );
		} );

		scroll->setVisible( g_TexBro.m_showTextureScrollbar );
	}

	{ // tag stuff
		g_TexBro.m_tagsListWidget = new Tags_QListWidget;
		g_TexBro.m_tagsListWidget->setSortingEnabled( true );
		g_TexBro.m_tagsListWidget->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
		g_TexBro.m_tagsListWidget->setEditTriggers( QAbstractItemView::EditTrigger::SelectedClicked | QAbstractItemView::EditTrigger::EditKeyPressed );
		g_TexBro.m_tagsListWidget->setUniformItemSizes( true ); // optimization
		g_TexBro.m_tagsListWidget->setFocusPolicy( Qt::FocusPolicy::ClickFocus );

		g_TexBro.m_tagsListWidget->setItemDelegate( new Tags_QItemDelegate( g_TexBro.m_tagsListWidget ) );

		QObject::connect( g_TexBro.m_tagsListWidget, &QListWidget::activated, TextureBrowser_searchTags );

		TagBuilder.GetAllTags( g_TexBro.m_all_tags );
		for ( const CopiedString& tag : g_TexBro.m_all_tags ){
			auto item = new QListWidgetItem( tag.c_str() );
			item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren );
			item->setCheckState( Qt::CheckState::Unchecked ); // is needed to see checkbox
			g_TexBro.m_tagsListWidget->addItem( item );
		}
	}
	{	// tag context menu
		g_TexBro.m_tagsMenu = TextureBrowser_constructTagsMenu();

		//show detached menu over floating tex bro and main wnd...
		g_TexBro.m_tagsMenu->setParent( g_TexBro.m_tagsListWidget, g_TexBro.m_tagsMenu->windowFlags() ); //don't reset windowFlags
	}

	{	// Texture/Tag notebook
		g_TexBro.m_tabs = new QTabWidget;
		g_TexBro.m_tabs->setFocusPolicy( Qt::FocusPolicy::ClickFocus );
		g_TexBro.m_tabs->setDocumentMode( true );
		g_TexBro.m_tabs->setTabBarAutoHide( true );
		g_TexBro.m_tabs->addTab( g_TexBro.m_treeView, "Textures" );
		static_cast<QObject*>( g_TexBro.m_tagsListWidget )->setParent( g_TexBro.m_tabs );
		TextureBrowser_tagsEnableGui( g_TexBro.m_tags );
		vbox->addWidget( g_TexBro.m_tabs );

		QObject::connect( g_TexBro.m_tabs, &QTabWidget::currentChanged, []( int index ){
			if( index == 1 )
				TextureBrowser_tagsSetCheckboxesForShader( g_TexBro.m_shader.c_str() );
		} );
	}

	splitter->setStretchFactor( 0, 0 ); // consistent treeview side sizing on resizes
	splitter->setStretchFactor( 1, 1 );
	g_guiSettings.addSplitter( splitter, "TextureBrowser/splitter", { 100, 800 } );
	return splitter;
}

void TextureBrowser_destroyWindow(){
	GlobalShaderSystem().setActiveShadersChangedNotify( Callback<void()>() );
}

const Vector3& TextureBrowser_getBackgroundColour(){
	return g_TexBro.m_color_textureback;
}

void TextureBrowser_setBackgroundColour( const Vector3& colour ){
	g_TexBro.m_color_textureback = colour;
	g_TexBro.queueDraw();
}

void TextureBrowser_shaderInfo(){
	const char* name = TextureBrowser_GetSelectedShader();
	IShader* shader = QERApp_Shader_ForName( name );

	DoShaderInfoDlg( name, shader->getShaderFileName(), "Shader Info" );

	shader->DecRef();
}

void RefreshShaders(){
	g_TextureBrowser_currentDirectory = "";
	g_TexBro.m_searchedTags = false;
	TextureBrowser_updateTitle();

	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Shaders" );
	GlobalShaderSystem().refresh();
	TextureBrowser_constructTreeStore(); /* texturebrowser tree update on vfs restart */
	UpdateAllWindows();
}

void TextureBrowser_ToggleShowShaders(){
	g_TexBro.m_showShaders ^= 1;
	g_TexBro.m_showshaders_item.update();

	g_TexBro.m_heightChanged = true;
	g_TexBro.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	g_TexBro.queueDraw();
}

void TextureBrowser_ToggleShowTextures(){
	g_TexBro.m_showTextures ^= 1;
	g_TexBro.m_showtextures_item.update();

	g_TexBro.m_heightChanged = true;
	g_TexBro.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	g_TexBro.queueDraw();
}

void TextureBrowser_ToggleShowShaderListOnly(){
	g_TextureBrowser_shaderlistOnly ^= 1;
	g_TexBro.m_showshaderlistonly_item.update();

	TextureBrowser_constructTreeStore();
}

void TextureBrowser_showAll(){
	g_TextureBrowser_currentDirectory = "";
	g_TexBro.m_searchedTags = false;
//	TextureBrowser_SetHideUnused( g_TexBro, false );
	TextureBrowser_ToggleHideUnused(); //toggle to show all used on the first hit and all on the second
	TextureBrowser_updateTitle();
}

void TextureBrowser_FixedSize(){
	g_TextureBrowser_fixedSize ^= 1;
	g_TexBro.m_fixedsize_item.update();
	TextureBrowser_activeShadersChanged( g_TexBro );
}

void TextureBrowser_FilterNotex(){
	g_TextureBrowser_filterNotex ^= 1;
	g_TexBro.m_filternotex_item.update();
	TextureBrowser_activeShadersChanged( g_TexBro );
}

void TextureBrowser_EnableAlpha(){
	g_TextureBrowser_enableAlpha ^= 1;
	g_TexBro.m_enablealpha_item.update();
	TextureBrowser_activeShadersChanged( g_TexBro );
}

void TextureBrowser_tagsToggleGui(){
	g_TexBro.m_tags ^= 1;
	g_TexBro.m_tags_item.update();
	TextureBrowser_tagsEnableGui( g_TexBro.m_tags );
}

void TextureBrowser_filter_searchFromStart(){
	g_TextureBrowser_filter_searchFromStart ^= 1;
	g_TexBro.m_filter_searchFromStart_item.update();
	TextureBrowser_activeShadersChanged( g_TexBro );
	TextureBrowser_filterSetModeIcon( g_TexBro.m_filter_action );
}


void TextureBrowser_exportTitle( const StringImportCallback& importer ){
	const auto buffer = StringStream<64>( "Textures: ", !g_TextureBrowser_currentDirectory.empty()
	                                                    ? g_TextureBrowser_currentDirectory.c_str()
	                                                    : "all" );
	importer( buffer );
}


void TextureScaleImport( TextureBrowser& textureBrowser, int value ){
	switch ( value )
	{
	case 0:
		TextureBrowser_setScale( textureBrowser, 10 );
		break;
	case 1:
		TextureBrowser_setScale( textureBrowser, 25 );
		break;
	case 2:
		TextureBrowser_setScale( textureBrowser, 50 );
		break;
	case 3:
		TextureBrowser_setScale( textureBrowser, 100 );
		break;
	case 4:
		TextureBrowser_setScale( textureBrowser, 200 );
		break;
	}
}
typedef ReferenceCaller<TextureBrowser, void(int), TextureScaleImport> TextureScaleImportCaller;

void TextureScaleExport( TextureBrowser& textureBrowser, const IntImportCallback& importer ){
	switch ( textureBrowser.m_textureScale )
	{
	case 10:
		importer( 0 );
		break;
	case 25:
		importer( 1 );
		break;
	case 50:
		importer( 2 );
		break;
	case 100:
		importer( 3 );
		break;
	case 200:
		importer( 4 );
		break;
	}
}
typedef ReferenceCaller<TextureBrowser, void(const IntImportCallback&), TextureScaleExport> TextureScaleExportCaller;

void UniformTextureSizeImport( TextureBrowser& textureBrowser, int value ){
	if ( value >= 16 )
		TextureBrowser_setUniformSize( textureBrowser, value );
}
typedef ReferenceCaller<TextureBrowser, void(int), UniformTextureSizeImport> UniformTextureSizeImportCaller;

void UniformTextureMinSizeImport( TextureBrowser& textureBrowser, int value ){
	if ( value >= 16 )
		TextureBrowser_setUniformMinSize( textureBrowser, value );
}
typedef ReferenceCaller<TextureBrowser, void(int), UniformTextureMinSizeImport> UniformTextureMinSizeImportCaller;

void TextureBrowser_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox(
	    "", "Texture scrollbar",
	    TextureBrowserImportShowScrollbarCaller( g_TexBro ),
	    BoolExportCaller( g_TexBro.m_showTextureScrollbar )
	);
	{
		const char* texture_scale[] = { "10%", "25%", "50%", "100%", "200%" };
		page.appendCombo(
		    "Texture Thumbnail Scale",
		    StringArrayRange( texture_scale ),
		    IntImportCallback( TextureScaleImportCaller( g_TexBro ) ),
		    IntExportCallback( TextureScaleExportCaller( g_TexBro ) )
		);
	}
	page.appendSpinner( "Thumbnails Max Size", g_TexBro.m_uniformTextureSize, 16, 8192 );
	page.appendSpinner( "Thumbnails Min Size", g_TexBro.m_uniformTextureMinSize, 16, 8192 );
	page.appendSpinner( "Mousewheel Increment", g_TexBro.m_mouseWheelScrollIncrement, 0, 8192 );
	{
		const char* startup_shaders[] = { "None", TextureBrowser_getCommonShadersName() };
		page.appendCombo( "Load Shaders at Startup", reinterpret_cast<int&>( g_TexBro.m_startupShaders ), StringArrayRange( startup_shaders ) );
	}
	{
		const auto str = StringStream<64>( "Hide nonShaders in ", TextureBrowser_getCommonShadersDir(), " folder" );
		page.appendCheckBox(
		    "", str,
		    g_TexBro.m_hideNonShadersInCommon
		);
	}
}
void TextureBrowser_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Texture Browser", "Texture Browser Preferences" ) );
	TextureBrowser_constructPreferences( page );
}
void TextureBrowser_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( TextureBrowser_constructPage ) );
}


#include "preferencesystem.h"
#include "stringio.h"

typedef ReferenceCaller<TextureBrowser, void(std::size_t), TextureBrowser_setScale> TextureBrowserSetScaleCaller;



void TextureClipboard_textureSelected( const char* shader );

void TextureBrowser_Construct(){
	GlobalCommands_insert( "ShaderInfo", makeCallbackF( TextureBrowser_shaderInfo ) );
	GlobalCommands_insert( "TagSearchUntagged", makeCallbackF( TextureBrowser_showUntagged ) );
	GlobalCommands_insert( "TagSearch", makeCallbackF( TextureBrowser_searchTags ) );
	GlobalCommands_insert( "TagAdd", makeCallbackF( TextureBrowser_addTag ) );
	GlobalCommands_insert( "TagRename", makeCallbackF( TextureBrowser_renameTag ) );
	GlobalCommands_insert( "TagDelete", makeCallbackF( TextureBrowser_deleteTag ) );
	GlobalCommands_insert( "TagCopy", makeCallbackF( TextureBrowser_copyTag ) );
	GlobalCommands_insert( "TagPaste", makeCallbackF( TextureBrowser_pasteTag ) );
	GlobalCommands_insert( "RefreshShaders", makeCallbackF( RefreshShaders ) );
	GlobalToggles_insert( "ShowInUse", makeCallbackF( TextureBrowser_ToggleHideUnused ), ToggleItem::AddCallbackCaller( g_TexBro.m_hideunused_item ), QKeySequence( "U" ) );
	GlobalCommands_insert( "ShowAllTextures", makeCallbackF( TextureBrowser_showAll ), QKeySequence( "Ctrl+A" ) );
	GlobalCommands_insert( "ToggleTextures", makeCallbackF( TextureBrowser_toggleShow ), QKeySequence( "T" ) );
	GlobalToggles_insert( "ToggleShowShaders", makeCallbackF( TextureBrowser_ToggleShowShaders ), ToggleItem::AddCallbackCaller( g_TexBro.m_showshaders_item ) );
	GlobalToggles_insert( "ToggleShowTextures", makeCallbackF( TextureBrowser_ToggleShowTextures ), ToggleItem::AddCallbackCaller( g_TexBro.m_showtextures_item ) );
	GlobalToggles_insert( "ToggleShowShaderlistOnly", makeCallbackF( TextureBrowser_ToggleShowShaderListOnly ), ToggleItem::AddCallbackCaller( g_TexBro.m_showshaderlistonly_item ) );
	GlobalToggles_insert( "FixedSize", makeCallbackF( TextureBrowser_FixedSize ), ToggleItem::AddCallbackCaller( g_TexBro.m_fixedsize_item ) );
	GlobalToggles_insert( "FilterNotex", makeCallbackF( TextureBrowser_FilterNotex ), ToggleItem::AddCallbackCaller( g_TexBro.m_filternotex_item ) );
	GlobalToggles_insert( "EnableAlpha", makeCallbackF( TextureBrowser_EnableAlpha ), ToggleItem::AddCallbackCaller( g_TexBro.m_enablealpha_item ) );
	GlobalToggles_insert( "TagsToggleGui", makeCallbackF( TextureBrowser_tagsToggleGui ), ToggleItem::AddCallbackCaller( g_TexBro.m_tags_item ) );
	GlobalToggles_insert( "SearchFromStart", makeCallbackF( TextureBrowser_filter_searchFromStart ), ToggleItem::AddCallbackCaller( g_TexBro.m_filter_searchFromStart_item ) );

	GlobalPreferenceSystem().registerPreference( "TextureScale",
	                                             makeSizeStringImportCallback( TextureBrowserSetScaleCaller( g_TexBro ) ),
	                                             SizeExportStringCaller( g_TexBro.m_textureScale )
	                                           );
	GlobalPreferenceSystem().registerPreference( "UniformTextureSize",
	                                             makeIntStringImportCallback( UniformTextureSizeImportCaller( g_TexBro ) ),
	                                             IntExportStringCaller( g_TexBro.m_uniformTextureSize ) );
	GlobalPreferenceSystem().registerPreference( "UniformTextureMinSize",
	                                             makeIntStringImportCallback( UniformTextureMinSizeImportCaller( g_TexBro ) ),
	                                             IntExportStringCaller( g_TexBro.m_uniformTextureMinSize ) );
	GlobalPreferenceSystem().registerPreference( "TextureScrollbar",
	                                             makeBoolStringImportCallback( TextureBrowserImportShowScrollbarCaller( g_TexBro ) ),
	                                             BoolExportStringCaller( g_TexBro.m_showTextureScrollbar )
	                                           );
	GlobalPreferenceSystem().registerPreference( "ShowShaders", BoolImportStringCaller( g_TexBro.m_showShaders ), BoolExportStringCaller( g_TexBro.m_showShaders ) );
	GlobalPreferenceSystem().registerPreference( "ShowTextures", BoolImportStringCaller( g_TexBro.m_showTextures ), BoolExportStringCaller( g_TexBro.m_showTextures ) );
	GlobalPreferenceSystem().registerPreference( "ShowShaderlistOnly", BoolImportStringCaller( g_TextureBrowser_shaderlistOnly ), BoolExportStringCaller( g_TextureBrowser_shaderlistOnly ) );
	GlobalPreferenceSystem().registerPreference( "FixedSize", BoolImportStringCaller( g_TextureBrowser_fixedSize ), BoolExportStringCaller( g_TextureBrowser_fixedSize ) );
	GlobalPreferenceSystem().registerPreference( "FilterNotex", BoolImportStringCaller( g_TextureBrowser_filterNotex ), BoolExportStringCaller( g_TextureBrowser_filterNotex ) );
	GlobalPreferenceSystem().registerPreference( "EnableAlpha", BoolImportStringCaller( g_TextureBrowser_enableAlpha ), BoolExportStringCaller( g_TextureBrowser_enableAlpha ) );
	GlobalPreferenceSystem().registerPreference( "TagsShowGui", BoolImportStringCaller( g_TexBro.m_tags ), BoolExportStringCaller( g_TexBro.m_tags ) );
	GlobalPreferenceSystem().registerPreference( "SearchFromStart", BoolImportStringCaller( g_TextureBrowser_filter_searchFromStart ), BoolExportStringCaller( g_TextureBrowser_filter_searchFromStart ) );
	GlobalPreferenceSystem().registerPreference( "LoadShaders", IntImportStringCaller( reinterpret_cast<int&>( g_TexBro.m_startupShaders ) ), IntExportStringCaller( reinterpret_cast<int&>( g_TexBro.m_startupShaders ) ) );
	GlobalPreferenceSystem().registerPreference( "WheelMouseInc", IntImportStringCaller( g_TexBro.m_mouseWheelScrollIncrement ), IntExportStringCaller( g_TexBro.m_mouseWheelScrollIncrement ) );
	GlobalPreferenceSystem().registerPreference( "ColorTexBroBackground", Vector3ImportStringCaller( g_TexBro.m_color_textureback ), Vector3ExportStringCaller( g_TexBro.m_color_textureback ) );
	GlobalPreferenceSystem().registerPreference( "HideNonShadersInCommon", BoolImportStringCaller( g_TexBro.m_hideNonShadersInCommon ), BoolExportStringCaller( g_TexBro.m_hideNonShadersInCommon ) );

	g_TexBro.m_shader = texdef_name_default();

	TextureBrowser::wads = !string_empty( g_pGameDescription->getKeyValue( "show_wads" ) );

	Textures_setModeChangedNotify( ConstMemberCaller<TextureBrowser, void(), &TextureBrowser::queueDraw>( g_TexBro ) );

	TextureBrowser_registerPreferencesPage();

	GlobalShaderSystem().attach( g_ShadersObserver );

	TextureBrowser_textureSelected = TextureClipboard_textureSelected;
}
void TextureBrowser_Destroy(){
	TagBuilder.SaveXmlDoc();

	GlobalShaderSystem().detach( g_ShadersObserver );

	Textures_setModeChangedNotify( Callback<void()>() );
}
