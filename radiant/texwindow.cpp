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
#include "warnings.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"

#include <set>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gtk/gtkrange.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvscrollbar.h>

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
#include "cmdlib.h"
#include "texmanip.h"
#include "textures.h"
#include "convert.h"

#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/cursor.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"

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

void TextureBrowser_queueDraw( TextureBrowser& textureBrowser );

bool string_equal_start( const char* string, StringRange start ){
	return string_equal_n( string, start.first, start.last - start.first );
}

typedef std::set<CopiedString> TextureGroups;

void TextureGroups_addWad( TextureGroups& groups, const char* archive ){
	if ( extension_equal( path_get_extension( archive ), "wad" ) ) {
#if 1
		groups.insert( archive );
#else
		CopiedString archiveBaseName( path_get_filename_start( archive ), path_get_filename_base_end( archive ) );
		groups.insert( archiveBaseName );
#endif
	}
}
typedef ReferenceCaller1<TextureGroups, const char*, TextureGroups_addWad> TextureGroupsAddWadCaller;

void TextureGroups_addShader( TextureGroups& groups, const char* shaderName ){
	const char* texture = path_make_relative( shaderName, "textures/" );
	if ( texture != shaderName ) {
		const char* last = path_remove_directory( texture );
		if ( !string_empty( last ) ) {
			groups.insert( CopiedString( StringRange( texture, --last ) ) );
		}
	}
}
typedef ReferenceCaller1<TextureGroups, const char*, TextureGroups_addShader> TextureGroupsAddShaderCaller;

void TextureGroups_addDirectory( TextureGroups& groups, const char* directory ){
	groups.insert( directory );
}
typedef ReferenceCaller1<TextureGroups, const char*, TextureGroups_addDirectory> TextureGroupsAddDirectoryCaller;

namespace
{
bool g_TextureBrowser_shaderlistOnly = false;
bool g_TextureBrowser_fixedSize = true;
bool g_TextureBrowser_filterNotex = false;
bool g_TextureBrowser_enableAlpha = false;
bool g_TextureBrowser_filter_searchFromStart = false;
}

class DeferredAdjustment
{
gdouble m_value;
guint m_handler;
typedef void ( *ValueChangedFunction )( void* data, gdouble value );
ValueChangedFunction m_function;
void* m_data;

static gboolean deferred_value_changed( gpointer data ){
	reinterpret_cast<DeferredAdjustment*>( data )->m_function(
		reinterpret_cast<DeferredAdjustment*>( data )->m_data,
		reinterpret_cast<DeferredAdjustment*>( data )->m_value
		);
	reinterpret_cast<DeferredAdjustment*>( data )->m_handler = 0;
	reinterpret_cast<DeferredAdjustment*>( data )->m_value = 0;
	return FALSE;
}
public:
DeferredAdjustment( ValueChangedFunction function, void* data ) : m_value( 0 ), m_handler( 0 ), m_function( function ), m_data( data ){
}
void flush(){
	if ( m_handler != 0 ) {
		g_source_remove( m_handler );
		deferred_value_changed( this );
	}
}
void value_changed( gdouble value ){
	m_value = value;
	if ( m_handler == 0 ) {
		m_handler = g_idle_add( deferred_value_changed, this );
	}
}
static void adjustment_value_changed( GtkAdjustment *adjustment, DeferredAdjustment* self ){
	self->value_changed( adjustment->value );
}
};



class TextureBrowser;

typedef ReferenceCaller<TextureBrowser, TextureBrowser_queueDraw> TextureBrowserQueueDrawCaller;

void TextureBrowser_scrollChanged( void* data, gdouble value );


enum StartupShaders
{
	STARTUPSHADERS_NONE = 0,
	STARTUPSHADERS_COMMON,
};

void TextureBrowser_hideUnusedExport( const BoolImportCallback& importer );
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_hideUnusedExport> TextureBrowserHideUnusedExport;

void TextureBrowser_showShadersExport( const BoolImportCallback& importer );
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_showShadersExport> TextureBrowserShowShadersExport;

void TextureBrowser_showTexturesExport( const BoolImportCallback& importer );
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_showTexturesExport> TextureBrowserShowTexturesExport;

void TextureBrowser_showShaderlistOnly( const BoolImportCallback& importer ){
	importer( g_TextureBrowser_shaderlistOnly );
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_showShaderlistOnly> TextureBrowserShowShaderlistOnlyExport;

void TextureBrowser_fixedSize( const BoolImportCallback& importer ){
	importer( g_TextureBrowser_fixedSize );
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_fixedSize> TextureBrowserFixedSizeExport;

void TextureBrowser_filterNotex( const BoolImportCallback& importer ){
	importer( g_TextureBrowser_filterNotex );
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_filterNotex> TextureBrowserFilterNotexExport;

void TextureBrowser_enableAlpha( const BoolImportCallback& importer ){
	importer( g_TextureBrowser_enableAlpha );
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_enableAlpha> TextureBrowserEnableAlphaExport;

void TextureBrowser_filter_searchFromStart( const BoolImportCallback& importer ){
	importer( g_TextureBrowser_filter_searchFromStart );
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_filter_searchFromStart> TextureBrowser_filter_searchFromStartExport;


class TextureBrowser
{
public:
int width, height;
int originy;
int m_nTotalHeight;

CopiedString shader;

GtkWindow* m_parent;
GtkWidget* m_gl_widget;
GtkWidget* m_texture_scroll;
GtkWidget* m_treeViewTree;
GtkWidget* m_treeViewTags;
GtkWidget* m_tag_frame;
GtkListStore* m_assigned_store;
GtkListStore* m_available_store;
GtkWidget* m_assigned_tree;
GtkWidget* m_available_tree;
GtkWidget* m_scr_win_tree;
GtkWidget* m_scr_win_tags;
GtkWidget* m_tag_notebook;
GtkWidget* m_search_button;
GtkWidget* m_shader_info_item;
GtkWidget* m_filter_entry;

std::set<CopiedString> m_all_tags;
GtkListStore* m_all_tags_list;
std::vector<CopiedString> m_copied_tags;
std::set<CopiedString> m_found_shaders;

ToggleItem m_hideunused_item;
ToggleItem m_showshaders_item;
ToggleItem m_showtextures_item;
ToggleItem m_showshaderlistonly_item;
ToggleItem m_fixedsize_item;
ToggleItem m_filternotex_item;
ToggleItem m_enablealpha_item;
ToggleItem m_filter_searchFromStart_item;

guint m_sizeHandler;
guint m_exposeHandler;

bool m_heightChanged;
bool m_originInvalid;

DeferredAdjustment m_scrollAdjustment;
FreezePointer m_freezePointer;

Vector3 color_textureback;
// the increment step we use against the wheel mouse
std::size_t m_mouseWheelScrollIncrement;
std::size_t m_textureScale;
// make the texture increments match the grid changes
bool m_showShaders;
bool m_showTextures;
bool m_showTextureScrollbar;
StartupShaders m_startupShaders;
// if true, the texture window will only display in-use shaders
// if false, all the shaders in memory are displayed
bool m_hideUnused;
bool m_rmbSelected;
bool m_searchedTags;
bool m_tags;
bool m_move_started;
int m_move_amount;
BasicVector2<int> m_move_start;
// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
int m_uniformTextureSize;
int m_uniformTextureMinSize;

bool m_hideNonShadersInCommon;

static bool wads;
// Return the display width of a texture in the texture browser
void getTextureWH( qtexture_t* tex, int &W, int &H ){
		// Don't use uniform size
		W = (int)( tex->width * ( (float)m_textureScale / 100 ) );
		H = (int)( tex->height * ( (float)m_textureScale / 100 ) );
		if ( W < 1 ) W = 1;
		if ( H < 1 ) H = 1;

	if ( g_TextureBrowser_fixedSize ){
		if	( W >= H ) {
			// Texture is square, or wider than it is tall
			if ( W >= m_uniformTextureSize ){
				H = m_uniformTextureSize * H / W;
				W = m_uniformTextureSize;
			}
			else if ( W <= m_uniformTextureMinSize ){
				H = m_uniformTextureMinSize * H / W;
				W = m_uniformTextureMinSize;
			}
		}
		else {
			// Texture taller than it is wide
			if ( H >= m_uniformTextureSize ){
				W = m_uniformTextureSize * W / H;
				H = m_uniformTextureSize;
			}
			else if ( H <= m_uniformTextureMinSize ){
				W = m_uniformTextureMinSize * W / H;
				H = m_uniformTextureMinSize;
			}
		}
	}
}

TextureBrowser() :
	m_texture_scroll( 0 ),
	m_hideunused_item( TextureBrowserHideUnusedExport() ),
	m_showshaders_item( TextureBrowserShowShadersExport() ),
	m_showtextures_item( TextureBrowserShowTexturesExport() ),
	m_showshaderlistonly_item( TextureBrowserShowShaderlistOnlyExport() ),
	m_fixedsize_item( TextureBrowserFixedSizeExport() ),
	m_filternotex_item( TextureBrowserFilterNotexExport() ),
	m_enablealpha_item( TextureBrowserEnableAlphaExport() ),
	m_filter_searchFromStart_item( TextureBrowser_filter_searchFromStartExport() ),
	m_heightChanged( true ),
	m_originInvalid( true ),
	m_scrollAdjustment( TextureBrowser_scrollChanged, this ),
	color_textureback( 0.25f, 0.25f, 0.25f ),
	m_mouseWheelScrollIncrement( 64 ),
	m_textureScale( 50 ),
	m_showShaders( true ),
	m_showTextures( true ),
	m_showTextureScrollbar( true ),
	m_startupShaders( STARTUPSHADERS_NONE ),
	m_hideUnused( false ),
	m_rmbSelected( false ),
	m_searchedTags( false ),
	m_tags( false ),
	m_move_started( false ),
	m_uniformTextureSize( 160 ),
	m_uniformTextureMinSize( 48 ),
	m_hideNonShadersInCommon( true ){
}
};
bool TextureBrowser::wads = false;

void ( *TextureBrowser_textureSelected )( const char* shader );


void TextureBrowser_updateScroll( TextureBrowser& textureBrowser );


const char* TextureBrowser_getComonShadersName(){
	const char* value = g_pGameDescription->getKeyValue( "common_shaders_name" );
	if ( !string_empty( value ) ) {
		return value;
	}
	return "Common";
}

const char* TextureBrowser_getComonShadersDir(){
	const char* value = g_pGameDescription->getKeyValue( "common_shaders_dir" );
	if ( !string_empty( value ) ) {
		return value;
	}
	return "common/";
}

inline int TextureBrowser_fontHeight( TextureBrowser& textureBrowser ){
	return GlobalOpenGL().m_font->getPixelHeight();
}

const char* TextureBrowser_GetSelectedShader(){
	return GlobalTextureBrowser().shader.c_str();
}

const char* TextureBrowser_GetSelectedShader( TextureBrowser& textureBrowser ){
	return textureBrowser.shader.c_str();
}

void TextureBrowser_SetStatus( TextureBrowser& textureBrowser, const char* name ){
	IShader* shader = QERApp_Shader_ForName( name );
	qtexture_t* q = shader->getTexture();
	StringOutputStream strTex( 256 );
	strTex << ( string_equal_prefix_nocase( name, "textures/" )? name + 9 : name ) << " W: " << Unsigned( q->width ) << " H: " << Unsigned( q->height );
	shader->DecRef();
	g_pParentWnd->SetStatusText( c_status_texture, strTex.c_str() );
}

void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name );

void TextureBrowser_SetSelectedShader( TextureBrowser& textureBrowser, const char* shader ){
	textureBrowser.shader = shader;
	TextureBrowser_SetStatus( textureBrowser, shader );
	TextureBrowser_Focus( textureBrowser, shader );

	if ( FindTextureDialog_isOpen() ) {
		FindTextureDialog_selectTexture( shader );
	}

	// disable the menu item "shader info" if no shader was selected
	if ( textureBrowser.m_shader_info_item == NULL ){
		return;
	}
	IShader* ishader = QERApp_Shader_ForName( shader );
	CopiedString filename = ishader->getShaderFileName();

	if ( filename.empty() ) {
		gtk_widget_set_sensitive( textureBrowser.m_shader_info_item, FALSE );
	}
	else {
		gtk_widget_set_sensitive( textureBrowser.m_shader_info_item, TRUE );
	}

	ishader->DecRef();
}

void TextureBrowser_SetSelectedShader( const char* shader ){
	TextureBrowser_SetSelectedShader( GlobalTextureBrowser(), shader );
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

class TextureLayout
{
public:
// texture layout functions
// TTimo: now based on shaders
int current_x, current_y, current_row;
};

void Texture_StartPos( TextureLayout& layout ){
	layout.current_x = 8;
	layout.current_y = -8;
	layout.current_row = 0;
}

void Texture_NextPos( TextureBrowser& textureBrowser, TextureLayout& layout, qtexture_t* current_texture, int *x, int *y ){
	qtexture_t* q = current_texture;

	int nWidth, nHeight;
	textureBrowser.getTextureWH( q, nWidth, nHeight );
	if ( layout.current_x + nWidth > textureBrowser.width - 8 && layout.current_row ) { // go to the next row unless the texture is the first on the row
		layout.current_x = 8;
		layout.current_y -= layout.current_row + TextureBrowser_fontHeight( textureBrowser ) + 5;//+4
		layout.current_row = 0;
	}

	*x = layout.current_x;
	*y = layout.current_y;

	// Is our texture larger than the row? If so, grow the
	// row height to match it

	if ( layout.current_row < nHeight ) {
		layout.current_row = nHeight;
	}

	// never go less than 96, or the names get all crunched up
	layout.current_x += nWidth < 96 ? 96 : nWidth;
	layout.current_x += 8;
}

bool TextureSearch_IsShown( const char* name ){
	std::set<CopiedString>::iterator iter;

	iter = GlobalTextureBrowser().m_found_shaders.find( name );

	return iter != GlobalTextureBrowser().m_found_shaders.end();
}

bool Texture_filtered( const char* name, TextureBrowser& textureBrowser ){
	const char* filter = gtk_entry_get_text( GTK_ENTRY( textureBrowser.m_filter_entry ) );
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
bool Texture_IsShown( IShader* shader, TextureBrowser& textureBrowser ){
	// filter notex / shadernotex images
	if ( g_TextureBrowser_filterNotex && ( string_equal( g_notex.c_str(), shader->getTexture()->name ) || string_equal( g_shadernotex.c_str(), shader->getTexture()->name ) ) ) {
		return false;
	}

	if ( g_TextureBrowser_currentDirectory == "Untagged" ) {
		std::set<CopiedString>::iterator iter;

		iter = textureBrowser.m_found_shaders.find( shader->getName() );

		if ( iter == textureBrowser.m_found_shaders.end() ) {
			return false;
		}
		else {
			return true;
		}
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
		&& shader_equal_prefix( shader_get_textureName( shader->getName() ), TextureBrowser_getComonShadersDir() ) ){
		return false;
	}

	if ( textureBrowser.m_searchedTags ) {
		return TextureSearch_IsShown( shader->getName() );
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

void TextureBrowser_heightChanged( TextureBrowser& textureBrowser ){
	textureBrowser.m_heightChanged = true;

	TextureBrowser_updateScroll( textureBrowser );
	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_evaluateHeight( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_heightChanged ) {
		textureBrowser.m_heightChanged = false;

		textureBrowser.m_nTotalHeight = 0;

		TextureLayout layout;
		Texture_StartPos( layout );
		for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
		{
			IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

			if ( !Texture_IsShown( shader, textureBrowser ) ) {
				continue;
			}

			int x, y;
			Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
			int nWidth, nHeight;
			textureBrowser.getTextureWH( shader->getTexture(), nWidth, nHeight );
			textureBrowser.m_nTotalHeight = std::max( textureBrowser.m_nTotalHeight, abs( layout.current_y ) + TextureBrowser_fontHeight( textureBrowser ) + nHeight + 4 );
		}
	}
}

int TextureBrowser_TotalHeight( TextureBrowser& textureBrowser ){
	TextureBrowser_evaluateHeight( textureBrowser );
	return textureBrowser.m_nTotalHeight;
}

void TextureBrowser_clampOriginY( TextureBrowser& textureBrowser ){
	if ( textureBrowser.originy > 0 ) {
		textureBrowser.originy = 0;
	}
	const int lower = std::min( textureBrowser.height - TextureBrowser_TotalHeight( textureBrowser ), 0 );
	if ( textureBrowser.originy < lower ) {
		textureBrowser.originy = lower;
	}
}

int TextureBrowser_getOriginY( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_originInvalid ) {
		textureBrowser.m_originInvalid = false;
		TextureBrowser_clampOriginY( textureBrowser );
		TextureBrowser_updateScroll( textureBrowser );
	}
	return textureBrowser.originy;
}

void TextureBrowser_setOriginY( TextureBrowser& textureBrowser, int originy ){
	textureBrowser.originy = originy;
	TextureBrowser_clampOriginY( textureBrowser );
	TextureBrowser_updateScroll( textureBrowser );
	TextureBrowser_queueDraw( textureBrowser );
}


Signal0 g_activeShadersChangedCallbacks;

void TextureBrowser_addActiveShadersChangedCallback( const SignalHandler& handler ){
	g_activeShadersChangedCallbacks.connectLast( handler );
}

void TextureBrowser_constructTreeStore();

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
	TextureBrowser_heightChanged( textureBrowser );
	textureBrowser.m_originInvalid = true;

	g_activeShadersChangedCallbacks();
}

void TextureBrowser_importShowScrollbar( TextureBrowser& textureBrowser, bool value ){
	textureBrowser.m_showTextureScrollbar = value;
	if ( textureBrowser.m_texture_scroll != 0 ) {
		widget_set_visible( textureBrowser.m_texture_scroll, textureBrowser.m_showTextureScrollbar );
		TextureBrowser_updateScroll( textureBrowser );
	}
}
typedef ReferenceCaller1<TextureBrowser, bool, TextureBrowser_importShowScrollbar> TextureBrowserImportShowScrollbarCaller;


/*
   ==============
   TextureBrowser_ShowDirectory
   relies on texture_directory global for the directory to use
   1) Load the shaders for the given directory
   2) Scan the remaining texture, load them and assign them a default shader (the "noshader" shader)
   NOTE: when writing a texture plugin, or some texture extensions, this function may need to be overriden, and made
   available through the IShaders interface
   NOTE: for texture window layout:
   all shaders are stored with alphabetical order after load
   previously loaded and displayed stuff is hidden, only in-use and newly loaded is shown
   ( the GL textures are not flushed though)
   ==============
 */

bool endswith( const char *haystack, const char *needle ){
	size_t lh = strlen( haystack );
	size_t ln = strlen( needle );
	if ( lh < ln ) {
		return false;
	}
	return !memcmp( haystack + ( lh - ln ), needle, ln );
}

bool texture_name_ignore( const char* name ){
	StringOutputStream strTemp( string_length( name ) );
	strTemp << LowerCase( name );

	return
		endswith( strTemp.c_str(), ".specular" ) ||
		endswith( strTemp.c_str(), ".glow" ) ||
		endswith( strTemp.c_str(), ".bump" ) ||
		endswith( strTemp.c_str(), ".diffuse" ) ||
		endswith( strTemp.c_str(), ".blend" ) ||
		endswith( strTemp.c_str(), ".alpha" ) ||
		endswith( strTemp.c_str(), "_norm" ) ||
		endswith( strTemp.c_str(), "_bump" ) ||
		endswith( strTemp.c_str(), "_glow" ) ||
		endswith( strTemp.c_str(), "_gloss" ) ||
		endswith( strTemp.c_str(), "_pants" ) ||
		endswith( strTemp.c_str(), "_shirt" ) ||
		endswith( strTemp.c_str(), "_reflect" ) ||
		endswith( strTemp.c_str(), "_alpha" ) ||
		0;
}

class LoadShaderVisitor : public Archive::Visitor
{
public:
void visit( const char* name ){
	IShader* shader = QERApp_Shader_ForName( CopiedString( StringRange( name, path_get_filename_base_end( name ) ) ).c_str() );
	shader->DecRef();
}
};

void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused );

GtkWidget* g_page_textures;

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
typedef const char* first_argument_type;

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

void TextureDirectory_loadTexture( const char* directory, const char* texture ){
	StringOutputStream name( 256 );
	name << directory << StringRange( texture, path_get_filename_base_end( texture ) );

	if ( texture_name_ignore( name.c_str() ) ) {
		return;
	}

	if ( !shader_valid( name.c_str() ) ) {
		globalWarningStream() << "Skipping invalid texture name: [" << name.c_str() << "]\n";
		return;
	}

	// if a texture is already in use to represent a shader, ignore it
	IShader* shader = QERApp_Shader_ForName( name.c_str() );
	shader->DecRef();
}
typedef ConstPointerCaller1<char, const char*, TextureDirectory_loadTexture> TextureDirectoryLoadTextureCaller;

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

void TextureBrowser_ShowDirectory( TextureBrowser& textureBrowser, const char* directory ){
	textureBrowser.m_searchedTags = false;
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
		GlobalShaderSystem().foreachShaderName( makeCallback1( TextureCategoryLoadShader( directory, shaders_count ) ) );
		globalOutputStream() << "Showing " << Unsigned( shaders_count ) << " shaders.\n";

		if ( g_pGameDescription->mGameType != "doom3" ) {
			// load remaining texture files

			StringOutputStream dirstring( 64 );
			dirstring << "textures/" << directory;

			Radiant_getImageModules().foreachModule( LoadTexturesByTypeVisitor( dirstring.c_str() ) );
		}
	}

	TextureBrowser_SetHideUnused( textureBrowser, false );

	TextureBrowser_updateTitle();
}

void TextureBrowser_ShowTagSearchResult( TextureBrowser& textureBrowser, const char* directory ){
	g_TextureBrowser_currentDirectory = directory;
	TextureBrowser_heightChanged( textureBrowser );

	std::size_t shaders_count;
	GlobalShaderSystem().foreachShaderName( makeCallback1( TextureCategoryLoadShader( directory, shaders_count ) ) );
	globalOutputStream() << "Showing " << Unsigned( shaders_count ) << " shaders.\n";

	if ( g_pGameDescription->mGameType != "doom3" ) {
		// load remaining texture files
		StringOutputStream dirstring( 64 );
		dirstring << "textures/" << directory;

		{
			LoadTexturesByTypeVisitor visitor( dirstring.c_str() );
			Radiant_getImageModules().foreachModule( visitor );
		}
	}

	// we'll display the newly loaded textures + all the ones already in use
	TextureBrowser_SetHideUnused( textureBrowser, false );
}


void TextureBrowser_hideUnusedExport( const BoolImportCallback& importer ){
	importer( GlobalTextureBrowser().m_hideUnused );
}

void TextureBrowser_showShadersExport( const BoolImportCallback& importer ){
	importer( GlobalTextureBrowser().m_showShaders );
}

void TextureBrowser_showTexturesExport( const BoolImportCallback& importer ){
	importer( GlobalTextureBrowser().m_showTextures );
}


void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused ){
	textureBrowser.m_hideUnused = hideUnused;

	textureBrowser.m_hideunused_item.update();

	TextureBrowser_heightChanged( textureBrowser );
	textureBrowser.m_originInvalid = true;
}

void TextureBrowser_ShowStartupShaders( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_startupShaders == STARTUPSHADERS_COMMON ) {
		TextureBrowser_ShowDirectory( textureBrowser, TextureBrowser_getComonShadersDir() );
	}
}


//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name ){
	TextureLayout layout;
	// scroll origin so the texture is completely on screen
	Texture_StartPos( layout );

	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t* q = shader->getTexture();
		if ( !q ) {
			break;
		}

		// we have found when texdef->name and the shader name match
		// NOTE: as everywhere else for our comparisons, we are not case sensitive
		if ( shader_equal( name, shader->getName() ) ) {
			//int textureHeight = (int)( q->height * ( (float)textureBrowser.m_textureScale / 100 ) ) + 2 * TextureBrowser_fontHeight( textureBrowser );
			int textureWidth, textureHeight;
			textureBrowser.getTextureWH( q, textureWidth, textureHeight );
			textureHeight += 2 * TextureBrowser_fontHeight( textureBrowser );


			int originy = TextureBrowser_getOriginY( textureBrowser );
			if ( y > originy ) {
				originy = y + 4;
			}

			if ( y - textureHeight < originy - textureBrowser.height ) {
				originy = ( y - textureHeight ) + textureBrowser.height;
			}

			TextureBrowser_setOriginY( textureBrowser, originy );
			return;
		}
	}
}

IShader* Texture_At( TextureBrowser& textureBrowser, int mx, int my ){
	my += TextureBrowser_getOriginY( textureBrowser ) - textureBrowser.height;

	TextureLayout layout;
	Texture_StartPos( layout );
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t  *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		int nWidth, nHeight;
		textureBrowser.getTextureWH( q, nWidth, nHeight );
		if ( mx > x && mx - x < nWidth
			 && my < y && y - my < nHeight + TextureBrowser_fontHeight( textureBrowser ) ) {
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
void SelectTexture( TextureBrowser& textureBrowser, int mx, int my, guint32 flags, bool texturizeSelection ){
	if ( ( flags & GDK_SHIFT_MASK ) == 0 ) {
		IShader* shader = Texture_At( textureBrowser, mx, my );
		if ( shader != 0 ) {
			TextureBrowser_SetSelectedShader( textureBrowser, shader->getName() );
			TextureBrowser_textureSelected( shader->getName() );

			if ( !FindTextureDialog_isOpen() && !textureBrowser.m_rmbSelected && !texturizeSelection ) {
				Select_SetShader_Undo( shader->getName() );
			}
		}
	}
}

/*
   ============================================================================

   MOUSE ACTIONS

   ============================================================================
 */

void TextureBrowser_trackingDelta( int x, int y, unsigned int state, void* data ){
	if ( y != 0 ) {
		TextureBrowser& textureBrowser = *reinterpret_cast<TextureBrowser*>( data );
		const int scale = ( state & GDK_SHIFT_MASK )? 4 : 1;
		const int originy = TextureBrowser_getOriginY( textureBrowser ) + y * scale;
		TextureBrowser_setOriginY( textureBrowser, originy );
		textureBrowser.m_move_amount += std::abs( y );
	}
}

void TextureBrowser_Tracking_MouseUp( TextureBrowser& textureBrowser ){
	if( textureBrowser.m_move_started ){
		textureBrowser.m_move_started = false;
		textureBrowser.m_freezePointer.unfreeze_pointer( textureBrowser.m_parent, false );
	}
}

void TextureBrowser_Tracking_MouseDown( TextureBrowser& textureBrowser ){
	if( textureBrowser.m_move_started ){
		TextureBrowser_Tracking_MouseUp( textureBrowser );
	}
	textureBrowser.m_move_started = true;
	textureBrowser.m_move_amount = 0;
	textureBrowser.m_freezePointer.freeze_pointer( textureBrowser.m_parent, textureBrowser.m_gl_widget, TextureBrowser_trackingDelta, &textureBrowser );
}

void TextureBrowser_Selection_MouseDown( TextureBrowser& textureBrowser, guint32 flags, int pointx, int pointy, bool texturizeSelection ){
	SelectTexture( textureBrowser, pointx, textureBrowser.height - 1 - pointy, flags, texturizeSelection );
}

void TextureBrowser_Selection_MouseUp( TextureBrowser& textureBrowser, guint32 flags, int pointx, int pointy ){
	if ( ( flags & GDK_SHIFT_MASK ) != 0 ) {
		IShader* shader = Texture_At( textureBrowser, pointx, textureBrowser.height - 1 - pointy );
		if ( shader != 0 ) {
			if ( shader->IsDefault() ) {
				globalWarningStream() << shader->getName() << " is not a shader, it's a texture.\n";
			}
			else{
				DoShaderView( shader->getShaderFileName(), shader->getName(), ( flags & GDK_CONTROL_MASK ) != 0 );
			}
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
   Texture_Draw
   TTimo: relying on the shaders list to display the textures
   we must query all qtexture_t* to manage and display through the IShaders interface
   this allows a plugin to completely override the texture system
   ============
 */
void Texture_Draw( TextureBrowser& textureBrowser ){
	const int fontHeight = TextureBrowser_fontHeight( textureBrowser );
	int originy = TextureBrowser_getOriginY( textureBrowser );

	glClearColor( textureBrowser.color_textureback[0],
				  textureBrowser.color_textureback[1],
				  textureBrowser.color_textureback[2],
				  0 );
	glViewport( 0, 0, textureBrowser.width, textureBrowser.height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glDisable( GL_DEPTH_TEST );
	if( GlobalOpenGL().GL_1_3() ) {
		glDisable( GL_MULTISAMPLE );
	}
	if ( g_TextureBrowser_enableAlpha ) {
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else {
		glDisable( GL_BLEND );
	}

	glOrtho( 0, textureBrowser.width, originy - textureBrowser.height, originy, -100, 100 );
	glEnable( GL_TEXTURE_2D );

	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	TextureLayout layout;
	Texture_StartPos( layout );
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		int nWidth, nHeight;
		textureBrowser.getTextureWH( q, nWidth, nHeight );

		// Is this texture visible?
		if ( ( y - nHeight - fontHeight < originy )
			 && ( y > originy - textureBrowser.height ) ) {
			glLineWidth( 1 );
			glDisable( GL_TEXTURE_2D );
			const float xf = x;
			const float yf = y - fontHeight;
			float xfMax = xf + 1.5 + nWidth;
			float xfMin = xf - 1.5;
			float yfMax = yf + 1.5;
			float yfMin = yf - nHeight - 1.5;
			#define TEXBRO_RENDER_BORDER \
				glBegin( GL_LINE_LOOP ); \
				glVertex2f( xfMin, yfMax ); \
				glVertex2f( xfMin, yfMin ); \
				glVertex2f( xfMax, yfMin ); \
				glVertex2f( xfMax, yfMax ); \
				glEnd();

			//selected texture
			if ( shader_equal( TextureBrowser_GetSelectedShader( textureBrowser ), shader->getName() ) ) {
				glLineWidth( 2 );
				if ( textureBrowser.m_rmbSelected ) {
					glColor3f( 0, 0, 1 );
				}
				else {
					glColor3f( 1, 0, 0 );
				}
				xfMax += .5;
				xfMin -= .5;
				yfMax += .5;
				yfMin -= .5;
				TEXBRO_RENDER_BORDER
			}
			// highlight in-use textures
			else if ( !textureBrowser.m_hideUnused && shader->IsInUse() ) {
				glColor3f( 0.5, 1, 0.5 );
				TEXBRO_RENDER_BORDER
			}
			// shader white border:
			else if ( !shader->IsDefault() ) {
				glColor3f( 1, 1, 1 );
				TEXBRO_RENDER_BORDER
			}

			// shader stipple:
			if ( !shader->IsDefault() ) {
				glEnable( GL_LINE_STIPPLE );
				glLineStipple( 1, 0xF000 );
				glColor3f( 0, 0, 0 );
				TEXBRO_RENDER_BORDER
				glDisable( GL_LINE_STIPPLE );
			}

			// draw checkerboard for transparent textures
 			if ( g_TextureBrowser_enableAlpha )
			{
				glBegin( GL_QUADS );
				for ( int i = 0; i < nHeight; i += 8 )
					for ( int j = 0; j < nWidth; j += 8 )
					{
						const unsigned char color = ( i + j ) / 8 % 2 ? 0x66 : 0x99;
						glColor3ub( color, color, color );
						const int left = j;
						const int right = std::min( j + 8, nWidth );
						const int top = i;
						const int bottom = std::min( i + 8, nHeight );
						glVertex2i( x + right, y - nHeight - fontHeight + top );
						glVertex2i( x + left,  y - nHeight - fontHeight + top );
						glVertex2i( x + left,  y - nHeight - fontHeight + bottom );
						glVertex2i( x + right, y - nHeight - fontHeight + bottom );
					}
				glEnd();
			}

			// Draw the texture
			glEnable( GL_TEXTURE_2D );
			glBindTexture( GL_TEXTURE_2D, q->texture_number );
			GlobalOpenGL_debugAssertNoErrors();
			glColor3f( 1, 1, 1 );
			glBegin( GL_QUADS );
			glTexCoord2i( 0, 0 );
			glVertex2i( x, y - fontHeight );
			glTexCoord2i( 1, 0 );
			glVertex2i( x + nWidth, y - fontHeight );
			glTexCoord2i( 1, 1 );
			glVertex2i( x + nWidth, y - fontHeight - nHeight );
			glTexCoord2i( 0, 1 );
			glVertex2i( x, y - fontHeight - nHeight );
			glEnd();

			// draw the texture name
			glDisable( GL_TEXTURE_2D );
//			glColor3f( 1, 1, 1 ); //already set

			glRasterPos2i( x, y - fontHeight + 3 );//+5

			// don't draw the directory name
			const char* name = shader->getName();
			name += strlen( name );
			while ( name != shader->getName() && *( name - 1 ) != '/' && *( name - 1 ) != '\\' )
				name--;

			GlobalOpenGL().drawString( name );
		}
	}

	// reset the current texture
	glBindTexture( GL_TEXTURE_2D, 0 );
	glDisable( GL_BLEND );
	//qglFinish();
}

void TextureBrowser_queueDraw( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_gl_widget != 0 ) {
		gtk_widget_queue_draw( textureBrowser.m_gl_widget );
	}
}


void TextureBrowser_setScale( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_textureScale = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_setUniformSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_setUniformMinSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureMinSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_MouseWheel( TextureBrowser& textureBrowser, bool bUp ){
	int originy = TextureBrowser_getOriginY( textureBrowser );

	if ( bUp ) {
		originy += int(textureBrowser.m_mouseWheelScrollIncrement);
	}
	else
	{
		originy -= int(textureBrowser.m_mouseWheelScrollIncrement);
	}

	TextureBrowser_setOriginY( textureBrowser, originy );
}

XmlTagBuilder TagBuilder;

enum
{
	TAG_COLUMN = 0,
	N_COLUMNS = 1
};

void BuildStoreAssignedTags( GtkListStore* store, const char* shader, TextureBrowser* textureBrowser ){
	GtkTreeIter iter;

	gtk_list_store_clear( store );

	std::vector<CopiedString> assigned_tags;
	TagBuilder.GetShaderTags( shader, assigned_tags );

	for ( size_t i = 0; i < assigned_tags.size(); i++ )
	{
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter, TAG_COLUMN, assigned_tags[i].c_str(), -1 );
	}
}

void BuildStoreAvailableTags(   GtkListStore* storeAvailable,
								GtkListStore* storeAssigned,
								const std::set<CopiedString>& allTags,
								TextureBrowser* textureBrowser ){
	GtkTreeIter iterAssigned;
	GtkTreeIter iterAvailable;
	std::set<CopiedString>::const_iterator iterAll;
	gchar* tag_assigned;

	gtk_list_store_clear( storeAvailable );

	bool row = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( storeAssigned ), &iterAssigned ) != 0;

	if ( !row ) { // does the shader have tags assigned?
		for ( iterAll = allTags.begin(); iterAll != allTags.end(); ++iterAll )
		{
			gtk_list_store_append( storeAvailable, &iterAvailable );
			gtk_list_store_set( storeAvailable, &iterAvailable, TAG_COLUMN, ( *iterAll ).c_str(), -1 );
		}
	}
	else
	{
		while ( row ) // available tags = all tags - assigned tags
		{
			gtk_tree_model_get( GTK_TREE_MODEL( storeAssigned ), &iterAssigned, TAG_COLUMN, &tag_assigned, -1 );

			for ( iterAll = allTags.begin(); iterAll != allTags.end(); ++iterAll )
			{
				if ( strcmp( (char*)tag_assigned, ( *iterAll ).c_str() ) != 0 ) {
					gtk_list_store_append( storeAvailable, &iterAvailable );
					gtk_list_store_set( storeAvailable, &iterAvailable, TAG_COLUMN, ( *iterAll ).c_str(), -1 );
				}
				else
				{
					row = gtk_tree_model_iter_next( GTK_TREE_MODEL( storeAssigned ), &iterAssigned ) != 0;

					if ( row ) {
						gtk_tree_model_get( GTK_TREE_MODEL( storeAssigned ), &iterAssigned, TAG_COLUMN, &tag_assigned, -1 );
					}
				}
			}
		}
	}
}

gboolean TextureBrowser_button_press( GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		gtk_widget_grab_focus( widget );
		if ( event->button == 3 ) {
			TextureBrowser_Tracking_MouseDown( *textureBrowser );
			textureBrowser->m_move_start = BasicVector2<int>( event->x, event->y );
		}
		else if ( event->button == 1 || event->button == 2 ) {
			TextureBrowser_Selection_MouseDown( *textureBrowser, event->state, static_cast<int>( event->x ), static_cast<int>( event->y ), event->button == 2 );

			if ( GlobalTextureBrowser().m_tags ) {
				textureBrowser->m_rmbSelected = false;
				gtk_widget_hide( textureBrowser->m_tag_frame );
			}
		}
	}
	/* loads directory, containing active shader + focuses on it */
	else if ( event->type == GDK_2BUTTON_PRESS && event->button == 1 && !TextureBrowser::wads ) {
		const StringRange range( strchr( textureBrowser->shader.c_str(), '/' ) + 1, strrchr( textureBrowser->shader.c_str(), '/' ) + 1 );
		if( range.last > range.first ){
			const CopiedString dir = range;
			ScopeDisableScreenUpdates disableScreenUpdates( dir.c_str(), "Loading Textures" );
			TextureBrowser_ShowDirectory( *textureBrowser, dir.c_str() );
			TextureBrowser_Focus( *textureBrowser, textureBrowser->shader.c_str() );
			TextureBrowser_queueDraw( *textureBrowser );
		}
	}
	else if ( event->type == GDK_2BUTTON_PRESS && event->button == 3 ) {
		ScopeDisableScreenUpdates disableScreenUpdates( TextureBrowser_getComonShadersDir(), "Loading Textures" );
		TextureBrowser_ShowDirectory( *textureBrowser, TextureBrowser_getComonShadersDir() );
		TextureBrowser_queueDraw( *textureBrowser );
	}
	return FALSE;
}

gboolean TextureBrowser_button_release( GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		if ( event->button == 3 ) {
			TextureBrowser_Tracking_MouseUp( *textureBrowser );
			if ( GlobalTextureBrowser().m_tags && textureBrowser->m_move_amount < 16 ) {
				textureBrowser->m_rmbSelected = true;
				TextureBrowser_Selection_MouseDown( *textureBrowser, event->state, textureBrowser->m_move_start.x(), textureBrowser->m_move_start.y(), false );

				BuildStoreAssignedTags( textureBrowser->m_assigned_store, textureBrowser->shader.c_str(), textureBrowser );
				BuildStoreAvailableTags( textureBrowser->m_available_store, textureBrowser->m_assigned_store, textureBrowser->m_all_tags, textureBrowser );
				textureBrowser->m_heightChanged = true;
				gtk_widget_show( textureBrowser->m_tag_frame );

				process_gui();

				TextureBrowser_Focus( *textureBrowser, textureBrowser->shader.c_str() );
			}
		}
		else if ( event->button == 1 ) {
			TextureBrowser_Selection_MouseUp( *textureBrowser, event->state, static_cast<int>( event->x ), static_cast<int>( event->y ) );
		}
	}
	return FALSE;
}

gboolean TextureBrowser_motion( GtkWidget *widget, GdkEventMotion *event, TextureBrowser* textureBrowser ){
	return FALSE;
}

gboolean TextureBrowser_scroll( GtkWidget* widget, GdkEventScroll* event, TextureBrowser* textureBrowser ){
	gtk_widget_grab_focus( widget );
	if( !gtk_window_is_active( textureBrowser->m_parent ) )
		gtk_window_present( textureBrowser->m_parent );

	if ( event->direction == GDK_SCROLL_UP ) {
		TextureBrowser_MouseWheel( *textureBrowser, true );
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		TextureBrowser_MouseWheel( *textureBrowser, false );
	}
	return FALSE;
}

void TextureBrowser_scrollChanged( void* data, gdouble value ){
	//globalOutputStream() << "vertical scroll\n";
	TextureBrowser_setOriginY( *reinterpret_cast<TextureBrowser*>( data ), -(int)value );
}

static void TextureBrowser_verticalScroll( GtkAdjustment *adjustment, TextureBrowser* textureBrowser ){
	textureBrowser->m_scrollAdjustment.value_changed( adjustment->value );
}

void TextureBrowser_updateScroll( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_showTextureScrollbar ) {
		int totalHeight = TextureBrowser_TotalHeight( textureBrowser );

		totalHeight = std::max( totalHeight, textureBrowser.height );

		GtkAdjustment *vadjustment = gtk_range_get_adjustment( GTK_RANGE( textureBrowser.m_texture_scroll ) );

		vadjustment->value = -TextureBrowser_getOriginY( textureBrowser );
		vadjustment->page_size = textureBrowser.height;
		vadjustment->page_increment = textureBrowser.height / 2;
		vadjustment->step_increment = 20;
		vadjustment->lower = 0;
		vadjustment->upper = totalHeight;

		g_signal_emit_by_name( G_OBJECT( vadjustment ), "changed" );
	}
}

gboolean TextureBrowser_size_allocate( GtkWidget* widget, GtkAllocation* allocation, TextureBrowser* textureBrowser ){
	textureBrowser->width = allocation->width;
	textureBrowser->height = allocation->height;
	TextureBrowser_heightChanged( *textureBrowser );
	textureBrowser->m_originInvalid = true;
	TextureBrowser_queueDraw( *textureBrowser );
	return FALSE;
}

gboolean TextureBrowser_expose( GtkWidget* widget, GdkEventExpose* event, TextureBrowser* textureBrowser ){
	if ( glwidget_make_current( textureBrowser->m_gl_widget ) != FALSE ) {
		GlobalOpenGL_debugAssertNoErrors();
		TextureBrowser_evaluateHeight( *textureBrowser );
		Texture_Draw( *textureBrowser );
		GlobalOpenGL_debugAssertNoErrors();
		glwidget_swap_buffers( textureBrowser->m_gl_widget );
	}
	return FALSE;
}


TextureBrowser g_TextureBrowser;

TextureBrowser& GlobalTextureBrowser(){
	return g_TextureBrowser;
}


void TextureBrowser_ToggleHideUnused(){
	TextureBrowser_SetHideUnused( g_TextureBrowser, !g_TextureBrowser.m_hideUnused );
}

void TextureGroups_constructTreeModel( TextureGroups groups, GtkTreeStore* store ){
	GtkTreeIter iter, child;

	TextureGroups::const_iterator i = groups.begin();
	while ( i != groups.end() )
	{
		const char* dirName = ( *i ).c_str();
		const char* firstUnderscore = strchr( dirName, '_' );
		StringRange dirRoot( dirName, ( firstUnderscore == 0 ) ? dirName : firstUnderscore + 1 );

		TextureGroups::const_iterator next = i;
		++next;
		if ( firstUnderscore != 0
			 && next != groups.end()
			 && string_equal_start( ( *next ).c_str(), dirRoot ) ) {
			gtk_tree_store_append( store, &iter, NULL );
			gtk_tree_store_set( store, &iter, 0, CopiedString( StringRange( dirName, firstUnderscore ) ).c_str(), 1 , "", -1 );

			// keep going...
			while ( i != groups.end() && string_equal_start( ( *i ).c_str(), dirRoot ) )
			{
				gtk_tree_store_append( store, &child, &iter );
				gtk_tree_store_set( store, &child, 0, ( *i ).c_str(), 1, ( *i ).c_str(), -1 );
				++i;
			}
		}
		else
		{
			gtk_tree_store_append( store, &iter, NULL );
			gtk_tree_store_set( store, &iter, 0, dirName, 1, dirName, -1 );
			++i;
		}
	}
}

void TextureGroups_constructTreeModel_childless( TextureGroups groups, GtkTreeStore* store ){
	GtkTreeIter iter;

	TextureGroups::const_iterator i = groups.begin();
	while ( i != groups.end() )
	{
		const char* dirName = ( *i ).c_str();
		const char* pakName = strrchr( dirName, '/' );
		const char* pakNameEnd = strrchr( dirName, '.' );
		ASSERT_MESSAGE( pakName != 0 && pakNameEnd != 0 && pakNameEnd > pakName, "interesting wad path" );
		{
			gtk_tree_store_append( store, &iter, NULL );
			gtk_tree_store_set( store, &iter, 0, CopiedString( StringRange( pakName + 1, pakNameEnd ) ).c_str(), 1, dirName, -1 );
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
	GtkTreeStore* store = gtk_tree_store_new( 2, G_TYPE_STRING, G_TYPE_STRING ); /* 0=display name;1=load path */
	if( !TextureBrowser::wads )
		TextureGroups_constructTreeModel( groups, store );
	else
		TextureGroups_constructTreeModel_childless( groups, store );

	gtk_tree_view_set_model( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTree ), GTK_TREE_MODEL( store ) );

	g_object_unref( G_OBJECT( store ) );
}

void TextureBrowser_constructTreeStoreTags(){
	GtkTreeStore* store = gtk_tree_store_new( 1, G_TYPE_STRING );
	GtkTreeModel* model = GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list );

	gtk_tree_view_set_model( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags ), model );

	g_object_unref( G_OBJECT( store ) );
}

void TreeView_onRowActivated( GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col, gpointer userdata ){
	GtkTreeIter iter;

	GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ) );

	if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
		gchar dirName[1024];

		gchar* buffer;
		gtk_tree_model_get( model, &iter, 1, &buffer, -1 );
		strcpy( dirName, buffer );
		g_free( buffer );

		if( string_empty( dirName ) ) //empty = directory group root
			return;

		g_TextureBrowser.m_searchedTags = false;

		if ( !TextureBrowser::wads ) {
			strcat( dirName, "/" );
		}

		ScopeDisableScreenUpdates disableScreenUpdates( dirName, "Loading Textures" );
		TextureBrowser_ShowDirectory( GlobalTextureBrowser(), dirName );
		TextureBrowser_queueDraw( GlobalTextureBrowser() );
		//deactivate, so SPACE and RETURN wont be broken for 2d
		gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( treeview ) ) ), NULL );
	}
}

static gboolean TextureBrowser_tree_view_set_tooltip_query_cb( GtkWidget* widget, gint x, gint y, gboolean keyboard_tip, GtkTooltip* tooltip, gpointer data ){
	GtkTreeIter iter;
	GtkTreePath* path;
	GtkTreeModel* model;
	GtkTreeView* tree_view = GTK_TREE_VIEW( widget );
	if( !gtk_tree_view_get_tooltip_context( GTK_TREE_VIEW( widget ), &x, &y, keyboard_tip, &model, &path, &iter ) )
		return FALSE;
	gchar* buffer;
	gtk_tree_model_get( model, &iter, 1, &buffer, -1 );
	gtk_tooltip_set_text( tooltip, buffer );
	gtk_tree_view_set_tooltip_row( tree_view, tooltip, path );
	g_free( buffer );
	gtk_tree_path_free( path );
	return TRUE;
}

void TextureBrowser_createTreeViewTree(){
	GtkCellRenderer* renderer;
	g_TextureBrowser.m_treeViewTree = gtk_tree_view_new();
	GtkTreeView* treeview = GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTree );
	//gtk_tree_view_set_enable_search( treeview, FALSE );

	gtk_tree_view_set_headers_visible( treeview, FALSE );
	g_signal_connect( treeview, "row-activated", (GCallback) TreeView_onRowActivated, NULL );

	renderer = gtk_cell_renderer_text_new();
	//g_object_set( G_OBJECT( renderer ), "ellipsize", PANGO_ELLIPSIZE_START, NULL );
	gtk_tree_view_insert_column_with_attributes( treeview, -1, "", renderer, "text", 0, NULL );

	if( TextureBrowser::wads ){
		//gtk_tree_view_set_tooltip_column( treeview, 1 );
		/* set own tooltip callback, since convenience function is using markup */
		g_signal_connect( treeview, "query-tooltip", G_CALLBACK( TextureBrowser_tree_view_set_tooltip_query_cb ), NULL );
		gtk_widget_set_has_tooltip( g_TextureBrowser.m_treeViewTree, TRUE );
	}

	TextureBrowser_constructTreeStore();
}

void TextureBrowser_addTag();
void TextureBrowser_renameTag();
void TextureBrowser_deleteTag();

void TextureBrowser_createContextMenu( GtkWidget *treeview, GdkEventButton *event ){
	GtkWidget* menu = gtk_menu_new();

	GtkWidget* menuitem = gtk_menu_item_new_with_label( "Add tag" );
	g_signal_connect( menuitem, "activate", (GCallback)TextureBrowser_addTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	menuitem = gtk_menu_item_new_with_label( "Rename tag" );
	g_signal_connect( menuitem, "activate", (GCallback)TextureBrowser_renameTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	menuitem = gtk_menu_item_new_with_label( "Delete tag" );
	g_signal_connect( menuitem, "activate", (GCallback)TextureBrowser_deleteTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	gtk_widget_show_all( menu );

	gtk_menu_popup( GTK_MENU( menu ), NULL, NULL, NULL, NULL,
					( event != NULL ) ? event->button : 0,
					gdk_event_get_time( (GdkEvent*)event ) );
}

void TextureBrowser_searchTags();

gboolean TreeViewTags_onButtonPressed( GtkWidget *treeview, GdkEventButton *event ){
	if ( event->type == GDK_BUTTON_PRESS && event->button == 3 ) {
		GtkTreePath *path;
		GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( treeview ) );

		if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( treeview ), event->x, event->y, &path, NULL, NULL, NULL ) ) {
			gtk_tree_selection_unselect_all( selection );
			gtk_tree_selection_select_path( selection, path );
			gtk_tree_path_free( path );
		}

		TextureBrowser_createContextMenu( treeview, event );
		return TRUE;
	}
	if( event->type == GDK_2BUTTON_PRESS && event->button == 1 ){
		TextureBrowser_searchTags();
		return TRUE;
	}
	return FALSE;
}

void TextureBrowser_createTreeViewTags(){
	GtkCellRenderer* renderer;
	g_TextureBrowser.m_treeViewTags = gtk_tree_view_new();
	GtkTreeView* treeview = GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags );
//	gtk_tree_view_set_enable_search( treeview, FALSE );

	g_signal_connect( treeview, "button-press-event", (GCallback)TreeViewTags_onButtonPressed, NULL );

	gtk_tree_view_set_headers_visible( treeview, FALSE );

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes( treeview, -1, "", renderer, "text", 0, NULL );

	TextureBrowser_constructTreeStoreTags();
}

GtkMenuItem* TextureBrowser_constructViewMenu( GtkMenu* menu ){
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic( "_View" );

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_check_menu_item_with_mnemonic( menu, "Hide _Unused", "ShowInUse" );
	create_menu_item_with_mnemonic( menu, "Show All", "ShowAllTextures" );
	menu_separator( menu );


	// we always want to show shaders but don't want a "Show Shaders" menu for doom3 and .wad file games
	if ( g_pGameDescription->mGameType == "doom3" || TextureBrowser::wads ) {
		g_TextureBrowser.m_showShaders = true;
	}
	else
	{
		create_check_menu_item_with_mnemonic( menu, "Show shaders", "ToggleShowShaders" );
		create_check_menu_item_with_mnemonic( menu, "Show textures", "ToggleShowTextures" );
		menu_separator( menu );
	}

	if ( g_TextureBrowser.m_tags ) {
		create_menu_item_with_mnemonic( menu, "Show Untagged", "ShowUntagged" );
	}
	if ( g_pGameDescription->mGameType != "doom3" && !TextureBrowser::wads ) {
		create_check_menu_item_with_mnemonic( menu, "ShaderList Only", "ToggleShowShaderlistOnly" );
	}
	if ( !TextureBrowser::wads ) {
		create_check_menu_item_with_mnemonic( menu, "Hide Image Missing", "FilterNotex" );
		menu_separator( menu );
	}

	create_check_menu_item_with_mnemonic( menu, "Fixed Size", "FixedSize" );
	create_check_menu_item_with_mnemonic( menu, "Transparency", "EnableAlpha" );

	if ( !TextureBrowser::wads ) {
		menu_separator( menu );
		g_TextureBrowser.m_shader_info_item = GTK_WIDGET( create_menu_item_with_mnemonic( menu, "Shader Info", "ShaderInfo" ) );
		gtk_widget_set_sensitive( g_TextureBrowser.m_shader_info_item, FALSE );
	}

	return textures_menu_item;
}

void Popup_View_Menu( GtkWidget *widget, GtkMenu *menu ){
	gtk_menu_popup( menu, NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time() );
}
#if 0
GtkMenuItem* TextureBrowser_constructToolsMenu( GtkMenu* menu ){
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic( "_Tools" );

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_menu_item_with_mnemonic( menu, "Flush & Reload Shaders", "RefreshShaders" );
	create_menu_item_with_mnemonic( menu, "Find / Replace...", "FindReplaceTextures" );

	return textures_menu_item;
}
#endif
GtkMenuItem* TextureBrowser_constructTagsMenu( GtkMenu* menu ){
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic( "T_ags" );

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_menu_item_with_mnemonic( menu, "Add tag", "AddTag" );
	create_menu_item_with_mnemonic( menu, "Rename tag", "RenameTag" );
	create_menu_item_with_mnemonic( menu, "Delete tag", "DeleteTag" );
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Copy tags from selected", "CopyTag" );
	create_menu_item_with_mnemonic( menu, "Paste tags to selected", "PasteTag" );

	return textures_menu_item;
}

gboolean TextureBrowser_tagMoveHelper( GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, GSList** selected ){
	g_assert( selected != NULL );

	GtkTreeRowReference* rowref = gtk_tree_row_reference_new( model, path );
	*selected = g_slist_append( *selected, rowref );

	return FALSE;
}

void TextureBrowser_assignTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag_assigned;

	GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_available_tree ) );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		for ( node = selected; node != NULL; node = node->next )
		{
			GtkTreePath* path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter( GTK_TREE_MODEL( g_TextureBrowser.m_available_store ), &iter, path ) ) {
					gtk_tree_model_get( GTK_TREE_MODEL( g_TextureBrowser.m_available_store ), &iter, TAG_COLUMN, &tag_assigned, -1 );
					if ( !TagBuilder.CheckShaderTag( g_TextureBrowser.shader.c_str() ) ) {
						// create a custom shader/texture entry
						IShader* ishader = QERApp_Shader_ForName( g_TextureBrowser.shader.c_str() );
						CopiedString filename = ishader->getShaderFileName();

						if ( filename.empty() ) {
							// it's a texture
							TagBuilder.AddShaderNode( g_TextureBrowser.shader.c_str(), CUSTOM, TEXTURE );
						}
						else {
							// it's a shader
							TagBuilder.AddShaderNode( g_TextureBrowser.shader.c_str(), CUSTOM, SHADER );
						}
						ishader->DecRef();
					}
					TagBuilder.AddShaderTag( g_TextureBrowser.shader.c_str(), (char*)tag_assigned, TAG );

					gtk_list_store_remove( g_TextureBrowser.m_available_store, &iter );
					gtk_list_store_append( g_TextureBrowser.m_assigned_store, &iter );
					gtk_list_store_set( g_TextureBrowser.m_assigned_store, &iter, TAG_COLUMN, (char*)tag_assigned, -1 );
				}
			}
		}

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		// Save changes
		TagBuilder.SaveXmlDoc();
	}
	g_slist_free( selected );
}

void TextureBrowser_removeTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag;

	GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_assigned_tree ) );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		for ( node = selected; node != NULL; node = node->next )
		{
			GtkTreePath* path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter( GTK_TREE_MODEL( g_TextureBrowser.m_assigned_store ), &iter, path ) ) {
					gtk_tree_model_get( GTK_TREE_MODEL( g_TextureBrowser.m_assigned_store ), &iter, TAG_COLUMN, &tag, -1 );
					TagBuilder.DeleteShaderTag( g_TextureBrowser.shader.c_str(), tag );
					gtk_list_store_remove( g_TextureBrowser.m_assigned_store, &iter );
				}
			}
		}

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		// Update the "available tags list"
		BuildStoreAvailableTags( g_TextureBrowser.m_available_store, g_TextureBrowser.m_assigned_store, g_TextureBrowser.m_all_tags, &g_TextureBrowser );

		// Save changes
		TagBuilder.SaveXmlDoc();
	}
	g_slist_free( selected );
}

void TextureBrowser_buildTagList(){
	GtkTreeIter treeIter;
	gtk_list_store_clear( g_TextureBrowser.m_all_tags_list );

	std::set<CopiedString>::iterator iter;

	for ( iter = g_TextureBrowser.m_all_tags.begin(); iter != g_TextureBrowser.m_all_tags.end(); ++iter )
	{
		gtk_list_store_append( g_TextureBrowser.m_all_tags_list, &treeIter );
		gtk_list_store_set( g_TextureBrowser.m_all_tags_list, &treeIter, TAG_COLUMN, ( *iter ).c_str(), -1 );
	}
}

void TextureBrowser_searchTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag;
	char buffer[256];
	char tags_searched[256];

	GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags ) );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		strcpy( buffer, "/root/*/*[tag='" );
		strcpy( tags_searched, "[TAGS] " );

		for ( node = selected; node != NULL; node = node->next )
		{
			GtkTreePath* path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iter, path ) ) {
					gtk_tree_model_get( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iter, TAG_COLUMN, &tag, -1 );

					strcat( buffer, tag );
					strcat( tags_searched, tag );
					if ( node != g_slist_last( node ) ) {
						strcat( buffer, "' and tag='" );
						strcat( tags_searched, ", " );
					}
				}
			}
		}

		strcat( buffer, "']" );

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		g_TextureBrowser.m_found_shaders.clear(); // delete old list
		TagBuilder.TagSearch( buffer, g_TextureBrowser.m_found_shaders );

		if ( !g_TextureBrowser.m_found_shaders.empty() ) { // found something
			size_t shaders_found = g_TextureBrowser.m_found_shaders.size();

			globalOutputStream() << "Found " << (unsigned int)shaders_found << " textures and shaders with " << tags_searched << "\n";
			ScopeDisableScreenUpdates disableScreenUpdates( "Searching...", "Loading Textures" );

			std::set<CopiedString>::iterator iter;

			for ( iter = g_TextureBrowser.m_found_shaders.begin(); iter != g_TextureBrowser.m_found_shaders.end(); iter++ )
			{
				std::string path = ( *iter ).c_str();
				size_t pos = path.find_last_of( "/", path.size() );
				std::string name = path.substr( pos + 1, path.size() );
				path = path.substr( 0, pos + 1 );
				TextureDirectory_loadTexture( path.c_str(), name.c_str() );
			}
		}
		TextureBrowser_SetHideUnused( g_TextureBrowser, false );
		g_TextureBrowser.m_searchedTags = true;
		g_TextureBrowser_currentDirectory = tags_searched;

		g_TextureBrowser.m_nTotalHeight = 0;
		TextureBrowser_setOriginY( g_TextureBrowser, 0 );
		TextureBrowser_heightChanged( g_TextureBrowser );
		TextureBrowser_updateTitle();
	}
	g_slist_free( selected );
}

void TextureBrowser_toggleSearchButton(){
	gint page = gtk_notebook_get_current_page( GTK_NOTEBOOK( g_TextureBrowser.m_tag_notebook ) );

	if ( page == 0 ) { // tag page
		gtk_widget_show_all( g_TextureBrowser.m_search_button );
	}
	else {
		gtk_widget_hide_all( g_TextureBrowser.m_search_button );
	}
}

void TextureBrowser_constructTagNotebook(){
	g_TextureBrowser.m_tag_notebook = gtk_notebook_new();
	GtkWidget* labelTags = gtk_label_new( "Tags" );
	GtkWidget* labelTextures = gtk_label_new( "Textures" );

	gtk_notebook_append_page( GTK_NOTEBOOK( g_TextureBrowser.m_tag_notebook ), g_TextureBrowser.m_scr_win_tree, labelTextures );
	gtk_notebook_append_page( GTK_NOTEBOOK( g_TextureBrowser.m_tag_notebook ), g_TextureBrowser.m_scr_win_tags, labelTags );

	g_signal_connect( G_OBJECT( g_TextureBrowser.m_tag_notebook ), "switch-page", G_CALLBACK( TextureBrowser_toggleSearchButton ), NULL );

	gtk_widget_show_all( g_TextureBrowser.m_tag_notebook );
}

void TextureBrowser_constructSearchButton(){
	GtkTooltips* tooltips = gtk_tooltips_new();

	GtkWidget* image = gtk_image_new_from_stock( GTK_STOCK_FIND, GTK_ICON_SIZE_SMALL_TOOLBAR );
	g_TextureBrowser.m_search_button = gtk_button_new();
	g_signal_connect( G_OBJECT( g_TextureBrowser.m_search_button ), "clicked", G_CALLBACK( TextureBrowser_searchTags ), NULL );
	gtk_tooltips_set_tip( GTK_TOOLTIPS( tooltips ), g_TextureBrowser.m_search_button, "Search with selected tags", "Search with selected tags" );
	gtk_container_add( GTK_CONTAINER( g_TextureBrowser.m_search_button ), image );
}

void TextureBrowser_checkTagFile(){
	const char SHADERTAG_FILE[] = "shadertags.xml";
	CopiedString default_filename, rc_filename;
	StringOutputStream stream( 256 );

	stream << LocalRcPath_get();
	stream << SHADERTAG_FILE;
	rc_filename = stream.c_str();

	if ( file_exists( rc_filename.c_str() ) ) {
		g_TextureBrowser.m_tags = TagBuilder.OpenXmlDoc( rc_filename.c_str() );

		if ( g_TextureBrowser.m_tags ) {
			globalOutputStream() << "Loading tag file " << rc_filename.c_str() << ".\n";
		}
	}
	else
	{
		// load default tagfile
		stream.clear();
		stream << g_pGameDescription->mGameToolsPath.c_str();
		stream << SHADERTAG_FILE;
		default_filename = stream.c_str();

		if ( file_exists( default_filename.c_str() ) ) {
			g_TextureBrowser.m_tags = TagBuilder.OpenXmlDoc( default_filename.c_str(), rc_filename.c_str() );

			if ( g_TextureBrowser.m_tags ) {
				globalOutputStream() << "Loading default tag file " << default_filename.c_str() << ".\n";
			}
		}
		else
		{
			globalWarningStream() << "Unable to find default tag file " << default_filename.c_str() << ". No tag support. Plugins -> ShaderPlug -> Create tag file: to start using texture tags\n";
		}
	}
}

void TextureBrowser_SetNotex(){
	StringOutputStream name( 256 );
	name << GlobalRadiant().getAppPath() << "bitmaps/notex.png";
	g_notex = name.c_str();

	name.clear();
	name << GlobalRadiant().getAppPath() << "bitmaps/shadernotex.png";
	g_shadernotex = name.c_str();
}

void TextureBrowser_filterChanged( GtkEditable *editable, TextureBrowser* textureBrowser ){
	gtk_entry_set_icon_sensitive( GTK_ENTRY( editable ), GTK_ENTRY_ICON_SECONDARY, ( gtk_entry_get_text_length( GTK_ENTRY( editable ) ) > 0 ) );
	TextureBrowser_heightChanged( *textureBrowser );
	textureBrowser->m_originInvalid = true;
}

void TextureBrowser_filterIconPress( GtkEntry* entry, gint position, GdkEventButton* event, gpointer user_data ) {
	if( position == GTK_ENTRY_ICON_PRIMARY ){
		GlobalToggles_find( "SearchFromStart" ).m_command.m_callback();
	}
	else{
		gtk_entry_set_text( entry, "" );
	}
}

static gboolean TextureBrowser_filterKeypress( GtkEntry* widget, GdkEventKey* event, gpointer user_data ){
	if ( event->keyval == GDK_Escape ) {
		gtk_entry_set_text( GTK_ENTRY( widget ), "" );
		return TRUE;
	}
	return FALSE;
}

gboolean TextureBrowser_filterEntryFocus( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	gtk_widget_grab_focus( widget );
	return FALSE;
}

gboolean TextureBrowser_filterEntryUnfocus( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( widget ) ), NULL );
	return FALSE;
}

void TextureBrowser_filterSetModeIcon( GtkEntry* entry ){
	if( g_TextureBrowser_filter_searchFromStart ){
		gtk_entry_set_icon_from_stock( entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_MEDIA_PLAY );
	}
	else{
		gtk_entry_set_icon_from_stock( entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_ABOUT );
	}
}


GtkWidget* TextureBrowser_constructWindow( GtkWindow* toplevel ){
	// The gl_widget and the tag assignment frame should be packed into a GtkVPaned with the slider
	// position stored in local.pref. gtk_paned_get_position() and gtk_paned_set_position() don't
	// seem to work in gtk 2.4 and the arrow buttons don't handle GTK_FILL, so here's another thing
	// for the "once-the-gtk-libs-are-updated-TODO-list" :x

	TextureBrowser_checkTagFile();
	TextureBrowser_SetNotex();

	GlobalShaderSystem().setActiveShadersChangedNotify( ReferenceCaller<TextureBrowser, TextureBrowser_activeShadersChanged>( g_TextureBrowser ) );

	g_TextureBrowser.m_parent = toplevel;

	GtkWidget* table = gtk_table_new( 3, 3, FALSE );
	GtkWidget* frame_table = NULL;
	GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
	gtk_table_attach( GTK_TABLE( table ), vbox, 0, 1, 0, 3, GTK_FILL, GTK_FILL, 0, 0 );
	gtk_widget_show( vbox );

	GtkToolbar* toolbar;

	{ // menu bar
		GtkWidget* menu_view = gtk_menu_new();
		TextureBrowser_constructViewMenu( GTK_MENU( menu_view ) );
		gtk_menu_set_title( GTK_MENU( menu_view ), "View" );

		toolbar = GTK_TOOLBAR( gtk_toolbar_new() );
		gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( toolbar ), FALSE, FALSE, 0 );

		//view menu button
		GtkButton* button = GTK_BUTTON( gtk_button_new() );
		button_set_icon( button, "texbro_view.png" );
		gtk_widget_show( GTK_WIDGET( button ) );
		gtk_button_set_relief( button, GTK_RELIEF_NONE );
		gtk_widget_set_size_request( GTK_WIDGET( button ), 24, 24 );
		GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
		GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_DEFAULT );
		gtk_toolbar_append_element( toolbar, GTK_TOOLBAR_CHILD_WIDGET, GTK_WIDGET( button ), "", "View", "", 0, 0, 0 );
		g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( Popup_View_Menu ), menu_view );

		//show detached menu over floating tex bro
		gtk_menu_attach_to_widget( GTK_MENU( menu_view ), GTK_WIDGET( button ), NULL );

		button = toolbar_append_button( toolbar, "Find / Replace...", "texbro_gtk-find-and-replace.png", "FindReplaceTextures" );
		gtk_widget_set_size_request( GTK_WIDGET( button ), 22, 22 );

		button = toolbar_append_button( toolbar, "Flush & Reload Shaders", "texbro_refresh.png", "RefreshShaders" );
		gtk_widget_set_size_request( GTK_WIDGET( button ), 22, 22 );
		gtk_widget_show( GTK_WIDGET( toolbar ) );
	}
	{//filter entry
		GtkWidget* entry = gtk_entry_new();
		gtk_widget_set_size_request( GTK_WIDGET( entry ), 64, -1 );
		gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( entry ), FALSE, FALSE, 0 );
		gtk_entry_set_icon_from_stock( GTK_ENTRY( entry ), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR );
		gtk_entry_set_icon_sensitive( GTK_ENTRY( entry ), GTK_ENTRY_ICON_SECONDARY, FALSE );
		TextureBrowser_filterSetModeIcon( GTK_ENTRY( entry ) );
		gtk_entry_set_icon_tooltip_text( GTK_ENTRY( entry ), GTK_ENTRY_ICON_PRIMARY, "toggle match mode ( start / any position )" );
		gtk_widget_show( entry );
		g_TextureBrowser.m_filter_entry = entry;
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( TextureBrowser_filterChanged ), &g_TextureBrowser );
		g_signal_connect( G_OBJECT( entry ), "icon-press", G_CALLBACK( TextureBrowser_filterIconPress ), 0 );
		g_signal_connect( G_OBJECT( entry ), "key_press_event", G_CALLBACK( TextureBrowser_filterKeypress ), 0 );
		g_signal_connect( G_OBJECT( entry ), "enter_notify_event", G_CALLBACK( TextureBrowser_filterEntryFocus ), 0 );
		g_signal_connect( G_OBJECT( entry ), "leave_notify_event", G_CALLBACK( TextureBrowser_filterEntryUnfocus ), 0 );
	}
	{ // Texture TreeView
		g_TextureBrowser.m_scr_win_tree = gtk_scrolled_window_new( NULL, NULL );
		gtk_container_set_border_width( GTK_CONTAINER( g_TextureBrowser.m_scr_win_tree ), 0 );

		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( g_TextureBrowser.m_scr_win_tree ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

		gtk_widget_show( g_TextureBrowser.m_scr_win_tree );

		TextureBrowser_createTreeViewTree();

		//gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( g_TextureBrowser.m_scr_win_tree ), g_TextureBrowser.m_treeViewTree );
		gtk_container_add( GTK_CONTAINER( g_TextureBrowser.m_scr_win_tree ), g_TextureBrowser.m_treeViewTree ); //GtkTreeView has native scrolling support; should not be used with the GtkViewport proxy.
		gtk_widget_show( GTK_WIDGET( g_TextureBrowser.m_treeViewTree ) );
	}
	{ // gl_widget scrollbar
		GtkWidget* w = gtk_vscrollbar_new( GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 0, 1, 1, 0 ) ) );
		gtk_table_attach( GTK_TABLE( table ), w, 2, 3, 1, 2, GTK_SHRINK, GTK_FILL, 0, 0 );
		gtk_widget_show( w );
		g_TextureBrowser.m_texture_scroll = w;

		GtkAdjustment *vadjustment = gtk_range_get_adjustment( GTK_RANGE( g_TextureBrowser.m_texture_scroll ) );
		g_signal_connect( G_OBJECT( vadjustment ), "value_changed", G_CALLBACK( TextureBrowser_verticalScroll ), &g_TextureBrowser );

		widget_set_visible( g_TextureBrowser.m_texture_scroll, g_TextureBrowser.m_showTextureScrollbar );
	}
	{ // gl_widget
#if NV_DRIVER_GAMMA_BUG
		g_TextureBrowser.m_gl_widget = glwidget_new( TRUE );
#else
		g_TextureBrowser.m_gl_widget = glwidget_new( FALSE );
#endif
		gtk_widget_ref( g_TextureBrowser.m_gl_widget );

		gtk_widget_set_events( g_TextureBrowser.m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );
		GTK_WIDGET_SET_FLAGS( g_TextureBrowser.m_gl_widget, GTK_CAN_FOCUS );

		gtk_table_attach_defaults( GTK_TABLE( table ), g_TextureBrowser.m_gl_widget, 1, 2, 1, 2 );
		gtk_widget_show( g_TextureBrowser.m_gl_widget );

		g_TextureBrowser.m_sizeHandler = g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "size_allocate", G_CALLBACK( TextureBrowser_size_allocate ), &g_TextureBrowser );
		g_TextureBrowser.m_exposeHandler = g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "expose_event", G_CALLBACK( TextureBrowser_expose ), &g_TextureBrowser );

		g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "button_press_event", G_CALLBACK( TextureBrowser_button_press ), &g_TextureBrowser );
		g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "button_release_event", G_CALLBACK( TextureBrowser_button_release ), &g_TextureBrowser );
		g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "motion_notify_event", G_CALLBACK( TextureBrowser_motion ), &g_TextureBrowser );
		g_signal_connect( G_OBJECT( g_TextureBrowser.m_gl_widget ), "scroll_event", G_CALLBACK( TextureBrowser_scroll ), &g_TextureBrowser );
	}

	// tag stuff
	if ( g_TextureBrowser.m_tags ) {
		{ // fill tag GtkListStore
			g_TextureBrowser.m_all_tags_list = gtk_list_store_new( N_COLUMNS, G_TYPE_STRING );
			GtkTreeSortable* sortable = GTK_TREE_SORTABLE( g_TextureBrowser.m_all_tags_list );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			TagBuilder.GetAllTags( g_TextureBrowser.m_all_tags );
			TextureBrowser_buildTagList();
		}
		{ // tag menu bar
			GtkWidget* menu_tags = gtk_menu_new();
			gtk_menu_set_title( GTK_MENU( menu_tags ), "Tags" );
			TextureBrowser_constructTagsMenu( GTK_MENU( menu_tags ) );

			GtkButton* button = GTK_BUTTON( gtk_button_new() );
			button_set_icon( button, "texbro_tags.png" );
//			GtkWidget *label = gtk_label_new ( ">t" );
//			gtk_container_add( GTK_CONTAINER( button ), label );
//			gtk_widget_show( label );

			gtk_widget_show( GTK_WIDGET( button ) );
			gtk_button_set_relief( button, GTK_RELIEF_NONE );
			gtk_widget_set_size_request( GTK_WIDGET( button ), 22, 22 );
			GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
			GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_DEFAULT );
			gtk_toolbar_append_element( toolbar, GTK_TOOLBAR_CHILD_WIDGET, GTK_WIDGET( button ), "", "Tags", "", 0, 0, 0 );
			g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( Popup_View_Menu ), menu_tags );

			//show detached menu over floating tex bro and main wnd...
			gtk_menu_attach_to_widget( GTK_MENU( menu_tags ), GTK_WIDGET( button ), NULL );
		}
		{ // Tag TreeView
			g_TextureBrowser.m_scr_win_tags = gtk_scrolled_window_new( NULL, NULL );
			gtk_container_set_border_width( GTK_CONTAINER( g_TextureBrowser.m_scr_win_tags ), 0 );

			// vertical only scrolling for treeview
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( g_TextureBrowser.m_scr_win_tags ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			TextureBrowser_createTreeViewTags();

			GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags ) );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

			gtk_container_add( GTK_CONTAINER( g_TextureBrowser.m_scr_win_tags ), g_TextureBrowser.m_treeViewTags );
			gtk_widget_show( GTK_WIDGET( g_TextureBrowser.m_treeViewTags ) );
		}
		{ // Texture/Tag notebook
			TextureBrowser_constructTagNotebook();
			gtk_box_pack_start( GTK_BOX( vbox ), g_TextureBrowser.m_tag_notebook, TRUE, TRUE, 0 );
		}
		{ // Tag search button
			TextureBrowser_constructSearchButton();
			gtk_box_pack_end( GTK_BOX( vbox ), g_TextureBrowser.m_search_button, FALSE, FALSE, 0 );
		}
		{ // Tag frame
			frame_table = gtk_table_new( 3, 3, FALSE );

			g_TextureBrowser.m_tag_frame = gtk_frame_new( "Tag assignment" );
			gtk_frame_set_label_align( GTK_FRAME( g_TextureBrowser.m_tag_frame ), 0.5, 0.5 );
			gtk_frame_set_shadow_type( GTK_FRAME( g_TextureBrowser.m_tag_frame ), GTK_SHADOW_NONE );

			gtk_table_attach( GTK_TABLE( table ), g_TextureBrowser.m_tag_frame, 1, 3, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0 );

			gtk_widget_show( frame_table );

			gtk_container_add( GTK_CONTAINER( g_TextureBrowser.m_tag_frame ), frame_table );
		}
		{ // assigned tag list
			GtkWidget* scrolled_win = gtk_scrolled_window_new( NULL, NULL );
			gtk_container_set_border_width( GTK_CONTAINER( scrolled_win ), 0 );
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_win ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			g_TextureBrowser.m_assigned_store = gtk_list_store_new( N_COLUMNS, G_TYPE_STRING );

			GtkTreeSortable* sortable = GTK_TREE_SORTABLE( g_TextureBrowser.m_assigned_store );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

			g_TextureBrowser.m_assigned_tree = gtk_tree_view_new_with_model( GTK_TREE_MODEL( g_TextureBrowser.m_assigned_store ) );
			g_object_unref( G_OBJECT( g_TextureBrowser.m_assigned_store ) );
			g_signal_connect( g_TextureBrowser.m_assigned_tree, "row-activated", (GCallback) TextureBrowser_removeTags, NULL );
			gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( g_TextureBrowser.m_assigned_tree ), FALSE );

			GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_assigned_tree ) );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

			GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes( "", renderer, "text", TAG_COLUMN, NULL );
			gtk_tree_view_append_column( GTK_TREE_VIEW( g_TextureBrowser.m_assigned_tree ), column );
			gtk_widget_show( g_TextureBrowser.m_assigned_tree );

			gtk_widget_show( scrolled_win );
			gtk_container_add( GTK_CONTAINER( scrolled_win ), g_TextureBrowser.m_assigned_tree );

			gtk_table_attach( GTK_TABLE( frame_table ), scrolled_win, 0, 1, 1, 3, GTK_FILL, GTK_FILL, 0, 0 );
		}
		{ // available tag list
			GtkWidget* scrolled_win = gtk_scrolled_window_new( NULL, NULL );
			gtk_container_set_border_width( GTK_CONTAINER( scrolled_win ), 0 );
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_win ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			g_TextureBrowser.m_available_store = gtk_list_store_new( N_COLUMNS, G_TYPE_STRING );
			GtkTreeSortable* sortable = GTK_TREE_SORTABLE( g_TextureBrowser.m_available_store );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

			g_TextureBrowser.m_available_tree = gtk_tree_view_new_with_model( GTK_TREE_MODEL( g_TextureBrowser.m_available_store ) );
			g_object_unref( G_OBJECT( g_TextureBrowser.m_available_store ) );
			g_signal_connect( g_TextureBrowser.m_available_tree, "row-activated", (GCallback) TextureBrowser_assignTags, NULL );
			gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( g_TextureBrowser.m_available_tree ), FALSE );

			GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_available_tree ) );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

			GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes( "", renderer, "text", TAG_COLUMN, NULL );
			gtk_tree_view_append_column( GTK_TREE_VIEW( g_TextureBrowser.m_available_tree ), column );
			gtk_widget_show( g_TextureBrowser.m_available_tree );

			gtk_widget_show( scrolled_win );
			gtk_container_add( GTK_CONTAINER( scrolled_win ), g_TextureBrowser.m_available_tree );

			gtk_table_attach( GTK_TABLE( frame_table ), scrolled_win, 2, 3, 1, 3, GTK_FILL, GTK_FILL, 0, 0 );
		}
		{ // tag arrow buttons
			GtkWidget* m_btn_left = gtk_button_new();
			GtkWidget* m_btn_right = gtk_button_new();
			GtkWidget* m_arrow_left = gtk_arrow_new( GTK_ARROW_LEFT, GTK_SHADOW_OUT );
			GtkWidget* m_arrow_right = gtk_arrow_new( GTK_ARROW_RIGHT, GTK_SHADOW_OUT );
			gtk_container_add( GTK_CONTAINER( m_btn_left ), m_arrow_left );
			gtk_container_add( GTK_CONTAINER( m_btn_right ), m_arrow_right );

			// workaround. the size of the tag frame depends of the requested size of the arrow buttons.
			gtk_widget_set_size_request( m_arrow_left, -1, 68 );
			gtk_widget_set_size_request( m_arrow_right, -1, 68 );

			gtk_table_attach( GTK_TABLE( frame_table ), m_btn_left, 1, 2, 1, 2, GTK_SHRINK, GTK_EXPAND, 0, 0 );
			gtk_table_attach( GTK_TABLE( frame_table ), m_btn_right, 1, 2, 2, 3, GTK_SHRINK, GTK_EXPAND, 0, 0 );

			g_signal_connect( G_OBJECT( m_btn_left ), "clicked", G_CALLBACK( TextureBrowser_assignTags ), NULL );
			g_signal_connect( G_OBJECT( m_btn_right ), "clicked", G_CALLBACK( TextureBrowser_removeTags ), NULL );

			gtk_widget_show( m_btn_left );
			gtk_widget_show( m_btn_right );
			gtk_widget_show( m_arrow_left );
			gtk_widget_show( m_arrow_right );
		}
		{ // tag fram labels
			GtkWidget* m_lbl_assigned = gtk_label_new( "Assigned" );
			GtkWidget* m_lbl_unassigned = gtk_label_new( "Available" );

			gtk_table_attach( GTK_TABLE( frame_table ), m_lbl_assigned, 0, 1, 0, 1, GTK_EXPAND, GTK_SHRINK, 0, 0 );
			gtk_table_attach( GTK_TABLE( frame_table ), m_lbl_unassigned, 2, 3, 0, 1, GTK_EXPAND, GTK_SHRINK, 0, 0 );

			gtk_widget_show( m_lbl_assigned );
			gtk_widget_show( m_lbl_unassigned );
		}
	}
	else { // no tag support, show the texture tree only
		gtk_box_pack_start( GTK_BOX( vbox ), g_TextureBrowser.m_scr_win_tree, TRUE, TRUE, 0 );
	}

	//prevent focusing on filter entry or tex dirs treeview after click on tab of floating group dialog (np, if called via hotkey)
	gtk_container_set_focus_chain( GTK_CONTAINER( table ), NULL );

	return table;
}

void TextureBrowser_destroyWindow(){
	GlobalShaderSystem().setActiveShadersChangedNotify( Callback() );

	g_signal_handler_disconnect( G_OBJECT( g_TextureBrowser.m_gl_widget ), g_TextureBrowser.m_sizeHandler );
	g_signal_handler_disconnect( G_OBJECT( g_TextureBrowser.m_gl_widget ), g_TextureBrowser.m_exposeHandler );

	gtk_widget_unref( g_TextureBrowser.m_gl_widget );
}

const Vector3& TextureBrowser_getBackgroundColour( TextureBrowser& textureBrowser ){
	return textureBrowser.color_textureback;
}

void TextureBrowser_setBackgroundColour( TextureBrowser& textureBrowser, const Vector3& colour ){
	textureBrowser.color_textureback = colour;
	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_selectionHelper( GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, GSList** selected ){
	g_assert( selected != NULL );

	gchar* name;
	gtk_tree_model_get( model, iter, TAG_COLUMN, &name, -1 );
	*selected = g_slist_append( *selected, name );
}

void TextureBrowser_shaderInfo(){
	const char* name = TextureBrowser_GetSelectedShader();
	IShader* shader = QERApp_Shader_ForName( name );

	DoShaderInfoDlg( name, shader->getShaderFileName(), "Shader Info" );

	shader->DecRef();
}

void TextureBrowser_addTag(){
	CopiedString tag;

	EMessageBoxReturn result = DoShaderTagDlg( &tag, "Add shader tag" );

	if ( result == eIDOK && !tag.empty() ) {
		GtkTreeIter iter, iter2;
		g_TextureBrowser.m_all_tags.insert( tag.c_str() );
		gtk_list_store_append( g_TextureBrowser.m_available_store, &iter );
		gtk_list_store_set( g_TextureBrowser.m_available_store, &iter, TAG_COLUMN, tag.c_str(), -1 );

		// Select the currently added tag in the available list
		GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_available_tree ) );
		gtk_tree_selection_select_iter( selection, &iter );

		gtk_list_store_append( g_TextureBrowser.m_all_tags_list, &iter2 );
		gtk_list_store_set( g_TextureBrowser.m_all_tags_list, &iter2, TAG_COLUMN, tag.c_str(), -1 );
	}
}

void TextureBrowser_renameTag(){
	/* WORKAROUND: The tag treeview is set to GTK_SELECTION_MULTIPLE. Because
	   gtk_tree_selection_get_selected() doesn't work with GTK_SELECTION_MULTIPLE,
	   we need to count the number of selected rows first and use
	   gtk_tree_selection_selected_foreach() then to go through the list of selected
	   rows (which always containins a single row).
	 */

	GSList* selected = NULL;

	GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags ) );
	gtk_tree_selection_selected_foreach( selection, GtkTreeSelectionForeachFunc( TextureBrowser_selectionHelper ), &selected );

	if ( g_slist_length( selected ) == 1 ) { // we only rename a single tag
		CopiedString newTag;
		EMessageBoxReturn result = DoShaderTagDlg( &newTag, "Rename shader tag" );

		if ( result == eIDOK && !newTag.empty() ) {
			GtkTreeIter iterList;
			gchar* rowTag;
			gchar* oldTag = (char*)selected->data;

			bool row = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterList ) != 0;

			while ( row )
			{
				gtk_tree_model_get( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterList, TAG_COLUMN, &rowTag, -1 );

				if ( strcmp( rowTag, oldTag ) == 0 ) {
					gtk_list_store_set( g_TextureBrowser.m_all_tags_list, &iterList, TAG_COLUMN, newTag.c_str(), -1 );
				}
				row = gtk_tree_model_iter_next( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterList ) != 0;
			}

			TagBuilder.RenameShaderTag( oldTag, newTag.c_str() );

			g_TextureBrowser.m_all_tags.erase( CopiedString( oldTag ) );
			g_TextureBrowser.m_all_tags.insert( newTag );

			BuildStoreAssignedTags( g_TextureBrowser.m_assigned_store, g_TextureBrowser.shader.c_str(), &g_TextureBrowser );
			BuildStoreAvailableTags( g_TextureBrowser.m_available_store, g_TextureBrowser.m_assigned_store, g_TextureBrowser.m_all_tags, &g_TextureBrowser );
		}
	}
	else
	{
		gtk_MessageBox( GTK_WIDGET( g_TextureBrowser.m_parent ), "Select a single tag for renaming." );
	}
}

void TextureBrowser_deleteTag(){
	GSList* selected = NULL;

	GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( g_TextureBrowser.m_treeViewTags ) );
	gtk_tree_selection_selected_foreach( selection, GtkTreeSelectionForeachFunc( TextureBrowser_selectionHelper ), &selected );

	if ( g_slist_length( selected ) == 1 ) { // we only delete a single tag
		EMessageBoxReturn result = gtk_MessageBox( GTK_WIDGET( g_TextureBrowser.m_parent ), "Are you sure you want to delete the selected tag?", "Delete Tag", eMB_YESNO, eMB_ICONQUESTION );

		if ( result == eIDYES ) {
			GtkTreeIter iterSelected;
			gchar *rowTag;

			gchar* tagSelected = (char*)selected->data;

			bool row = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterSelected ) != 0;

			while ( row )
			{
				gtk_tree_model_get( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterSelected, TAG_COLUMN, &rowTag, -1 );

				if ( strcmp( rowTag, tagSelected ) == 0 ) {
					gtk_list_store_remove( g_TextureBrowser.m_all_tags_list, &iterSelected );
					break;
				}
				row = gtk_tree_model_iter_next( GTK_TREE_MODEL( g_TextureBrowser.m_all_tags_list ), &iterSelected ) != 0;
			}

			TagBuilder.DeleteTag( tagSelected );
			g_TextureBrowser.m_all_tags.erase( CopiedString( tagSelected ) );

			BuildStoreAssignedTags( g_TextureBrowser.m_assigned_store, g_TextureBrowser.shader.c_str(), &g_TextureBrowser );
			BuildStoreAvailableTags( g_TextureBrowser.m_available_store, g_TextureBrowser.m_assigned_store, g_TextureBrowser.m_all_tags, &g_TextureBrowser );
		}
	}
	else {
		gtk_MessageBox( GTK_WIDGET( g_TextureBrowser.m_parent ), "Select a single tag for deletion." );
	}
}

void TextureBrowser_copyTag(){
	g_TextureBrowser.m_copied_tags.clear();
	TagBuilder.GetShaderTags( g_TextureBrowser.shader.c_str(), g_TextureBrowser.m_copied_tags );
}

void TextureBrowser_pasteTag(){
	IShader* ishader = QERApp_Shader_ForName( g_TextureBrowser.shader.c_str() );
	CopiedString shader = g_TextureBrowser.shader.c_str();

	if ( !TagBuilder.CheckShaderTag( shader.c_str() ) ) {
		CopiedString shaderFile = ishader->getShaderFileName();
		if ( shaderFile.empty() ) {
			// it's a texture
			TagBuilder.AddShaderNode( shader.c_str(), CUSTOM, TEXTURE );
		}
		else
		{
			// it's a shader
			TagBuilder.AddShaderNode( shader.c_str(), CUSTOM, SHADER );
		}

		for ( size_t i = 0; i < g_TextureBrowser.m_copied_tags.size(); ++i )
		{
			TagBuilder.AddShaderTag( shader.c_str(), g_TextureBrowser.m_copied_tags[i].c_str(), TAG );
		}
	}
	else
	{
		for ( size_t i = 0; i < g_TextureBrowser.m_copied_tags.size(); ++i )
		{
			if ( !TagBuilder.CheckShaderTag( shader.c_str(), g_TextureBrowser.m_copied_tags[i].c_str() ) ) {
				// the tag doesn't exist - let's add it
				TagBuilder.AddShaderTag( shader.c_str(), g_TextureBrowser.m_copied_tags[i].c_str(), TAG );
			}
		}
	}

	ishader->DecRef();

	TagBuilder.SaveXmlDoc();
	BuildStoreAssignedTags( g_TextureBrowser.m_assigned_store, shader.c_str(), &g_TextureBrowser );
	BuildStoreAvailableTags( g_TextureBrowser.m_available_store, g_TextureBrowser.m_assigned_store, g_TextureBrowser.m_all_tags, &g_TextureBrowser );
}

void RefreshShaders(){
	g_TextureBrowser_currentDirectory = "";
	g_TextureBrowser.m_searchedTags = false;
	TextureBrowser_updateTitle();

	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Shaders" );
	GlobalShaderSystem().refresh();
	TextureBrowser_constructTreeStore(); /* texturebrowser tree update on vfs restart */
	UpdateAllWindows();
}

void TextureBrowser_ToggleShowShaders(){
	g_TextureBrowser.m_showShaders ^= 1;
	g_TextureBrowser.m_showshaders_item.update();

	g_TextureBrowser.m_heightChanged = true;
	g_TextureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( g_TextureBrowser );
}

void TextureBrowser_ToggleShowTextures(){
	g_TextureBrowser.m_showTextures ^= 1;
	g_TextureBrowser.m_showtextures_item.update();

	g_TextureBrowser.m_heightChanged = true;
	g_TextureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( g_TextureBrowser );
}

void TextureBrowser_ToggleShowShaderListOnly(){
	g_TextureBrowser_shaderlistOnly ^= 1;
	g_TextureBrowser.m_showshaderlistonly_item.update();

	TextureBrowser_constructTreeStore();
}

void TextureBrowser_showAll(){
	g_TextureBrowser_currentDirectory = "";
	g_TextureBrowser.m_searchedTags = false;
//	TextureBrowser_SetHideUnused( g_TextureBrowser, false );
	TextureBrowser_ToggleHideUnused(); //toggle to show all used on the first hit and all on the second
	TextureBrowser_updateTitle();
}

void TextureBrowser_showUntagged(){
	EMessageBoxReturn result = gtk_MessageBox( GTK_WIDGET( g_TextureBrowser.m_parent ), "WARNING! This function might need a lot of memory and time. Are you sure you want to use it?", "Show Untagged", eMB_YESNO, eMB_ICONWARNING );

	if ( result == eIDYES ) {
		g_TextureBrowser.m_found_shaders.clear();
		TagBuilder.GetUntagged( g_TextureBrowser.m_found_shaders );
		std::set<CopiedString>::iterator iter;

		ScopeDisableScreenUpdates disableScreenUpdates( "Searching untagged textures...", "Loading Textures" );

		for ( iter = g_TextureBrowser.m_found_shaders.begin(); iter != g_TextureBrowser.m_found_shaders.end(); iter++ )
		{
			std::string path = ( *iter ).c_str();
			size_t pos = path.find_last_of( "/", path.size() );
			std::string name = path.substr( pos + 1, path.size() );
			path = path.substr( 0, pos + 1 );
			TextureDirectory_loadTexture( path.c_str(), name.c_str() );
			globalErrorStream() << path.c_str() << name.c_str() << "\n";
		}

		g_TextureBrowser_currentDirectory = "Untagged";
		TextureBrowser_queueDraw( GlobalTextureBrowser() );
		TextureBrowser_heightChanged( g_TextureBrowser );
		TextureBrowser_updateTitle();
	}
}

void TextureBrowser_FixedSize(){
	g_TextureBrowser_fixedSize ^= 1;
	GlobalTextureBrowser().m_fixedsize_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
}

void TextureBrowser_FilterNotex(){
	g_TextureBrowser_filterNotex ^= 1;
	GlobalTextureBrowser().m_filternotex_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
}

void TextureBrowser_EnableAlpha(){
	g_TextureBrowser_enableAlpha ^= 1;
	GlobalTextureBrowser().m_enablealpha_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
}

void TextureBrowser_filter_searchFromStart(){
	g_TextureBrowser_filter_searchFromStart ^= 1;
	GlobalTextureBrowser().m_filter_searchFromStart_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
	TextureBrowser_filterSetModeIcon( GTK_ENTRY( GlobalTextureBrowser().m_filter_entry ) );
}


void TextureBrowser_exportTitle( const StringImportCallback& importer ){
	StringOutputStream buffer( 64 );
	buffer << "Textures: ";
	if ( !string_empty( g_TextureBrowser_currentDirectory.c_str() ) ) {
		buffer << g_TextureBrowser_currentDirectory.c_str();
	}
	else
	{
		buffer << "all";
	}
	importer( buffer.c_str() );
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
typedef ReferenceCaller1<TextureBrowser, int, TextureScaleImport> TextureScaleImportCaller;

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
typedef ReferenceCaller1<TextureBrowser, const IntImportCallback&, TextureScaleExport> TextureScaleExportCaller;

void UniformTextureSizeImport( TextureBrowser& textureBrowser, int value ){
	if ( value >= 16 )
		TextureBrowser_setUniformSize( textureBrowser, value );
}
typedef ReferenceCaller1<TextureBrowser, int, UniformTextureSizeImport> UniformTextureSizeImportCaller;

void UniformTextureMinSizeImport( TextureBrowser& textureBrowser, int value ){
	if ( value >= 16 )
		TextureBrowser_setUniformMinSize( textureBrowser, value );
}
typedef ReferenceCaller1<TextureBrowser, int, UniformTextureMinSizeImport> UniformTextureMinSizeImportCaller;

void TextureBrowser_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox(
		"", "Texture scrollbar",
		TextureBrowserImportShowScrollbarCaller( GlobalTextureBrowser() ),
		BoolExportCaller( GlobalTextureBrowser().m_showTextureScrollbar )
		);
	{
		const char* texture_scale[] = { "10%", "25%", "50%", "100%", "200%" };
		page.appendCombo(
			"Texture Thumbnail Scale",
			STRING_ARRAY_RANGE( texture_scale ),
			IntImportCallback( TextureScaleImportCaller( GlobalTextureBrowser() ) ),
			IntExportCallback( TextureScaleExportCaller( GlobalTextureBrowser() ) )
			);
	}
	page.appendSpinner( "Thumbnails Max Size", GlobalTextureBrowser().m_uniformTextureSize, 160.0, 16, 8192 );
	page.appendSpinner( "Thumbnails Min Size", GlobalTextureBrowser().m_uniformTextureMinSize, 48.0, 16, 8192 );
	page.appendEntry( "Mousewheel Increment", GlobalTextureBrowser().m_mouseWheelScrollIncrement );
	{
		const char* startup_shaders[] = { "None", TextureBrowser_getComonShadersName() };
		page.appendCombo( "Load Shaders at Startup", reinterpret_cast<int&>( GlobalTextureBrowser().m_startupShaders ), STRING_ARRAY_RANGE( startup_shaders ) );
	}
	{
		StringOutputStream sstream( 256 );
		sstream << "Hide nonShaders in " << TextureBrowser_getComonShadersDir() << " folder";
		page.appendCheckBox(
			"", sstream.c_str(),
			GlobalTextureBrowser().m_hideNonShadersInCommon
			);
	}
}
void TextureBrowser_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Texture Browser", "Texture Browser Preferences" ) );
	TextureBrowser_constructPreferences( page );
}
void TextureBrowser_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, TextureBrowser_constructPage>() );
}


#include "preferencesystem.h"
#include "stringio.h"

typedef ReferenceCaller1<TextureBrowser, std::size_t, TextureBrowser_setScale> TextureBrowserSetScaleCaller;



void TextureClipboard_textureSelected( const char* shader );

void TextureBrowser_Construct(){
	GlobalCommands_insert( "ShaderInfo", FreeCaller<TextureBrowser_shaderInfo>() );
	GlobalCommands_insert( "ShowUntagged", FreeCaller<TextureBrowser_showUntagged>() );
	GlobalCommands_insert( "AddTag", FreeCaller<TextureBrowser_addTag>() );
	GlobalCommands_insert( "RenameTag", FreeCaller<TextureBrowser_renameTag>() );
	GlobalCommands_insert( "DeleteTag", FreeCaller<TextureBrowser_deleteTag>() );
	GlobalCommands_insert( "CopyTag", FreeCaller<TextureBrowser_copyTag>() );
	GlobalCommands_insert( "PasteTag", FreeCaller<TextureBrowser_pasteTag>() );
	GlobalCommands_insert( "RefreshShaders", FreeCaller<RefreshShaders>() );
	GlobalToggles_insert( "ShowInUse", FreeCaller<TextureBrowser_ToggleHideUnused>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_hideunused_item ), Accelerator( 'U' ) );
	GlobalCommands_insert( "ShowAllTextures", FreeCaller<TextureBrowser_showAll>(), Accelerator( 'A', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "ToggleTextures", FreeCaller<TextureBrowser_toggleShow>(), Accelerator( 'T' ) );
	GlobalToggles_insert( "ToggleShowShaders", FreeCaller<TextureBrowser_ToggleShowShaders>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_showshaders_item ) );
	GlobalToggles_insert( "ToggleShowTextures", FreeCaller<TextureBrowser_ToggleShowTextures>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_showtextures_item ) );
	GlobalToggles_insert( "ToggleShowShaderlistOnly", FreeCaller<TextureBrowser_ToggleShowShaderListOnly>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_showshaderlistonly_item ) );
	GlobalToggles_insert( "FixedSize", FreeCaller<TextureBrowser_FixedSize>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_fixedsize_item ) );
	GlobalToggles_insert( "FilterNotex", FreeCaller<TextureBrowser_FilterNotex>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_filternotex_item ) );
	GlobalToggles_insert( "EnableAlpha", FreeCaller<TextureBrowser_EnableAlpha>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_enablealpha_item ) );
	GlobalToggles_insert( "SearchFromStart", FreeCaller<TextureBrowser_filter_searchFromStart>(), ToggleItem::AddCallbackCaller( g_TextureBrowser.m_filter_searchFromStart_item ) );

	GlobalPreferenceSystem().registerPreference( "TextureScale",
												 makeSizeStringImportCallback( TextureBrowserSetScaleCaller( g_TextureBrowser ) ),
												 SizeExportStringCaller( g_TextureBrowser.m_textureScale )
												 );
	GlobalPreferenceSystem().registerPreference( "UniformTextureSize",
												makeIntStringImportCallback(UniformTextureSizeImportCaller(g_TextureBrowser)),
												IntExportStringCaller(g_TextureBrowser.m_uniformTextureSize) );
	GlobalPreferenceSystem().registerPreference( "UniformTextureMinSize",
												makeIntStringImportCallback(UniformTextureMinSizeImportCaller(g_TextureBrowser)),
												IntExportStringCaller(g_TextureBrowser.m_uniformTextureMinSize) );
	GlobalPreferenceSystem().registerPreference( "TextureScrollbar",
												 makeBoolStringImportCallback( TextureBrowserImportShowScrollbarCaller( g_TextureBrowser ) ),
												 BoolExportStringCaller( GlobalTextureBrowser().m_showTextureScrollbar )
												 );
	GlobalPreferenceSystem().registerPreference( "ShowShaders", BoolImportStringCaller( GlobalTextureBrowser().m_showShaders ), BoolExportStringCaller( GlobalTextureBrowser().m_showShaders ) );
	GlobalPreferenceSystem().registerPreference( "ShowTextures", BoolImportStringCaller( GlobalTextureBrowser().m_showTextures ), BoolExportStringCaller( GlobalTextureBrowser().m_showTextures ) );
	GlobalPreferenceSystem().registerPreference( "ShowShaderlistOnly", BoolImportStringCaller( g_TextureBrowser_shaderlistOnly ), BoolExportStringCaller( g_TextureBrowser_shaderlistOnly ) );
	GlobalPreferenceSystem().registerPreference( "FixedSize", BoolImportStringCaller( g_TextureBrowser_fixedSize ), BoolExportStringCaller( g_TextureBrowser_fixedSize ) );
	GlobalPreferenceSystem().registerPreference( "FilterNotex", BoolImportStringCaller( g_TextureBrowser_filterNotex ), BoolExportStringCaller( g_TextureBrowser_filterNotex ) );
	GlobalPreferenceSystem().registerPreference( "EnableAlpha", BoolImportStringCaller( g_TextureBrowser_enableAlpha ), BoolExportStringCaller( g_TextureBrowser_enableAlpha ) );
	GlobalPreferenceSystem().registerPreference( "SearchFromStart", BoolImportStringCaller( g_TextureBrowser_filter_searchFromStart ), BoolExportStringCaller( g_TextureBrowser_filter_searchFromStart ) );
	GlobalPreferenceSystem().registerPreference( "LoadShaders", IntImportStringCaller( reinterpret_cast<int&>( GlobalTextureBrowser().m_startupShaders ) ), IntExportStringCaller( reinterpret_cast<int&>( GlobalTextureBrowser().m_startupShaders ) ) );
	GlobalPreferenceSystem().registerPreference( "WheelMouseInc", SizeImportStringCaller( GlobalTextureBrowser().m_mouseWheelScrollIncrement ), SizeExportStringCaller( GlobalTextureBrowser().m_mouseWheelScrollIncrement ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors0", Vector3ImportStringCaller( GlobalTextureBrowser().color_textureback ), Vector3ExportStringCaller( GlobalTextureBrowser().color_textureback ) );
	GlobalPreferenceSystem().registerPreference( "HideNonShadersInCommon", BoolImportStringCaller( GlobalTextureBrowser().m_hideNonShadersInCommon ), BoolExportStringCaller( GlobalTextureBrowser().m_hideNonShadersInCommon ) );

	g_TextureBrowser.shader = texdef_name_default();

	TextureBrowser::wads = !string_empty( g_pGameDescription->getKeyValue( "show_wads" ) );

	Textures_setModeChangedNotify( ReferenceCaller<TextureBrowser, TextureBrowser_queueDraw>( g_TextureBrowser ) );

	TextureBrowser_registerPreferencesPage();

	GlobalShaderSystem().attach( g_ShadersObserver );

	TextureBrowser_textureSelected = TextureClipboard_textureSelected;
}
void TextureBrowser_Destroy(){
	GlobalShaderSystem().detach( g_ShadersObserver );

	Textures_setModeChangedNotify( Callback() );
}

GtkWidget* TextureBrowser_getGLWidget(){
	return GlobalTextureBrowser().m_gl_widget;
}
