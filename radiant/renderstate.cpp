/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include "renderstate.h"

#include "debugging/debugging.h"

#include "ishaders.h"
#include "irender.h"
#include "itextures.h"
#include "igl.h"
#include "iglrender.h"
#include "renderable.h"
#include "qerplugin.h"

#include <set>
#include <vector>
#include <list>
#include <map>

#include "math/matrix.h"
#include "math/aabb.h"
#include "generic/callback.h"
#include "texturelib.h"
#include "string/string.h"
#include "container/hashfunc.h"
#include "container/cache.h"
#include "generic/reference.h"
#include "moduleobservers.h"
#include "stream/filestream.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "preferences.h"

#include "xywindow.h"
#include "camwindow.h"



#define DEBUG_RENDER 0

inline void debug_string( const char* string ){
#if ( DEBUG_RENDER )
	globalOutputStream() << string << '\n';
#endif
}

inline void debug_int( const char* comment, int i ){
#if ( DEBUG_RENDER )
	globalOutputStream() << comment << ' ' << i << '\n';
#endif
}

inline void debug_colour( const char* comment ){
#if ( DEBUG_RENDER )
	Vector4 v;
	gl().glGetFloatv( GL_CURRENT_COLOR, reinterpret_cast<float*>( &v ) );
	globalOutputStream() << comment << " colour: "
	                     << v[0] << ' '
	                     << v[1] << ' '
	                     << v[2] << ' '
	                     << v[3];
	if ( gl().glIsEnabled( GL_COLOR_ARRAY ) ) {
		globalOutputStream() << " ARRAY";
	}
	if ( gl().glIsEnabled( GL_COLOR_MATERIAL ) ) {
		globalOutputStream() << " MATERIAL";
	}
	globalOutputStream() << '\n';
#endif
}

#include "timer.h"

StringOutputStream g_renderer_stats;
std::size_t g_count_prims;
std::size_t g_count_states;
std::size_t g_count_transforms;
Timer g_timer;

inline void count_prim(){
	++g_count_prims;
}

inline void count_state(){
	++g_count_states;
}

inline void count_transform(){
	++g_count_transforms;
}

void Renderer_ResetStats(){
	g_count_prims = 0;
	g_count_states = 0;
	g_count_transforms = 0;
	g_timer.start();
}

const char* Renderer_GetStats( int frame2frame ){
	return g_renderer_stats(
		"prims: ", g_count_prims,
		" | states: ", g_count_states,
		" | transforms: ", g_count_transforms,
		" | msec: ", g_timer.elapsed_msec(),
		" | f2f: ", frame2frame
	);
}


void printShaderLog( GLuint shader ){
	GLint log_length = 0;
	gl().glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &log_length );

	Array<char> log( log_length );
	gl().glGetShaderInfoLog( shader, log_length, &log_length, log.data() );

	globalErrorStream() << StringRange( log.begin(), log_length ) << '\n';
}

void printProgramLog( GLuint program ){
	GLint log_length = 0;
	gl().glGetProgramiv( program, GL_INFO_LOG_LENGTH, &log_length );

	Array<char> log( log_length );
	gl().glGetProgramInfoLog( program, log_length, &log_length, log.data() );

	globalErrorStream() << StringRange( log.begin(), log_length ) << '\n';
}

void createShader( GLuint program, const char* filename, GLenum type ){
	GLuint shader = gl().glCreateShader( type );
	GlobalOpenGL_debugAssertNoErrors();

	// load shader
	{
		std::size_t size = file_size( filename );
		FileInputStream file( filename );
		ASSERT_MESSAGE( !file.failed(), "failed to open " << makeQuoted( filename ) );
		Array<GLchar> buffer( size );
		size = file.read( reinterpret_cast<StreamBase::byte_type*>( buffer.data() ), size );

		const GLchar* string = buffer.data();
		GLint length = GLint( size );
		gl().glShaderSource( shader, 1, &string, &length );
	}

	// compile shader
	{
		gl().glCompileShader( shader );

		GLint compiled = 0;
		gl().glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );

		if ( !compiled ) {
			printShaderLog( shader );
		}

		ASSERT_MESSAGE( compiled, "shader compile failed: " << makeQuoted( filename ) );
	}

	// attach shader
	gl().glAttachShader( program, shader );

	gl().glDeleteShader( shader );

	GlobalOpenGL_debugAssertNoErrors();
}

void GLSLProgram_link( GLuint program ){
	gl().glLinkProgram( program );

	GLint linked = false;
	gl().glGetProgramiv( program, GL_LINK_STATUS, &linked );

	if ( !linked ) {
		printProgramLog( program );
	}

	ASSERT_MESSAGE( linked, "program link failed" );
}

void GLSLProgram_validate( GLuint program ){
	gl().glValidateProgram( program );

	GLint validated = false;
	gl().glGetProgramiv( program, GL_VALIDATE_STATUS, &validated );

	if ( !validated ) {
		printProgramLog( program );
	}

	ASSERT_MESSAGE( validated, "program validation failed" );
}

bool g_bumpGLSLPass_enabled = false;
bool g_depthfillPass_enabled = false;

class GLSLBumpProgram : public GLProgram
{
public:
	GLuint m_program;
	qtexture_t* m_light_attenuation_xy;
	qtexture_t* m_light_attenuation_z;
	GLint u_view_origin;
	GLint u_light_origin;
	GLint u_light_color;
	GLint u_bump_scale;
	GLint u_specular_exponent;

	GLSLBumpProgram() : m_program( 0 ), m_light_attenuation_xy( 0 ), m_light_attenuation_z( 0 ){
	}

	void create(){
		// create program
		m_program = gl().glCreateProgram();

		// create shader
		{
			StringOutputStream filename( 256 );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/lighting_DBS_omni_vp.glsl" ), GL_VERTEX_SHADER );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/lighting_DBS_omni_fp.glsl" ), GL_FRAGMENT_SHADER );
		}

		GLSLProgram_link( m_program );
		GLSLProgram_validate( m_program );

		gl().glUseProgram( m_program );

		gl().glBindAttribLocation( m_program, c_attr_TexCoord0, "attr_TexCoord0" );
		gl().glBindAttribLocation( m_program, c_attr_Tangent, "attr_Tangent" );
		gl().glBindAttribLocation( m_program, c_attr_Binormal, "attr_Binormal" );

		gl().glUniform1i( gl().glGetUniformLocation( m_program, "u_diffusemap" ), 0 );
		gl().glUniform1i( gl().glGetUniformLocation( m_program, "u_bumpmap" ), 1 );
		gl().glUniform1i( gl().glGetUniformLocation( m_program, "u_specularmap" ), 2 );
		gl().glUniform1i( gl().glGetUniformLocation( m_program, "u_attenuationmap_xy" ), 3 );
		gl().glUniform1i( gl().glGetUniformLocation( m_program, "u_attenuationmap_z" ), 4 );

		u_view_origin = gl().glGetUniformLocation( m_program, "u_view_origin" );
		u_light_origin = gl().glGetUniformLocation( m_program, "u_light_origin" );
		u_light_color = gl().glGetUniformLocation( m_program, "u_light_color" );
		u_bump_scale = gl().glGetUniformLocation( m_program, "u_bump_scale" );
		u_specular_exponent = gl().glGetUniformLocation( m_program, "u_specular_exponent" );

		gl().glUseProgram( 0 );

		GlobalOpenGL_debugAssertNoErrors();
	}

	void destroy(){
		gl().glDeleteProgram( m_program );
		m_program = 0;
	}

	void enable(){
		gl().glUseProgram( m_program );

		gl().glEnableVertexAttribArray( c_attr_TexCoord0 );
		gl().glEnableVertexAttribArray( c_attr_Tangent );
		gl().glEnableVertexAttribArray( c_attr_Binormal );

		GlobalOpenGL_debugAssertNoErrors();

		debug_string( "enable bump" );
		g_bumpGLSLPass_enabled = true;
	}

	void disable(){
		gl().glUseProgram( 0 );

		gl().glDisableVertexAttribArray( c_attr_TexCoord0 );
		gl().glDisableVertexAttribArray( c_attr_Tangent );
		gl().glDisableVertexAttribArray( c_attr_Binormal );

		GlobalOpenGL_debugAssertNoErrors();

		debug_string( "disable bump" );
		g_bumpGLSLPass_enabled = false;
	}

	void setParameters( const Vector3& viewer, const Matrix4& localToWorld, const Vector3& origin, const Vector3& colour, const Matrix4& world2light ){
		Matrix4 world2local( localToWorld );
		matrix4_affine_invert( world2local );

		Vector3 localLight( origin );
		matrix4_transform_point( world2local, localLight );

		Vector3 localViewer( viewer );
		matrix4_transform_point( world2local, localViewer );

		Matrix4 local2light( world2light );
		matrix4_multiply_by_matrix4( local2light, localToWorld ); // local->world->light

		gl().glUniform3f( u_view_origin, localViewer.x(), localViewer.y(), localViewer.z() );
		gl().glUniform3f( u_light_origin, localLight.x(), localLight.y(), localLight.z() );
		gl().glUniform3f( u_light_color, colour.x(), colour.y(), colour.z() );
		gl().glUniform1f( u_bump_scale, 1.0 );
		gl().glUniform1f( u_specular_exponent, 32.0 );

		gl().glActiveTexture( GL_TEXTURE3 );
		gl().glClientActiveTexture( GL_TEXTURE3 );

		gl().glMatrixMode( GL_TEXTURE );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &local2light ) );
		gl().glMatrixMode( GL_MODELVIEW );

		GlobalOpenGL_debugAssertNoErrors();
	}
};

GLSLBumpProgram g_bumpGLSL;


class GLSLDepthFillProgram : public GLProgram
{
public:
	GLuint m_program;

	void create(){
		// create program
		m_program = gl().glCreateProgram();

		// create shader
		{
			StringOutputStream filename( 256 );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/zfill_vp.glsl" ), GL_VERTEX_SHADER );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/zfill_fp.glsl" ), GL_FRAGMENT_SHADER );
		}

		GLSLProgram_link( m_program );
		GLSLProgram_validate( m_program );

		GlobalOpenGL_debugAssertNoErrors();
	}

	void destroy(){
		gl().glDeleteProgram( m_program );
		m_program = 0;
	}
	void enable(){
		gl().glUseProgram( m_program );
		GlobalOpenGL_debugAssertNoErrors();
		debug_string( "enable depthfill" );
		g_depthfillPass_enabled = true;
	}
	void disable(){
		gl().glUseProgram( 0 );
		GlobalOpenGL_debugAssertNoErrors();
		debug_string( "disable depthfill" );
		g_depthfillPass_enabled = false;
	}
	void setParameters( const Vector3& viewer, const Matrix4& localToWorld, const Vector3& origin, const Vector3& colour, const Matrix4& world2light ){
	}
};

GLSLDepthFillProgram g_depthFillGLSL;


class GLSLSkyboxProgram : public GLProgram
{
public:
	GLuint m_program;
	GLint u_view_origin;

	GLSLSkyboxProgram() : m_program( 0 ){
	}

	void create(){
		// create program
		m_program = gl().glCreateProgram();

		// create shader
		{
			StringOutputStream filename( 256 );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/skybox_vp.glsl" ), GL_VERTEX_SHADER );
			createShader( m_program, filename( GlobalRadiant().getAppPath(), "gl/skybox_fp.glsl" ), GL_FRAGMENT_SHADER );
		}

		GLSLProgram_link( m_program );
		GLSLProgram_validate( m_program );

		gl().glUseProgram( m_program );

		u_view_origin = gl().glGetUniformLocation( m_program, "u_view_origin" );

		gl().glUseProgram( 0 );

		GlobalOpenGL_debugAssertNoErrors();
	}

	void destroy(){
		gl().glDeleteProgram( m_program );
		m_program = 0;
	}

	void enable(){
		gl().glUseProgram( m_program );

		GlobalOpenGL_debugAssertNoErrors();

		debug_string( "enable skybox" );
	}

	void disable(){
		gl().glUseProgram( 0 );

		GlobalOpenGL_debugAssertNoErrors();

		debug_string( "disable skybox" );
	}

	void setParameters( const Vector3& viewer, const Matrix4& localToWorld, const Vector3& origin, const Vector3& colour, const Matrix4& world2light ){
		gl().glUniform3f( u_view_origin, viewer.x(), viewer.y(), viewer.z() );

		GlobalOpenGL_debugAssertNoErrors();
	}
};

GLSLSkyboxProgram g_skyboxGLSL;



bool g_vertexArray_enabled = false;
bool g_normalArray_enabled = false;
bool g_texcoordArray_enabled = false;
bool g_colorArray_enabled = false;

inline bool OpenGLState_less( const OpenGLState& self, const OpenGLState& other ){
	//! Sort by sort-order override.
	if ( self.m_sort != other.m_sort ) {
		return self.m_sort < other.m_sort;
	}
	//! Sort by texture handle.
	if ( self.m_texture != other.m_texture ) {
		return self.m_texture < other.m_texture;
	}
	if ( self.m_texture1 != other.m_texture1 ) {
		return self.m_texture1 < other.m_texture1;
	}
	if ( self.m_texture2 != other.m_texture2 ) {
		return self.m_texture2 < other.m_texture2;
	}
	if ( self.m_texture3 != other.m_texture3 ) {
		return self.m_texture3 < other.m_texture3;
	}
	if ( self.m_texture4 != other.m_texture4 ) {
		return self.m_texture4 < other.m_texture4;
	}
	if ( self.m_texture5 != other.m_texture5 ) {
		return self.m_texture5 < other.m_texture5;
	}
	if ( self.m_texture6 != other.m_texture6 ) {
		return self.m_texture6 < other.m_texture6;
	}
	if ( self.m_texture7 != other.m_texture7 ) {
		return self.m_texture7 < other.m_texture7;
	}
	if ( self.m_textureSkyBox != other.m_textureSkyBox ) {
		return self.m_textureSkyBox < other.m_textureSkyBox;
	}
	//! Sort by state bit-vector.
	if ( self.m_state != other.m_state ) {
		return self.m_state < other.m_state;
	}
	//! Comparing address makes sure states are never equal.
	return &self < &other;
}

void OpenGLState_constructDefault( OpenGLState& state ){
	state.m_state = RENDER_DEFAULT;

	state.m_texture = 0;
	state.m_texture1 = 0;
	state.m_texture2 = 0;
	state.m_texture3 = 0;
	state.m_texture4 = 0;
	state.m_texture5 = 0;
	state.m_texture6 = 0;
	state.m_texture7 = 0;
	state.m_textureSkyBox = 0;

	state.m_colour[0] = 1;
	state.m_colour[1] = 1;
	state.m_colour[2] = 1;
	state.m_colour[3] = 1;

	state.m_depthfunc = GL_LESS;

	state.m_blend_src = GL_SRC_ALPHA;
	state.m_blend_dst = GL_ONE_MINUS_SRC_ALPHA;

	state.m_alphafunc = GL_ALWAYS;
	state.m_alpharef = 0;

	state.m_linewidth = 1;
	state.m_pointsize = 1;

	state.m_linestipple_factor = 1;
	state.m_linestipple_pattern = 0xaaaa;

	state.m_fog = OpenGLFogState();
}




/// \brief A container of Renderable references.
/// May contain the same Renderable multiple times, with different transforms.
class OpenGLStateBucket
{
public:
	struct RenderTransform
	{
		const Matrix4* m_transform;
		const OpenGLRenderable *m_renderable;
		const RendererLight* m_light;

		RenderTransform( const OpenGLRenderable& renderable, const Matrix4& transform, const RendererLight* light )
			: m_transform( &transform ), m_renderable( &renderable ), m_light( light ){
		}
	};

	typedef std::vector<RenderTransform> Renderables;

private:

	OpenGLState m_state;
	Renderables m_renderables;

public:
	OpenGLStateBucket(){
	}
	void addRenderable( const OpenGLRenderable& renderable, const Matrix4& modelview, const RendererLight* light = 0 ){
		m_renderables.push_back( RenderTransform( renderable, modelview, light ) );
	}

	OpenGLState& state(){
		return m_state;
	}

	void render( OpenGLState& current, unsigned int globalstate, const Vector3& viewer );
};

#define LIGHT_SHADER_DEBUG 0

#if LIGHT_SHADER_DEBUG
typedef std::vector<Shader*> LightDebugShaders;
LightDebugShaders g_lightDebugShaders;
#endif

class OpenGLStateLess
{
public:
	bool operator()( const OpenGLState& self, const OpenGLState& other ) const {
		return OpenGLState_less( self, other );
	}
};

typedef ConstReference<OpenGLState> OpenGLStateReference;
typedef std::map<OpenGLStateReference, OpenGLStateBucket*, OpenGLStateLess> OpenGLStates;
OpenGLStates g_state_sorted;

class OpenGLStateBucketAdd
{
	OpenGLStateBucket& m_bucket;
	const OpenGLRenderable& m_renderable;
	const Matrix4& m_modelview;
public:
	using func = void(const RendererLight&);

	OpenGLStateBucketAdd( OpenGLStateBucket& bucket, const OpenGLRenderable& renderable, const Matrix4& modelview ) :
		m_bucket( bucket ), m_renderable( renderable ), m_modelview( modelview ){
	}
	void operator()( const RendererLight& light ){
		m_bucket.addRenderable( m_renderable, m_modelview, &light );
	}
};

class CountLights
{
	std::size_t m_count;
public:
	using func = void(RendererLight&);

	CountLights() : m_count( 0 ){
	}
	void operator()( const RendererLight& light ){
		++m_count;
	}
	std::size_t count() const {
		return m_count;
	}
};

class OpenGLShader final : public Shader
{
	typedef std::list<OpenGLStateBucket*> Passes;
	Passes m_passes;
	IShader* m_shader;
	std::size_t m_used;
	ModuleObservers m_observers;
public:
	OpenGLShader() : m_shader( 0 ), m_used( 0 ){
	}
	~OpenGLShader(){
	}
	void construct( const char* name );
	void destroy(){
		if ( m_shader ) {
			m_shader->DecRef();
		}
		m_shader = 0;

		for ( Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i )
		{
			delete *i;
		}
		m_passes.clear();
	}
	void addRenderable( const OpenGLRenderable& renderable, const Matrix4& modelview, const LightList* lights ){
		for ( Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i )
		{
#if LIGHT_SHADER_DEBUG
			if ( ( ( *i )->state().m_state & RENDER_BUMP ) != 0 ) {
				if ( lights != 0 ) {
					CountLights counter;
					lights->forEachLight( makeCallback( counter ) );
					globalOutputStream() << "count = " << counter.count() << '\n';
					for ( std::size_t i = 0; i < counter.count(); ++i )
					{
						g_lightDebugShaders[counter.count()]->addRenderable( renderable, modelview );
					}
				}
			}
			else
#else
			if ( ( ( *i )->state().m_state & RENDER_BUMP ) != 0 ) {
				if ( lights != 0 ) {
					OpenGLStateBucketAdd add( *( *i ), renderable, modelview );
					lights->forEachLight( makeCallback( add ) );
				}
			}
			else
#endif
			{
				( *i )->addRenderable( renderable, modelview );
			}
		}
	}
	void incrementUsed(){
		if ( ++m_used == 1 && m_shader != 0 ) {
			m_shader->SetInUse( true );
		}
	}
	void decrementUsed(){
		if ( --m_used == 0 && m_shader != 0 ) {
			m_shader->SetInUse( false );
		}
	}
	bool realised() const {
		return m_shader != 0;
	}
	void attach( ModuleObserver& observer ){
		if ( realised() ) {
			observer.realise();
		}
		m_observers.attach( observer );
	}
	void detach( ModuleObserver& observer ){
		if ( realised() ) {
			observer.unrealise();
		}
		m_observers.detach( observer );
	}
	void realise( const CopiedString& name ){
		construct( name.c_str() );

		if ( m_used != 0 && m_shader != 0 ) {
			m_shader->SetInUse( true );
		}

		for ( Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i )
		{
			g_state_sorted.insert( OpenGLStates::value_type( OpenGLStateReference( ( *i )->state() ), *i ) );
		}

		m_observers.realise();
	}
	void unrealise(){
		m_observers.unrealise();

		for ( Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i )
		{
			g_state_sorted.erase( OpenGLStateReference( ( *i )->state() ) );
		}

		destroy();
	}
	qtexture_t& getTexture() const {
		ASSERT_NOTNULL( m_shader );
		return *m_shader->getTexture();
	}
	unsigned int getFlags() const {
		ASSERT_NOTNULL( m_shader );
		return m_shader->getFlags();
	}
	IShader& getShader() const {
		ASSERT_NOTNULL( m_shader );
		return *m_shader;
	}
	OpenGLState& appendDefaultPass(){
		m_passes.push_back( new OpenGLStateBucket );
		OpenGLState& state = m_passes.back()->state();
		OpenGLState_constructDefault( state );
		return state;
	}
};


inline bool lightEnabled( const RendererLight& light, const LightCullable& cullable ){
	return cullable.testLight( light );
}

typedef std::set<RendererLight*> RendererLights;

#define DEBUG_LIGHT_SYNC 0

class LinearLightList : public LightList
{
	LightCullable& m_cullable;
	RendererLights& m_allLights;
	Callback<void()> m_evaluateChanged;

	typedef std::list<RendererLight*> Lights;
	mutable Lights m_lights;
	mutable bool m_lightsChanged;
public:
	LinearLightList( LightCullable& cullable, RendererLights& lights, const Callback<void()>& evaluateChanged ) :
		m_cullable( cullable ), m_allLights( lights ), m_evaluateChanged( evaluateChanged ){
		m_lightsChanged = true;
	}
	void evaluateLights() const {
		m_evaluateChanged();
		if ( m_lightsChanged ) {
			m_lightsChanged = false;

			m_lights.clear();
			m_cullable.clearLights();
			for ( RendererLights::const_iterator i = m_allLights.begin(); i != m_allLights.end(); ++i )
			{
				if ( lightEnabled( *( *i ), m_cullable ) ) {
					m_lights.push_back( *i );
					m_cullable.insertLight( *( *i ) );
				}
			}
		}
#if ( DEBUG_LIGHT_SYNC )
		else
		{
			Lights lights;
			for ( RendererLights::const_iterator i = m_allLights.begin(); i != m_allLights.end(); ++i )
			{
				if ( lightEnabled( *( *i ), m_cullable ) ) {
					lights.push_back( *i );
				}
			}
			ASSERT_MESSAGE(
			    !std::lexicographical_compare( lights.begin(), lights.end(), m_lights.begin(), m_lights.end() )
			    && !std::lexicographical_compare( m_lights.begin(), m_lights.end(), lights.begin(), lights.end() ),
			    "lights out of sync"
			);
		}
#endif
	}
	void forEachLight( const RendererLightCallback& callback ) const {
		evaluateLights();

		for ( Lights::const_iterator i = m_lights.begin(); i != m_lights.end(); ++i )
		{
			callback( *( *i ) );
		}
	}
	void lightsChanged() const {
		m_lightsChanged = true;
	}
};

inline void setFogState( const OpenGLFogState& state ){
	gl().glFogi( GL_FOG_MODE, state.mode );
	gl().glFogf( GL_FOG_DENSITY, state.density );
	gl().glFogf( GL_FOG_START, state.start );
	gl().glFogf( GL_FOG_END, state.end );
	gl().glFogi( GL_FOG_INDEX, state.index );
	gl().glFogfv( GL_FOG_COLOR, vector4_to_array( state.colour ) );
}

#define DEBUG_SHADERS 0
void OpenGLState_apply( const OpenGLState& self, OpenGLState& current, unsigned int globalstate );

class OpenGLShaderCache final : public ShaderCache, public TexturesCacheObserver, public ModuleObserver
{
	class CreateOpenGLShader
	{
		OpenGLShaderCache* m_cache;
	public:
		explicit CreateOpenGLShader( OpenGLShaderCache* cache = 0 )
			: m_cache( cache ){
		}
		OpenGLShader* construct( const CopiedString& name ){
			OpenGLShader* shader = new OpenGLShader;
			if ( m_cache->realised() ) {
				shader->realise( name );
			}
			return shader;
		}
		void destroy( OpenGLShader* shader ){
			if ( m_cache->realised() ) {
				shader->unrealise();
			}
			delete shader;
		}
	};

	typedef HashedCache<CopiedString, OpenGLShader, HashString, std::equal_to<CopiedString>, CreateOpenGLShader> Shaders;
	Shaders m_shaders;
	std::size_t m_unrealised;

	bool m_lightingEnabled;

public:
	OpenGLShaderCache() :
		m_shaders( CreateOpenGLShader( this ) ),
		m_unrealised( 3 ), // wait until shaders, gl-context and textures are realised before creating any render-states
		m_lightingEnabled( true ),
		m_lightsChanged( true ),
		m_traverseRenderablesMutex( false ){
	}
	~OpenGLShaderCache(){
		for ( Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i )
		{
			globalOutputStream() << "leaked shader: " << makeQuoted( ( *i ).key ) << '\n';
		}
	}
	Shader* capture( const char* name ){
		ASSERT_MESSAGE( name[0] == '$'
		                || *name == '['
		                || *name == '<'
		                || *name == '('
		                || *name == '{'
		                || strchr( name, '\\' ) == 0, "shader name contains invalid characters: " << makeQuoted( name ) );
#if DEBUG_SHADERS
		globalOutputStream() << "shaders capture: " << makeQuoted( name ) << '\n';
#endif
		return m_shaders.capture( name ).get();
	}
	void release( const char *name ){
#if DEBUG_SHADERS
		globalOutputStream() << "shaders release: " << makeQuoted( name ) << '\n';
#endif
		m_shaders.release( name );
	}
	void render( RenderStateFlags globalstate, const Matrix4& modelview, const Matrix4& projection, const Vector3& viewer ){
		gl().glMatrixMode( GL_PROJECTION );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &projection ) );
#if 0
		//qglGetFloatv( GL_PROJECTION_MATRIX, reinterpret_cast<float*>( &projection ) );
#endif

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &modelview ) );
#if 0
		//qglGetFloatv( GL_MODELVIEW_MATRIX, reinterpret_cast<float*>( &modelview ) );
#endif

		ASSERT_MESSAGE( realised(), "render states are not realised" );

		// global settings that are not set in renderstates
		gl().glFrontFace( GL_CW );
		gl().glCullFace( GL_BACK );
		gl().glPolygonOffset( -1, 1 );
		{
			const GLubyte pattern[132] = {
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
				0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55
			};
			gl().glPolygonStipple( pattern );
		}
		gl().glEnableClientState( GL_VERTEX_ARRAY );
		g_vertexArray_enabled = true;
		gl().glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );

		gl().glActiveTexture( GL_TEXTURE0 );
		gl().glClientActiveTexture( GL_TEXTURE0 );

		gl().glUseProgram( 0 );
		gl().glDisableVertexAttribArray( c_attr_TexCoord0 );
		gl().glDisableVertexAttribArray( c_attr_Tangent );
		gl().glDisableVertexAttribArray( c_attr_Binormal );

		if ( globalstate & RENDER_TEXTURE ) {
			gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
			gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		}

		OpenGLState current;
		OpenGLState_constructDefault( current );
		current.m_sort = OpenGLState::eSortFirst;

		// default renderstate settings
		gl().glLineStipple( current.m_linestipple_factor, current.m_linestipple_pattern );
		gl().glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		gl().glDisable( GL_LIGHTING );
		gl().glDisable( GL_TEXTURE_2D );
		gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		g_texcoordArray_enabled = false;
		gl().glDisableClientState( GL_COLOR_ARRAY );
		g_colorArray_enabled = false;
		gl().glDisableClientState( GL_NORMAL_ARRAY );
		g_normalArray_enabled = false;
		gl().glDisable( GL_BLEND );
		gl().glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		gl().glDisable( GL_CULL_FACE );
		gl().glShadeModel( GL_FLAT );
		gl().glDisable( GL_DEPTH_TEST );
		gl().glDepthMask( GL_FALSE );
		gl().glDisable( GL_ALPHA_TEST );
		gl().glDisable( GL_LINE_STIPPLE );
		gl().glDisable( GL_POLYGON_STIPPLE );
		gl().glDisable( GL_POLYGON_OFFSET_LINE );

		gl().glBindTexture( GL_TEXTURE_2D, 0 );
		gl().glColor4f( 1, 1, 1, 1 );
		gl().glDepthFunc( GL_LESS );
		gl().glAlphaFunc( GL_ALWAYS, 0 );
		gl().glLineWidth( 1 );
		gl().glPointSize( 1 );

		gl().glHint( GL_FOG_HINT, GL_NICEST );
		gl().glDisable( GL_FOG );
		setFogState( OpenGLFogState() );

		GlobalOpenGL_debugAssertNoErrors();

		debug_string( "begin rendering" );
		for ( OpenGLStates::iterator i = g_state_sorted.begin(); i != g_state_sorted.end(); ++i )
		{
			( *i ).second->render( current, globalstate, viewer );
		}
		debug_string( "end rendering" );

		OpenGLState reset = current; /* reset some states */
		reset.m_state = current.m_state & ~RENDER_TEXT; /* popmatrix after RENDER_TEXT */
		reset.m_program = nullptr; /* disable shader */
		OpenGLState_apply( reset, current, globalstate );
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			if ( lightingEnabled() ) {
				g_bumpGLSL.create();
				g_depthFillGLSL.create();
			}

			g_skyboxGLSL.create();

			for ( Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i )
			{
				if ( !( *i ).value.empty() ) {
					( *i ).value->realise( i->key );
				}
			}
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			for ( Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i )
			{
				if ( !( *i ).value.empty() ) {
					( *i ).value->unrealise();
				}
			}
			if ( GlobalOpenGL().contextValid && lightingEnabled() ) {
				g_bumpGLSL.destroy();
				g_depthFillGLSL.destroy();
			}
			if( GlobalOpenGL().contextValid )
				g_skyboxGLSL.destroy();
		}
	}
	bool realised(){
		return m_unrealised == 0;
	}


	bool lightingEnabled() const {
		return m_lightingEnabled;
	}
	void extensionsInitialised(){
		setLightingEnabled( m_lightingEnabled );
	}
	void setLightingEnabled( bool enabled ){
		const bool refresh = ( m_lightingEnabled != enabled );

		if ( refresh ) {
			unrealise();
			GlobalShaderSystem().setLightingEnabled( enabled );
		}

		m_lightingEnabled = enabled;

		if ( refresh ) {
			realise();
		}
	}

// light culling

	RendererLights m_lights;
	bool m_lightsChanged;
	typedef std::map<LightCullable*, LinearLightList> LightLists;
	LightLists m_lightLists;

	const LightList& attach( LightCullable& cullable ){
		return ( *m_lightLists.insert( LightLists::value_type( &cullable, LinearLightList( cullable, m_lights, EvaluateChangedCaller( *this ) ) ) ).first ).second;
	}
	void detach( LightCullable& cullable ){
		m_lightLists.erase( &cullable );
	}
	void changed( LightCullable& cullable ){
		LightLists::iterator i = m_lightLists.find( &cullable );
		ASSERT_MESSAGE( i != m_lightLists.end(), "cullable not attached" );
		( *i ).second.lightsChanged();
	}
	void attach( RendererLight& light ){
		const bool inserted = m_lights.insert( &light ).second;
		ASSERT_MESSAGE( inserted, "light could not be attached" );
		changed( light );
	}
	void detach( RendererLight& light ){
		const bool erased = m_lights.erase( &light );
		ASSERT_MESSAGE( erased, "light could not be detached" );
		changed( light );
	}
	void changed( RendererLight& light ){
		m_lightsChanged = true;
	}
	void evaluateChanged(){
		if ( m_lightsChanged ) {
			m_lightsChanged = false;
			for ( LightLists::iterator i = m_lightLists.begin(); i != m_lightLists.end(); ++i )
			{
				( *i ).second.lightsChanged();
			}
		}
	}
	typedef MemberCaller<OpenGLShaderCache, void(), &OpenGLShaderCache::evaluateChanged> EvaluateChangedCaller;

	typedef std::set<const Renderable*> Renderables;
	Renderables m_renderables;
	mutable bool m_traverseRenderablesMutex;

// renderables
	void attachRenderable( const Renderable& renderable ){
		ASSERT_MESSAGE( !m_traverseRenderablesMutex, "attaching renderable during traversal" );
		const bool inserted = m_renderables.insert( &renderable ).second;
		ASSERT_MESSAGE( inserted, "renderable could not be attached" );
	}
	void detachRenderable( const Renderable& renderable ){
		ASSERT_MESSAGE( !m_traverseRenderablesMutex, "detaching renderable during traversal" );
		const bool erased = m_renderables.erase( &renderable );
		ASSERT_MESSAGE( erased, "renderable could not be detached" );
	}
	void forEachRenderable( const RenderableCallback& callback ) const {
		ASSERT_MESSAGE( !m_traverseRenderablesMutex, "for-each during traversal" );
		m_traverseRenderablesMutex = true;
		for ( Renderables::const_iterator i = m_renderables.begin(); i != m_renderables.end(); ++i )
		{
			callback( *( *i ) );
		}
		m_traverseRenderablesMutex = false;
	}
};

static OpenGLShaderCache* g_ShaderCache;

void ShaderCache_extensionsInitialised(){
	g_ShaderCache->extensionsInitialised();
}

void ShaderCache_setBumpEnabled( bool enabled ){
	g_ShaderCache->setLightingEnabled( enabled );
}


Vector3 g_DebugShaderColours[256];
Shader* g_defaultPointLight = 0;

void ShaderCache_Construct(){
	g_ShaderCache = new OpenGLShaderCache;
	GlobalTexturesCache().attach( *g_ShaderCache );
	GlobalShaderSystem().attach( *g_ShaderCache );

	if ( g_pGameDescription->mGameType == "doom3" ) {
		g_defaultPointLight = g_ShaderCache->capture( "lights/defaultPointLight" );
		//Shader* overbright =
		g_ShaderCache->capture( "$OVERBRIGHT" );

#if LIGHT_SHADER_DEBUG
		for ( std::size_t i = 0; i < 256; ++i )
		{
			g_DebugShaderColours[i] = Vector3( i / 256.0, i / 256.0, i / 256.0 );
		}

		g_DebugShaderColours[0] = Vector3( 1, 0, 0 );
		g_DebugShaderColours[1] = Vector3( 1, 0.5, 0 );
		g_DebugShaderColours[2] = Vector3( 1, 1, 0 );
		g_DebugShaderColours[3] = Vector3( 0.5, 1, 0 );
		g_DebugShaderColours[4] = Vector3( 0, 1, 0 );
		g_DebugShaderColours[5] = Vector3( 0, 1, 0.5 );
		g_DebugShaderColours[6] = Vector3( 0, 1, 1 );
		g_DebugShaderColours[7] = Vector3( 0, 0.5, 1 );
		g_DebugShaderColours[8] = Vector3( 0, 0, 1 );
		g_DebugShaderColours[9] = Vector3( 0.5, 0, 1 );
		g_DebugShaderColours[10] = Vector3( 1, 0, 1 );
		g_DebugShaderColours[11] = Vector3( 1, 0, 0.5 );

		g_lightDebugShaders.reserve( 256 );
		StringOutputStream buffer( 256 );
		for ( std::size_t i = 0; i < 256; ++i )
		{
			g_lightDebugShaders.push_back( g_ShaderCache->capture( buffer( '(', g_DebugShaderColours[i].x(), ' ', g_DebugShaderColours[i].y(), ' ', g_DebugShaderColours[i].z(), ')' ) ) );
		}
#endif
	}
}

void ShaderCache_Destroy(){
	if ( g_pGameDescription->mGameType == "doom3" ) {
		g_ShaderCache->release( "lights/defaultPointLight" );
		g_ShaderCache->release( "$OVERBRIGHT" );
		g_defaultPointLight = 0;

#if LIGHT_SHADER_DEBUG
		g_lightDebugShaders.clear();
		StringOutputStream buffer( 256 );
		for ( std::size_t i = 0; i < 256; ++i )
		{
			g_ShaderCache->release( buffer( '(', g_DebugShaderColours[i].x(), ' ', g_DebugShaderColours[i].y(), ' ', g_DebugShaderColours[i].z(), ')' ) );
		}
#endif
	}

	GlobalShaderSystem().detach( *g_ShaderCache );
	GlobalTexturesCache().detach( *g_ShaderCache );
	delete g_ShaderCache;
}

ShaderCache* GetShaderCache(){
	return g_ShaderCache;
}

inline void setTextureState( GLint& current, const GLint& texture, GLenum textureUnit ){
	if ( texture != current ) {
		gl().glActiveTexture( textureUnit );
		gl().glClientActiveTexture( textureUnit );
		gl().glBindTexture( GL_TEXTURE_2D, texture );
		GlobalOpenGL_debugAssertNoErrors();
		current = texture;
	}
}

inline void setTextureState( GLint& current, const GLint& texture ){
	if ( texture != current ) {
		gl().glBindTexture( GL_TEXTURE_2D, texture );
		GlobalOpenGL_debugAssertNoErrors();
		current = texture;
	}
}

inline void setState( unsigned int state, unsigned int delta, unsigned int flag, GLenum glflag ){
	if ( delta & state & flag ) {
		gl().glEnable( glflag );
		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & flag ) {
		gl().glDisable( glflag );
		GlobalOpenGL_debugAssertNoErrors();
	}
}

void OpenGLState_apply( const OpenGLState& self, OpenGLState& current, unsigned int globalstate ){
	debug_int( "sort", int( self.m_sort ) );
	debug_int( "texture", self.m_texture );
	debug_int( "state", self.m_state );
	debug_int( "address", int( std::size_t( &self ) ) );

	count_state();

	if ( self.m_state & RENDER_OVERRIDE ) {
		globalstate |= RENDER_FILL;
	}
	if ( self.m_state & RENDER_TEXT ) {
		globalstate |= RENDER_TEXTURE | RENDER_BLEND | RENDER_FILL | RENDER_TEXT;
	}

	const unsigned int state = self.m_state & globalstate;
	const unsigned int delta = state ^ current.m_state;

	GlobalOpenGL_debugAssertNoErrors();

	if ( delta & state & RENDER_TEXT ) {
		gl().glMatrixMode( GL_PROJECTION );
		gl().glPushMatrix();
		gl().glLoadIdentity();
		GLint viewprt[4];
		gl().glGetIntegerv( GL_VIEWPORT, viewprt );
		//globalOutputStream() << viewprt[2] << ' ' << viewprt[3] << '\n';
		gl().glOrtho( 0, viewprt[2], 0, viewprt[3], -100, 100 );
		gl().glTranslated( double( viewprt[2] ) / 2.0, double( viewprt[3] ) / 2.0, 0 );
		gl().glMatrixMode( GL_MODELVIEW );
		gl().glPushMatrix();
		gl().glLoadIdentity();

		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_TEXT ) {
		gl().glMatrixMode( GL_PROJECTION );
		gl().glPopMatrix();
		gl().glMatrixMode( GL_MODELVIEW );
		gl().glPopMatrix();

		GlobalOpenGL_debugAssertNoErrors();
	}

	GLProgram* program = ( state & RENDER_PROGRAM ) != 0 ? self.m_program : 0;

	if ( program != current.m_program ) {
		if ( current.m_program != 0 ) {
			current.m_program->disable();
//why?			gl().glColor4fv( vector4_to_array( current.m_colour ) );
			debug_colour( "cleaning program" );
		}

		current.m_program = program;

		if ( current.m_program != 0 ) {
			current.m_program->enable();
		}
	}

	if ( delta & state & RENDER_FILL ) {
		//qglPolygonMode ( GL_BACK, GL_LINE );
		//qglPolygonMode ( GL_FRONT, GL_FILL );
		gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_FILL ) {
		gl().glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		GlobalOpenGL_debugAssertNoErrors();
	}

	setState( state, delta, RENDER_OFFSETLINE, GL_POLYGON_OFFSET_LINE );

	if ( delta & state & RENDER_LIGHTING ) {
		gl().glEnable( GL_LIGHTING );
		gl().glEnable( GL_COLOR_MATERIAL );
		gl().glEnable( GL_RESCALE_NORMAL );
		gl().glEnableClientState( GL_NORMAL_ARRAY );
		GlobalOpenGL_debugAssertNoErrors();
		g_normalArray_enabled = true;
	}
	else if ( delta & ~state & RENDER_LIGHTING ) {
		gl().glDisable( GL_LIGHTING );
		gl().glDisable( GL_COLOR_MATERIAL );
		gl().glDisable( GL_RESCALE_NORMAL );
		gl().glDisableClientState( GL_NORMAL_ARRAY );
		GlobalOpenGL_debugAssertNoErrors();
		g_normalArray_enabled = false;
	}

	if ( delta & state & RENDER_TEXTURE ) {
		GlobalOpenGL_debugAssertNoErrors();

		gl().glActiveTexture( GL_TEXTURE0 );
		gl().glClientActiveTexture( GL_TEXTURE0 );

		gl().glEnable( GL_TEXTURE_2D );

		gl().glColor4f( 1, 1, 1, self.m_colour[3] );
		debug_colour( "setting texture" );

		gl().glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		GlobalOpenGL_debugAssertNoErrors();
		g_texcoordArray_enabled = true;
	}
	else if ( delta & ~state & RENDER_TEXTURE ) {
		gl().glActiveTexture( GL_TEXTURE0 );
		gl().glClientActiveTexture( GL_TEXTURE0 );

		gl().glDisable( GL_TEXTURE_2D );
		gl().glBindTexture( GL_TEXTURE_2D, 0 );
		gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );

		GlobalOpenGL_debugAssertNoErrors();
		g_texcoordArray_enabled = false;
	}

	if ( delta & state & RENDER_BLEND ) {
// FIXME: some .TGA are buggy, have a completely empty alpha channel
// if such brushes are rendered in this loop they would be totally transparent with GL_MODULATE
// so I decided using GL_DECAL instead
// if an empty-alpha-channel or nearly-empty texture is used. It will be blank-transparent.
// this could get better if you can get glTexEnviv (GL_TEXTURE_ENV, to work .. patches are welcome

		gl().glEnable( GL_BLEND );
		gl().glActiveTexture( GL_TEXTURE0 );
//		gl().glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
//		gl().glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); //uses actual alpha channel, = invis, if qer_trans + empty alpha channel
		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_BLEND ) {
		gl().glDisable( GL_BLEND );
		gl().glActiveTexture( GL_TEXTURE0 );
//		gl().glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		GlobalOpenGL_debugAssertNoErrors();
	}

	setState( state, delta, RENDER_CULLFACE, GL_CULL_FACE );

	if ( delta & state & RENDER_SMOOTH ) {
		gl().glShadeModel( GL_SMOOTH );
		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_SMOOTH ) {
		gl().glShadeModel( GL_FLAT );
		GlobalOpenGL_debugAssertNoErrors();
	}

	setState( state, delta, RENDER_SCALED, GL_NORMALIZE ); // not GL_RESCALE_NORMAL

	setState( state, delta, RENDER_DEPTHTEST, GL_DEPTH_TEST );

	if ( delta & state & RENDER_DEPTHWRITE ) {
		gl().glDepthMask( GL_TRUE );

#if DEBUG_RENDER
		GLboolean depthEnabled;
		gl().glGetBooleanv( GL_DEPTH_WRITEMASK, &depthEnabled );
		ASSERT_MESSAGE( depthEnabled, "failed to set depth buffer mask bit" );
#endif
		debug_string( "enabled depth-buffer writing" );

		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_DEPTHWRITE ) {
		gl().glDepthMask( GL_FALSE );

#if DEBUG_RENDER
		GLboolean depthEnabled;
		gl().glGetBooleanv( GL_DEPTH_WRITEMASK, &depthEnabled );
		ASSERT_MESSAGE( !depthEnabled, "failed to set depth buffer mask bit" );
#endif
		debug_string( "disabled depth-buffer writing" );

		GlobalOpenGL_debugAssertNoErrors();
	}

	if ( delta & state & RENDER_COLOURWRITE ) {
		gl().glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		GlobalOpenGL_debugAssertNoErrors();
	}
	else if ( delta & ~state & RENDER_COLOURWRITE ) {
		gl().glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		GlobalOpenGL_debugAssertNoErrors();
	}

	setState( state, delta, RENDER_ALPHATEST, GL_ALPHA_TEST );

	if ( delta & state & RENDER_COLOURARRAY ) {
		gl().glEnableClientState( GL_COLOR_ARRAY );
		GlobalOpenGL_debugAssertNoErrors();
		debug_colour( "enabling color_array" );
		g_colorArray_enabled = true;
	}
	else if ( delta & ~state & RENDER_COLOURARRAY ) {
		gl().glDisableClientState( GL_COLOR_ARRAY );
		gl().glColor4fv( vector4_to_array( self.m_colour ) );
		debug_colour( "cleaning color_array" );
		GlobalOpenGL_debugAssertNoErrors();
		g_colorArray_enabled = false;
	}

	if ( delta & ~state & RENDER_COLOURCHANGE ) {
		gl().glColor4fv( vector4_to_array( self.m_colour ) );
		GlobalOpenGL_debugAssertNoErrors();
	}

	setState( state, delta, RENDER_LINESTIPPLE, GL_LINE_STIPPLE );

	setState( state, delta, RENDER_POLYGONSTIPPLE, GL_POLYGON_STIPPLE );

	setState( state, delta, RENDER_FOG, GL_FOG );

	if ( ( state & RENDER_FOG ) != 0 ) {
		setFogState( self.m_fog );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_fog = self.m_fog;
	}

	if ( state & RENDER_DEPTHTEST && self.m_depthfunc != current.m_depthfunc ) {
		gl().glDepthFunc( self.m_depthfunc );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_depthfunc = self.m_depthfunc;
	}

	if ( state & RENDER_LINESTIPPLE
	     && ( self.m_linestipple_factor != current.m_linestipple_factor
	          || self.m_linestipple_pattern != current.m_linestipple_pattern ) ) {
		gl().glLineStipple( self.m_linestipple_factor, self.m_linestipple_pattern );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_linestipple_factor = self.m_linestipple_factor;
		current.m_linestipple_pattern = self.m_linestipple_pattern;
	}


	if ( state & RENDER_ALPHATEST
	     && ( self.m_alphafunc != current.m_alphafunc
	          || self.m_alpharef != current.m_alpharef ) ) {
		gl().glAlphaFunc( self.m_alphafunc, self.m_alpharef );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_alphafunc = self.m_alphafunc;
		current.m_alpharef = self.m_alpharef;
	}

	{
		GLint texture0 = 0;
		GLint texture1 = 0;
		GLint texture2 = 0;
		GLint texture3 = 0;
		GLint texture4 = 0;
		GLint texture5 = 0;
		GLint texture6 = 0;
		GLint texture7 = 0;
		//if( state & RENDER_TEXTURE ) != 0)
		{
			texture0 = self.m_texture;
			texture1 = self.m_texture1;
			texture2 = self.m_texture2;
			texture3 = self.m_texture3;
			texture4 = self.m_texture4;
			texture5 = self.m_texture5;
			texture6 = self.m_texture6;
			texture7 = self.m_texture7;
		}

		{
			setTextureState( current.m_texture, texture0, GL_TEXTURE0 );
			setTextureState( current.m_texture1, texture1, GL_TEXTURE1 );
			setTextureState( current.m_texture2, texture2, GL_TEXTURE2 );
			setTextureState( current.m_texture3, texture3, GL_TEXTURE3 );
			setTextureState( current.m_texture4, texture4, GL_TEXTURE4 );
			setTextureState( current.m_texture5, texture5, GL_TEXTURE5 );
			setTextureState( current.m_texture6, texture6, GL_TEXTURE6 );
			setTextureState( current.m_texture7, texture7, GL_TEXTURE7 );
		}
	}


	if( current.m_textureSkyBox != self.m_textureSkyBox ){
		gl().glActiveTexture( GL_TEXTURE0 );
		gl().glClientActiveTexture( GL_TEXTURE0 );
		gl().glBindTexture( GL_TEXTURE_CUBE_MAP, self.m_textureSkyBox );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_textureSkyBox = self.m_textureSkyBox;
	}

	if ( state & RENDER_TEXTURE && self.m_colour[3] != current.m_colour[3] ) {
		debug_colour( "setting alpha" );
		gl().glColor4f( 1, 1, 1, self.m_colour[3] );
		GlobalOpenGL_debugAssertNoErrors();
	}

	if ( !( state & RENDER_TEXTURE )
	     && self.m_colour != current.m_colour ) {
		gl().glColor4fv( vector4_to_array( self.m_colour ) );
		debug_colour( "setting non-texture" );
		GlobalOpenGL_debugAssertNoErrors();
	}
	current.m_colour = self.m_colour;

	if ( state & RENDER_BLEND
	     && ( self.m_blend_src != current.m_blend_src || self.m_blend_dst != current.m_blend_dst ) ) {
		gl().glBlendFunc( self.m_blend_src, self.m_blend_dst );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_blend_src = self.m_blend_src;
		current.m_blend_dst = self.m_blend_dst;
	}

	if ( !( state & RENDER_FILL )
	     && self.m_linewidth != current.m_linewidth ) {
		gl().glLineWidth( self.m_linewidth );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_linewidth = self.m_linewidth;
	}

	if ( !( state & RENDER_FILL )
	     && self.m_pointsize != current.m_pointsize ) {
		gl().glPointSize( self.m_pointsize );
		GlobalOpenGL_debugAssertNoErrors();
		current.m_pointsize = self.m_pointsize;
	}

	current.m_state = state;

	GlobalOpenGL_debugAssertNoErrors();
}

void Renderables_flush( OpenGLStateBucket::Renderables& renderables, OpenGLState& current, unsigned int globalstate, const Vector3& viewer ){
	const Matrix4* transform = 0;
	gl().glPushMatrix();

	if ( current.m_program != 0 && current.m_textureSkyBox != 0 && globalstate & RENDER_PROGRAM ) {
		current.m_program->setParameters( viewer, g_matrix4_identity, g_vector3_identity, g_vector3_identity, g_matrix4_identity );
	}

	for ( OpenGLStateBucket::Renderables::const_iterator i = renderables.begin(); i != renderables.end(); ++i )
	{
		//qglLoadMatrixf( i->m_transform );
		if ( !transform || ( transform != ( *i ).m_transform && !matrix4_affine_equal( *transform, *( *i ).m_transform ) ) ) {
			count_transform();
			transform = ( *i ).m_transform;
			gl().glPopMatrix();
			gl().glPushMatrix();
			gl().glMultMatrixf( reinterpret_cast<const float*>( transform ) );
			gl().glFrontFace( ( ( current.m_state & RENDER_CULLFACE ) != 0 && matrix4_handedness( *transform ) == MATRIX4_RIGHTHANDED ) ? GL_CW : GL_CCW );
		}

		count_prim();

		if ( current.m_program != 0 && ( *i ).m_light != 0 ) {
			const IShader& lightShader = static_cast<OpenGLShader*>( ( *i ).m_light->getShader() )->getShader();
			if ( lightShader.firstLayer() != 0 ) {
				GLuint attenuation_xy = lightShader.firstLayer()->texture()->texture_number;
				GLuint attenuation_z = lightShader.lightFalloffImage() != 0
				                       ? lightShader.lightFalloffImage()->texture_number
				                       : static_cast<OpenGLShader*>( g_defaultPointLight )->getShader().lightFalloffImage()->texture_number;

				setTextureState( current.m_texture3, attenuation_xy, GL_TEXTURE3 );
				gl().glActiveTexture( GL_TEXTURE3 );
				gl().glBindTexture( GL_TEXTURE_2D, attenuation_xy );
				gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
				gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );

				setTextureState( current.m_texture4, attenuation_z, GL_TEXTURE4 );
				gl().glActiveTexture( GL_TEXTURE4 );
				gl().glBindTexture( GL_TEXTURE_2D, attenuation_z );
				gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
				gl().glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );


				AABB lightBounds( ( *i ).m_light->aabb() );

				Matrix4 world2light( g_matrix4_identity );

				if ( ( *i ).m_light->isProjected() ) {
					world2light = ( *i ).m_light->projection();
					matrix4_multiply_by_matrix4( world2light, matrix4_transposed( ( *i ).m_light->rotation() ) );
					matrix4_translate_by_vec3( world2light, vector3_negated( lightBounds.origin ) ); // world->lightBounds
				}
				if ( !( *i ).m_light->isProjected() ) {
					matrix4_translate_by_vec3( world2light, Vector3( 0.5f, 0.5f, 0.5f ) );
					matrix4_scale_by_vec3( world2light, Vector3( 0.5f, 0.5f, 0.5f ) );
					matrix4_scale_by_vec3( world2light, Vector3( 1.0f / lightBounds.extents.x(), 1.0f / lightBounds.extents.y(), 1.0f / lightBounds.extents.z() ) );
					matrix4_multiply_by_matrix4( world2light, matrix4_transposed( ( *i ).m_light->rotation() ) );
					matrix4_translate_by_vec3( world2light, vector3_negated( lightBounds.origin ) ); // world->lightBounds
				}

				current.m_program->setParameters( viewer, *( *i ).m_transform, lightBounds.origin + ( *i ).m_light->offset(), ( *i ).m_light->colour(), world2light );
				debug_string( "set lightBounds parameters" );
			}
		}

		( *i ).m_renderable->render( current.m_state );
	}
	gl().glPopMatrix();
	renderables.clear();
}

void OpenGLStateBucket::render( OpenGLState& current, unsigned int globalstate, const Vector3& viewer ){
	if ( ( globalstate & m_state.m_state & RENDER_SCREEN ) != 0 ) {
		OpenGLState_apply( m_state, current, globalstate );
		debug_colour( "screen fill" );

		gl().glMatrixMode( GL_PROJECTION );
		gl().glPushMatrix();
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &g_matrix4_identity ) );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glPushMatrix();
		gl().glLoadMatrixf( reinterpret_cast<const float*>( &g_matrix4_identity ) );

		gl().glBegin( GL_QUADS );
		gl().glVertex3f( -1, -1, 0 );
		gl().glVertex3f( 1, -1, 0 );
		gl().glVertex3f( 1, 1, 0 );
		gl().glVertex3f( -1, 1, 0 );
		gl().glEnd();

		gl().glMatrixMode( GL_PROJECTION );
		gl().glPopMatrix();

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glPopMatrix();
	}
	else if ( !m_renderables.empty() ) {
		OpenGLState_apply( m_state, current, globalstate );
		Renderables_flush( m_renderables, current, globalstate, viewer );
	}
}


class OpenGLStateMap : public OpenGLStateLibrary
{
	typedef std::map<CopiedString, OpenGLState> States;
	States m_states;
public:
	~OpenGLStateMap(){
		ASSERT_MESSAGE( m_states.empty(), "OpenGLStateMap::~OpenGLStateMap: not empty" );
	}

	typedef States::iterator iterator;
	iterator begin(){
		return m_states.begin();
	}
	iterator end(){
		return m_states.end();
	}

	void getDefaultState( OpenGLState& state ) const {
		OpenGLState_constructDefault( state );
	}

	void insert( const char* name, const OpenGLState& state ){
		bool inserted = m_states.insert( States::value_type( name, state ) ).second;
		ASSERT_MESSAGE( inserted, "OpenGLStateMap::insert: " << name << " already exists" );
	}
	void erase( const char* name ){
		std::size_t count = m_states.erase( name );
		ASSERT_MESSAGE( count == 1, "OpenGLStateMap::erase: " << name << " does not exist" );
	}

	iterator find( const char* name ){
		return m_states.find( name );
	}
};

OpenGLStateMap* g_openglStates = 0;

inline GLenum convertBlendFactor( BlendFactor factor ){
	switch ( factor )
	{
	case BLEND_ZERO:
		return GL_ZERO;
	case BLEND_ONE:
		return GL_ONE;
	case BLEND_SRC_COLOUR:
		return GL_SRC_COLOR;
	case BLEND_ONE_MINUS_SRC_COLOUR:
		return GL_ONE_MINUS_SRC_COLOR;
	case BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case BLEND_ONE_MINUS_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case BLEND_DST_COLOUR:
		return GL_DST_COLOR;
	case BLEND_ONE_MINUS_DST_COLOUR:
		return GL_ONE_MINUS_DST_COLOR;
	case BLEND_DST_ALPHA:
		return GL_DST_ALPHA;
	case BLEND_ONE_MINUS_DST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case BLEND_SRC_ALPHA_SATURATE:
		return GL_SRC_ALPHA_SATURATE;
	}
	return GL_ZERO;
}

/// \todo Define special-case shaders in a data file.
void OpenGLShader::construct( const char* name ){
	OpenGLState& state = appendDefaultPass();
	switch ( name[0] )
	{
	case '{':	//add
		sscanf( name, "{%g %g %g}", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2] );
		state.m_colour[3] = 1.0f;
		state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_BLEND | RENDER_FILL | RENDER_COLOURWRITE /*| RENDER_DEPTHWRITE */| RENDER_LIGHTING;
		state.m_blend_src = GL_ONE;
		state.m_blend_dst = GL_ONE;
//		state.m_blend_src = GL_DST_COLOR;
//		state.m_blend_dst = GL_SRC_COLOR;
//		state.m_blend_src = GL_DST_COLOR;
//		state.m_blend_dst = GL_ONE;
		state.m_sort = OpenGLState::eSortTranslucent;
		break;

	case '(':	//fill
		sscanf( name, "(%g %g %g)", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2] );
		state.m_colour[3] = 1.0f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		break;

	case '[':	//blend
		sscanf( name, "[%g %g %g]", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2] );
		state.m_colour[3] = 0.5f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_BLEND;
		state.m_sort = OpenGLState::eSortTranslucent;
		break;

	case '<':	//wire
		sscanf( name, "<%g %g %g>", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2] );
		state.m_colour[3] = 1;
		state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		state.m_depthfunc = GL_LESS;
		state.m_linewidth = 1;
		state.m_pointsize = 1;
		break;

	case '$':
		{
			OpenGLStateMap::iterator i = g_openglStates->find( name );
			if ( i != g_openglStates->end() ) {
				state = ( *i ).second;
				break;
			}
		}
		if ( string_equal( name + 1, "TEXT" ) ) {
			state.m_state = RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_FILL | RENDER_TEXTURE | RENDER_BLEND | RENDER_TEXT;
			state.m_sort = OpenGLState::eSortText;
		}
		else if ( string_equal( name + 1, "POINT" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst;
			state.m_pointsize = 6;
		}
		else if ( string_equal( name + 1, "DEEPPOINT" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst;
			state.m_pointsize = 6;

			OpenGLState& hiddenLine = appendDefaultPass(); // glBeginQuery glEndQuery
			hiddenLine.m_state = RENDER_DEPTHTEST;
			hiddenLine.m_sort = OpenGLState::eSortControlFirst - 1;
			hiddenLine.m_pointsize = 6;
			hiddenLine.m_depthfunc = GL_LEQUAL;
		}
		else if ( string_equal( name + 1, "SELPOINT" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst + 1;
			state.m_pointsize = 4;
		}
		else if ( string_equal( name + 1, "BIGPOINT" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortGUI1 + 1;
			state.m_pointsize = 6;
		}
		else if ( string_equal( name + 1, "PIVOT" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_linewidth = 2;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_linewidth = 2;
			hiddenLine.m_depthfunc = GL_GREATER;
		}
		else if ( string_equal( name + 1, "BLENDLINE" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_BLEND;
			state.m_sort = OpenGLState::eSortGUI0 - 1;
			state.m_linewidth = 1;
		}
		else if ( string_equal( name + 1, "LATTICE" ) ) {
			state.m_colour = Vector4( 1, 0.5, 0, 1 );
			state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst;
		}
		else if ( string_equal( name + 1, "WIREFRAME" ) ) {
			state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFullbright;
		}
		else if ( string_equal( name + 1, "CAM_HIGHLIGHT" ) ) {
			state.m_colour = Vector4( g_camwindow_globals.color_selbrushes3d, 0.3f );
			state.m_state = RENDER_FILL | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_BLEND | RENDER_COLOURWRITE/* | RENDER_DEPTHWRITE*/;
			state.m_sort = OpenGLState::eSortHighlight;
			state.m_depthfunc = GL_LEQUAL;
		}
		else if ( string_equal( name + 1, "CAM_OVERLAY" ) ) {
#if 0
			state.m_state = RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
#else
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_OFFSETLINE;
			state.m_sort = OpenGLState::eSortOverlayFirst + 1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_colour = Vector4( 0.75, 0.75, 0.75, 1 );
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_OFFSETLINE | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortOverlayFirst;
			hiddenLine.m_depthfunc = GL_GREATER;
			hiddenLine.m_linestipple_factor = 2;
#endif
		}
		else if ( string_equal( name + 1, "CAM_WIRE" ) ) {
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE;// | RENDER_OFFSETLINE;
			state.m_colour = Vector4( 0.75, 0.75, 0.75, 1 );
			state.m_linewidth = 0.5;
			state.m_sort = OpenGLState::eSortOverlayFirst + 1;
			state.m_depthfunc = GL_LEQUAL;
		}
		else if ( string_equal( name + 1, "CAM_FACEWIRE" ) ) {
			state.m_colour = Vector4( g_camwindow_globals.color_selbrushes3d, 1 );
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_OFFSETLINE;
			state.m_sort = OpenGLState::eSortOverlayFirst + 2;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_colour = Vector4( g_camwindow_globals.color_selbrushes3d, 1 );
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_OFFSETLINE | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortOverlayFirst + 1;
			hiddenLine.m_depthfunc = GL_GREATER;
			hiddenLine.m_linestipple_factor = 2;
		}
		else if ( string_equal( name + 1, "CAM_WORKZONE" ) ) {
			state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_BLEND | RENDER_COLOURARRAY | RENDER_OFFSETLINE | RENDER_SMOOTH;
			state.m_sort = OpenGLState::eSortOverlayFirst + 3;
			state.m_depthfunc = GL_LEQUAL;
		}
		else if ( string_equal( name + 1, "XY_OVERLAY" ) ) {
			state.m_colour = Vector4( g_xywindow_globals.color_selbrushes, 1 );
			state.m_state = RENDER_COLOURWRITE | RENDER_LINESTIPPLE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
			state.m_linewidth = 2;
			state.m_linestipple_factor = 3;
		}
		else if ( string_equal( name + 1, "DEBUG_CLIPPED" ) ) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortLast;
		}
		else if ( string_equal( name + 1, "POINTFILE" ) ) {
			state.m_colour = Vector4( 1, 0, 0, 1 );
			state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFullbright;
			state.m_linewidth = 4;
		}
#if 0
		else if ( string_equal( name + 1, "LIGHT_SPHERE" ) ) {
			state.m_colour = Vector4( .15f * .95f, .15f * .95f, .15f * .95f, 1 );
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_BLEND | RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_blend_src = GL_ONE;
			state.m_blend_dst = GL_ONE;
			state.m_sort = OpenGLState::eSortTranslucent;
		}
		else if ( string_equal( name + 1, "Q3MAP2_LIGHT_SPHERE" ) ) {
			state.m_colour = Vector4( .05f, .05f, .05f, 1 );
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_BLEND | RENDER_FILL;
			state.m_blend_src = GL_ONE;
			state.m_blend_dst = GL_ONE;
			state.m_sort = OpenGLState::eSortTranslucent;
		}
#endif // 0
		else if ( string_equal( name + 1, "PLANE_WIRE_OVERLAY" ) ) {
			state.m_colour = Vector4( 1, 1, 0, 1 );
			state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OFFSETLINE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;
			state.m_linewidth = 2;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_colour = state.m_colour;
			hiddenLine.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
			hiddenLine.m_linestipple_factor = 2;
		}
		else if ( string_equal( name + 1, "WIRE_OVERLAY" ) ) {
#if 0
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST;
			state.m_sort = OpenGLState::eSortOverlayFirst;
#else
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
#endif
		}
		else if ( string_equal( name + 1, "FLATSHADE_OVERLAY" ) ) {
			state.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY | RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY | RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE | RENDER_POLYGONSTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
		}
		else if ( string_equal( name + 1, "CLIPPER_OVERLAY" ) ) {
			state.m_colour = Vector4( g_xywindow_globals.color_clipper, 1 );
			state.m_state = RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_FILL | RENDER_POLYGONSTIPPLE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
		}
		else if ( string_equal( name + 1, "OVERBRIGHT" ) ) {
			const float lightScale = 2;
			state.m_colour = Vector4( Vector3( lightScale * 0.5f ), 0.5 );
			state.m_state = RENDER_FILL | RENDER_BLEND | RENDER_COLOURWRITE | RENDER_SCREEN;
			state.m_sort = OpenGLState::eSortOverbrighten;
			state.m_blend_src = GL_DST_COLOR;
			state.m_blend_dst = GL_SRC_COLOR;
		}
		else
		{
			// default to something recognisable.. =)
			ERROR_MESSAGE( "hardcoded renderstate not found" );
			state.m_colour = Vector4( 1, 0, 1, 1 );
			state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFirst;
		}
		break;
	default:
		// construction from IShader
		m_shader = QERApp_Shader_ForName( name );

		if ( g_ShaderCache->lightingEnabled() && m_shader->getBump() != 0 && m_shader->getBump()->texture_number != 0 ) { // is a bump shader
			state.m_state = RENDER_FILL | RENDER_CULLFACE | RENDER_TEXTURE | RENDER_DEPTHTEST | RENDER_DEPTHWRITE | RENDER_COLOURWRITE | RENDER_PROGRAM;
			state.m_colour = Vector4( 0, 0, 0, 1 );
			state.m_sort = OpenGLState::eSortOpaque;

			state.m_program = &g_depthFillGLSL;

			OpenGLState& bumpPass = appendDefaultPass();
			bumpPass.m_texture = m_shader->getDiffuse()->texture_number;
			bumpPass.m_texture1 = m_shader->getBump()->texture_number;
			bumpPass.m_texture2 = m_shader->getSpecular()->texture_number;

			bumpPass.m_state = RENDER_BLEND | RENDER_FILL | RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_SMOOTH | RENDER_BUMP | RENDER_PROGRAM | RENDER_LIGHTING;

			bumpPass.m_program = &g_bumpGLSL;

			bumpPass.m_depthfunc = GL_LEQUAL;
			bumpPass.m_sort = OpenGLState::eSortMultiFirst;
			bumpPass.m_blend_src = GL_ONE;
			bumpPass.m_blend_dst = GL_ONE;
		}
		else if( m_shader->getSkyBox() != nullptr && m_shader->getSkyBox()->texture_number != 0 )
		{
			state.m_texture = m_shader->getTexture()->texture_number;
			state.m_textureSkyBox = m_shader->getSkyBox()->texture_number;

			state.m_state = RENDER_FILL | RENDER_CULLFACE | RENDER_TEXTURE | RENDER_DEPTHTEST | RENDER_DEPTHWRITE | RENDER_COLOURWRITE | RENDER_PROGRAM;
			state.m_colour = Vector4( m_shader->getTexture()->color, 1 );
			state.m_sort = OpenGLState::eSortFullbright;

			state.m_program = &g_skyboxGLSL;
		}
		else
		{
			state.m_texture = m_shader->getTexture()->texture_number;

			state.m_state = RENDER_FILL | RENDER_TEXTURE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_LIGHTING | RENDER_SMOOTH;
			if ( ( m_shader->getFlags() & QER_CULL ) != 0 ) {
				if ( m_shader->getCull() == IShader::eCullBack ) {
					state.m_state |= RENDER_CULLFACE;
				}
			}
			else
			{
				state.m_state |= RENDER_CULLFACE;
			}
			if ( ( m_shader->getFlags() & QER_ALPHATEST ) != 0 ) {
				state.m_state |= RENDER_ALPHATEST;
				IShader::EAlphaFunc alphafunc;
				m_shader->getAlphaFunc( &alphafunc, &state.m_alpharef );
				switch ( alphafunc )
				{
				case IShader::eAlways:
					state.m_alphafunc = GL_ALWAYS;
					break;
				case IShader::eEqual:
					state.m_alphafunc = GL_EQUAL;
					break;
				case IShader::eLess:
					state.m_alphafunc = GL_LESS;
					break;
				case IShader::eGreater:
					state.m_alphafunc = GL_GREATER;
					break;
				case IShader::eLEqual:
					state.m_alphafunc = GL_LEQUAL;
					break;
				case IShader::eGEqual:
					state.m_alphafunc = GL_GEQUAL;
					break;
				}
			}
			state.m_colour = Vector4( m_shader->getTexture()->color, 1 );

			if ( ( m_shader->getFlags() & QER_TRANS ) != 0 ) {
				state.m_state |= RENDER_BLEND;
				state.m_colour[3] = m_shader->getTrans();
				state.m_sort = OpenGLState::eSortTranslucent;
				BlendFunc blendFunc = m_shader->getBlendFunc();
				state.m_blend_src = convertBlendFactor( blendFunc.m_src );
				state.m_blend_dst = convertBlendFactor( blendFunc.m_dst );
				state.m_depthfunc = GL_LEQUAL;
//				if ( state.m_blend_src == GL_SRC_ALPHA || state.m_blend_dst == GL_SRC_ALPHA ) {
//					state.m_state |= RENDER_DEPTHWRITE;
//				}
			}
			else
			{
				state.m_state |= RENDER_DEPTHWRITE;
				state.m_sort = OpenGLState::eSortFullbright;
			}
		}
	}
}


#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class OpenGLStateLibraryAPI
{
	OpenGLStateMap m_stateMap;
public:
	typedef OpenGLStateLibrary Type;
	STRING_CONSTANT( Name, "*" );

	OpenGLStateLibraryAPI(){
		g_openglStates = &m_stateMap;
	}
	~OpenGLStateLibraryAPI(){
		g_openglStates = 0;
	}
	OpenGLStateLibrary* getTable(){
		return &m_stateMap;
	}
};

typedef SingletonModule<OpenGLStateLibraryAPI> OpenGLStateLibraryModule;
typedef Static<OpenGLStateLibraryModule> StaticOpenGLStateLibraryModule;
StaticRegisterModule staticRegisterOpenGLStateLibrary( StaticOpenGLStateLibraryModule::instance() );

class ShaderCacheDependencies : public GlobalShadersModuleRef, public GlobalTexturesModuleRef, public GlobalOpenGLStateLibraryModuleRef
{
public:
	ShaderCacheDependencies() :
		GlobalShadersModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "shaders" ) ){
	}
};

class ShaderCacheAPI
{
	ShaderCache* m_shaderCache;
public:
	typedef ShaderCache Type;
	STRING_CONSTANT( Name, "*" );

	ShaderCacheAPI(){
		ShaderCache_Construct();

		m_shaderCache = GetShaderCache();
	}
	~ShaderCacheAPI(){
		ShaderCache_Destroy();
	}
	ShaderCache* getTable(){
		return m_shaderCache;
	}
};

typedef SingletonModule<ShaderCacheAPI, ShaderCacheDependencies> ShaderCacheModule;
typedef Static<ShaderCacheModule> StaticShaderCacheModule;
StaticRegisterModule staticRegisterShaderCache( StaticShaderCacheModule::instance() );
