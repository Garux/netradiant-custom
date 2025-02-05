/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// monitoring window for running BSP processes (and possibly various other stuff)

#include "watchbsp.h"

#include <algorithm>
#include <QTimer>

#include "commandlib.h"
#include "string/string.h"
#include "stream/stringstream.h"

#include "gtkutil/messagebox.h"
#include "xmlstuff.h"
#include "console.h"
#include "preferences.h"
#include "points.h"
#include "feedback.h"
#include "mainframe.h"
#include "sockets.h"
#include "timer.h"
#include "xmlstuff.h"

class CWatchBSP
{
private:
// a flag we have set to true when using an external BSP plugin
// the resulting code with that is a bit dirty, cleaner solution would be to separate the succession of commands from the listening loop
// (in two separate classes probably)
	bool m_bBSPPlugin;

// EIdle: we are not listening
//   DoMonitoringLoop will change state to EBeginStep
// EBeginStep: the socket is up for listening, we are expecting incoming connection
//   incoming connection will change state to EWatching
// EWatching: we have a connection, monitor it
//   connection closed will see if we start a new step (EBeginStep) or launch Quake3 and end (EIdle)
	enum EWatchBSPState { EIdle, EBeginStep, EWatching } m_eState;
	socket_t *m_pListenSocket;
	socket_t *m_pInSocket;
	netmessage_t msg;
	std::vector<CopiedString> m_commands;
// used to timeout EBeginStep
	Timer m_timeout_timer;
	std::size_t m_iCurrentStep;
	QTimer m_monitoring_timer;
// name of the map so we can run the engine
	CopiedString m_sBSPName;
// buffer we use in push mode to receive data directly from the network
	xmlParserInputBufferPtr m_xmlInputBuffer;
	xmlParserCtxtPtr m_xmlParserCtxt;
// call this to switch the set listening mode
	bool SetupListening();
// start a new EBeginStep
	void DoEBeginStep();
// the xml and sax parser state
	char m_xmlBuf[MAX_NETMESSAGE];
	bool m_bNeedCtxtInit;
	message_info_t m_message_info;

public:
	CWatchBSP(){
		m_bBSPPlugin = false;
		m_pListenSocket = NULL;
		m_pInSocket = NULL;
		m_eState = EIdle;
		m_xmlInputBuffer = NULL;
		m_bNeedCtxtInit = true;
		m_monitoring_timer.callOnTimeout( [this](){ RoutineProcessing(); } );
		m_monitoring_timer.setInterval( 25 );
	}
	virtual ~CWatchBSP(){
		EndMonitoringLoop();
		Net_Shutdown();
	}

	bool HasBSPPlugin() const
	{
		return m_bBSPPlugin;
	}

// called regularly to keep listening
	void RoutineProcessing();
// start a monitoring loop with the following steps
	void DoMonitoringLoop( const std::vector<CopiedString>& commands, const char *sBSPName );
	void EndMonitoringLoop(){
		Reset();
	}
// close everything - may be called from the outside to abort the process
	void Reset();
// start a listening loop for an external process, possibly a BSP plugin
	void ExternalListen();
};

CWatchBSP* g_pWatchBSP;

// watch the BSP process through network connections
// true: trigger the BSP steps one by one and monitor them through the network
// false: create a BAT / .sh file and execute it. don't bother monitoring it.
bool g_WatchBSP_Enabled = true;
// do we stop the compilation process if we come across a leak?
bool g_WatchBSP_LeakStop = true;
bool g_WatchBSP_RunQuake = false;
bool g_WatchBSP0_DumpLog = false;
// timeout when beginning a step (in seconds)
// if we don't get a connection quick enough we assume something failed and go back to idling
const int g_WatchBSP_Timeout = 5;

// manages customizable string, having variable internal default
// keeps empty, if marked with "default:" prefix, to allow altered default
// has CB to return custom or default to prefs dialog, other CB to return customized or empty to prefs saver
class DefaultableString
{
	CopiedString m_string;
	CopiedString ( * const m_getDefault )();
	static constexpr char m_defaultPrefix[] = "default:";
public:
	DefaultableString( CopiedString ( * const getDefault )() ) : m_getDefault( getDefault ){}
	void Import( const char *string ){
		if( string_equal_prefix( string, m_defaultPrefix ) )
			m_string = "";
		else
			m_string = string;
	}
	void ExportWithDefault( const StringImportCallback& importer ) const {
		importer( m_string.empty()? StringStream( m_defaultPrefix, m_getDefault() ) : m_string.c_str() );
	}
	void Export( const StringImportCallback& importer ) const {
		importer( m_string.c_str() );
	}
	auto getImportCaller(){
		return MemberCaller<DefaultableString, void(const char*), &DefaultableString::Import>( *this );
	}
	auto getExportWithDefaultCaller(){
		return ConstMemberCaller<DefaultableString, void(const StringImportCallback&), &DefaultableString::ExportWithDefault>( *this );
	}
	auto getExportCaller(){
		return ConstMemberCaller<DefaultableString, void(const StringImportCallback&), &DefaultableString::Export>( *this );
	}
	CopiedString string() const {
		return m_string.empty()? m_getDefault() : m_string;
	}
};

template<bool isMP>
CopiedString constructEngineArgs(){
	StringOutputStream string( 256 );
	if ( g_pGameDescription->mGameType == "q2"
	  || g_pGameDescription->mGameType == "heretic2" ) {
		string << ". +exec radiant.cfg +map %mapname%";
	}
	else{
		string << "+set sv_pure 0";
		// TTimo: a check for vm_* but that's all fine
		//cmdline = "+set sv_pure 0 +set vm_ui 0 +set vm_cgame 0 +set vm_game 0 ";
		const char* fs_game = gamename_get();
		if ( !string_equal( fs_game, basegame_get() ) ) {
			string << " +set fs_game " << fs_game;
		}
		if ( g_pGameDescription->mGameType == "wolf" ) {
		//|| g_pGameDescription->mGameType == "et" )
			if constexpr ( isMP ) // MP
				string << " +devmap %mapname%";
			else // SP
				string << " +set nextmap \"spdevmap %mapname%\"";
		}
		else{
			string << " +devmap %mapname%";
		}
	}
	return string.c_str();
}

#if defined( WIN32 )
#define ENGINE_ATTRIBUTE "engine_win32"
#define MP_ENGINE_ATTRIBUTE "mp_engine_win32"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
#define ENGINE_ATTRIBUTE "engine_linux"
#define MP_ENGINE_ATTRIBUTE "mp_engine_linux"
#elif defined( __APPLE__ )
#define ENGINE_ATTRIBUTE "engine_macos"
#define MP_ENGINE_ATTRIBUTE "mp_engine_macos"
#else
#error "unsupported platform"
#endif

static DefaultableString g_engineExecutable( []()->CopiedString{ return g_pGameDescription->getRequiredKeyValue( ENGINE_ATTRIBUTE ); } );
static DefaultableString g_engineExecutableMP( []()->CopiedString{ return g_pGameDescription->getKeyValue( MP_ENGINE_ATTRIBUTE ); } );

static DefaultableString g_engineArgs( constructEngineArgs<false> );
static DefaultableString g_engineArgsMP( constructEngineArgs<true> );

extern CopiedString g_regionBoxShader;


void Build_constructPreferences( PreferencesPage& page ){
	QCheckBox* monitorbsp = page.appendCheckBox( "", "Enable Build Process Monitoring", g_WatchBSP_Enabled );
	QCheckBox* leakstop = page.appendCheckBox( "", "Stop Compilation on Leak", g_WatchBSP_LeakStop );
	QCheckBox* runengine = page.appendCheckBox( "", "Run Engine After Compile", g_WatchBSP_RunQuake );
	Widget_connectToggleDependency( leakstop, monitorbsp );
	Widget_connectToggleDependency( runengine, monitorbsp );

	QWidget* engine = page.appendEntry( "Engine to Run", g_engineExecutable.getImportCaller(), g_engineExecutable.getExportWithDefaultCaller() );
	Widget_connectToggleDependency( engine, runengine );
	QWidget* engineargs = page.appendEntry( "Engine Arguments", g_engineArgs.getImportCaller(), g_engineArgs.getExportWithDefaultCaller() );
	Widget_connectToggleDependency( engineargs, runengine );
	if( !string_empty( g_pGameDescription->getKeyValue( "show_gamemode" ) ) ){
		QWidget* mpengine = page.appendEntry( "MP Engine to Run", g_engineExecutableMP.getImportCaller(), g_engineExecutableMP.getExportWithDefaultCaller() );
		Widget_connectToggleDependency( mpengine, runengine );
		QWidget* mpengineargs = page.appendEntry( "MP Engine Arguments", g_engineArgsMP.getImportCaller(), g_engineArgsMP.getExportWithDefaultCaller() );
		Widget_connectToggleDependency( mpengineargs, runengine );
	}

	page.appendCheckBox( "", "Dump non Monitored Builds Log", g_WatchBSP0_DumpLog );

	page.appendEntry( "Region Box Shader", g_regionBoxShader );
}
void Build_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Build", "Build Preferences" ) );
	Build_constructPreferences( page );
}
void Build_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( Build_constructPage ) );
}

#include "preferencesystem.h"
#include "stringio.h"

void BuildMonitor_Construct(){
	g_pWatchBSP = new CWatchBSP();

	g_WatchBSP_Enabled = !string_equal( g_pGameDescription->getKeyValue( "no_bsp_monitor" ), "1" );

	GlobalPreferenceSystem().registerPreference( "BuildMonitor", BoolImportStringCaller( g_WatchBSP_Enabled ), BoolExportStringCaller( g_WatchBSP_Enabled ) );
	GlobalPreferenceSystem().registerPreference( "BuildRunGame", BoolImportStringCaller( g_WatchBSP_RunQuake ), BoolExportStringCaller( g_WatchBSP_RunQuake ) );
	GlobalPreferenceSystem().registerPreference( "BuildLeakStop", BoolImportStringCaller( g_WatchBSP_LeakStop ), BoolExportStringCaller( g_WatchBSP_LeakStop ) );
	GlobalPreferenceSystem().registerPreference( "BuildEngineExecutable", g_engineExecutable.getImportCaller(), g_engineExecutable.getExportCaller() );
	GlobalPreferenceSystem().registerPreference( "BuildEngineExecutableMP", g_engineExecutableMP.getImportCaller(), g_engineExecutableMP.getExportCaller() );
	GlobalPreferenceSystem().registerPreference( "BuildEngineArgs", g_engineArgs.getImportCaller(), g_engineArgs.getExportCaller() );
	GlobalPreferenceSystem().registerPreference( "BuildEngineArgsMP", g_engineArgsMP.getImportCaller(), g_engineArgsMP.getExportCaller() );
	GlobalPreferenceSystem().registerPreference( "BuildDumpLog", BoolImportStringCaller( g_WatchBSP0_DumpLog ), BoolExportStringCaller( g_WatchBSP0_DumpLog ) );
	GlobalPreferenceSystem().registerPreference( "RegionBoxShader", CopiedStringImportStringCaller( g_regionBoxShader ), CopiedStringExportStringCaller( g_regionBoxShader ) );
	Build_registerPreferencesPage();
}

void BuildMonitor_Destroy(){
	delete g_pWatchBSP;
	g_pWatchBSP = NULL;
}

CWatchBSP *GetWatchBSP(){
	return g_pWatchBSP;
}

void BuildMonitor_Run( const std::vector<CopiedString>& commands, const char* mapName ){
	GetWatchBSP()->DoMonitoringLoop( commands, mapName );
}


// Static functions for the SAX callbacks -------------------------------------------------------

// utility for saxStartElement below
static void abortStream( message_info_t *data ){
	GetWatchBSP()->EndMonitoringLoop();
	// tell there has been an error
#if 0
	if ( GetWatchBSP()->HasBSPPlugin() ) {
		g_BSPFrontendTable.m_pfnEndListen( 2 );
	}
#endif
	// yeah this doesn't look good.. but it's needed so that everything will be ignored until the stream goes out
	data->ignore_depth = -1;
	data->recurse++;
}

#include "stream_version.h"

static void saxStartElement( message_info_t *data, const xmlChar *name, const xmlChar **attrs ){
#if 0
	globalOutputStream() << '<' << name;
	if ( attrs != 0 ) {
		for ( const xmlChar** p = attrs; *p != 0; p += 2 )
		{
			globalOutputStream() << ' ' << p[0] << '=' << makeQuoted( p[1] );
		}
	}
	globalOutputStream() << ">\n";
#endif

	if ( data->ignore_depth == 0 ) {
		if ( data->pGeometry != 0 ) {
			// we have a handler
			data->pGeometry->saxStartElement( data, name, attrs );
		}
		else
		{
			if ( strcmp( reinterpret_cast<const char*>( name ), "q3map_feedback" ) == 0 ) {
				// check the correct version
				// old q3map don't send a version attribute
				// the ones we support .. send Q3MAP_STREAM_VERSION
				if ( !attrs[0] || !attrs[1] || ( strcmp( reinterpret_cast<const char*>( attrs[0] ), "version" ) != 0 ) ) {
					globalErrorStream() << "No stream version given in the feedback stream, this is an old q3map version.\n"
					                       "Please turn off monitored compiling if you still wish to use this q3map executable\n";
					abortStream( data );
					return;
				}
				else if ( strcmp( reinterpret_cast<const char*>( attrs[1] ), Q3MAP_STREAM_VERSION ) != 0 ) {
					globalErrorStream() <<
					    "This version of Radiant reads version " Q3MAP_STREAM_VERSION " debug streams, I got an incoming connection with version " << reinterpret_cast<const char*>( attrs[1] ) << "\n"
					    "Please make sure your versions of Radiant and q3map are matching.\n";
					abortStream( data );
					return;
				}
			}
			// we don't treat locally
			else if ( strcmp( reinterpret_cast<const char*>( name ), "message" ) == 0 ) {
				int msg_level = atoi( reinterpret_cast<const char*>( attrs[1] ) );
				if ( msg_level != data->msg_level ) {
					data->msg_level = msg_level;
				}
			}
			else if ( strcmp( reinterpret_cast<const char*>( name ), "polyline" ) == 0 ) {
				// polyline has a particular status .. right now we only use it for leakfile ..
				data->geometry_depth = data->recurse;
				data->pGeometry = &g_pointfile;
				data->pGeometry->saxStartElement( data, name, attrs );
			}
			else if ( strcmp( reinterpret_cast<const char*>( name ), "select" ) == 0 ) {
				CSelectMsg *pSelect = new CSelectMsg();
				data->geometry_depth = data->recurse;
				data->pGeometry = pSelect;
				data->pGeometry->saxStartElement( data, name, attrs );
			}
			else if ( strcmp( reinterpret_cast<const char*>( name ), "pointmsg" ) == 0 ) {
				CPointMsg *pPoint = new CPointMsg();
				data->geometry_depth = data->recurse;
				data->pGeometry = pPoint;
				data->pGeometry->saxStartElement( data, name, attrs );
			}
			else if ( strcmp( reinterpret_cast<const char*>( name ), "windingmsg" ) == 0 ) {
				CWindingMsg *pWinding = new CWindingMsg();
				data->geometry_depth = data->recurse;
				data->pGeometry = pWinding;
				data->pGeometry->saxStartElement( data, name, attrs );
			}
			else
			{
				globalWarningStream() << "Warning: ignoring unrecognized node in XML stream (" << reinterpret_cast<const char*>( name ) << ")\n";
				// we don't recognize this node, jump over it
				// (NOTE: the ignore mechanism is a bit screwed, only works when starting an ignore at the highest level)
				data->ignore_depth = data->recurse;
			}
		}
	}
	data->recurse++;
}

static void saxEndElement( message_info_t *data, const xmlChar *name ){
#if 0
	globalOutputStream() << '<' << name << "/>\n";
#endif

	data->recurse--;
	// we are out of an ignored chunk
	if ( data->recurse == data->ignore_depth ) {
		data->ignore_depth = 0;
		return;
	}
	if ( data->pGeometry != 0 ) {
		data->pGeometry->saxEndElement( data, name );
		// we add the object to the debug window
		if ( data->geometry_depth == data->recurse ) {
			g_DbgDlg.Push( data->pGeometry );
			data->pGeometry = 0;
		}
	}
	if ( data->recurse == data->stop_depth ) {
#ifdef _DEBUG
		globalWarningStream() << "Received error msg .. shutting down..\n";
#endif
		GetWatchBSP()->EndMonitoringLoop();
		// tell there has been an error
#if 0
		if ( GetWatchBSP()->HasBSPPlugin() ) {
			g_BSPFrontendTable.m_pfnEndListen( 2 );
		}
#endif
		return;
	}
}

class MessageOutputStream : public TextOutputStream
{
	message_info_t* m_data;
public:
	MessageOutputStream( message_info_t* data ) : m_data( data ){
	}
	std::size_t write( const char* buffer, std::size_t length ){
		if ( m_data->pGeometry != 0 ) {
			m_data->pGeometry->saxCharacters( m_data, reinterpret_cast<const xmlChar*>( buffer ), int( length ) );
		}
		else
		{
			if ( m_data->ignore_depth == 0 ) {
				// output the message using the level
				Sys_Print( m_data->msg_level, buffer, length );
				// if this message has error level flag, we mark the depth to stop the compilation when we get out
				// we don't set the msg level if we don't stop on leak
				if ( m_data->msg_level == 3 ) {
					m_data->stop_depth = m_data->recurse - 1;
				}
			}
		}

		return length;
	}
};

template<typename T>
inline MessageOutputStream& operator<<( MessageOutputStream& ostream, const T& t ){
	return ostream_write( ostream, t );
}

static void saxCharacters( message_info_t *data, const xmlChar *ch, int len ){
	MessageOutputStream ostream( data );
	ostream << StringRange( reinterpret_cast<const char*>( ch ), len );
}

static void saxComment( void *ctx, const xmlChar *msg ){
	globalOutputStream() << "XML comment: " << reinterpret_cast<const char*>( msg ) << '\n';
}

static void saxWarning( void *ctx, const char *msg, ... ){
	char saxMsgBuffer[4096];
	va_list args;

	va_start( args, msg );
	vsprintf( saxMsgBuffer, msg, args );
	va_end( args );
	globalWarningStream() << "XML warning: " << saxMsgBuffer << '\n';
}

static void saxError( void *ctx, const char *msg, ... ){
	char saxMsgBuffer[4096];
	va_list args;

	va_start( args, msg );
	vsprintf( saxMsgBuffer, msg, args );
	va_end( args );
	globalErrorStream() << "XML error: " << saxMsgBuffer << '\n';
}

static void saxFatal( void *ctx, const char *msg, ... ){
	char buffer[4096];

	va_list args;

	va_start( args, msg );
	vsprintf( buffer, msg, args );
	va_end( args );
	globalErrorStream() << "XML fatal error: " << buffer << '\n';
}

static xmlSAXHandler saxParser = {
	0, /* internalSubset */
	0, /* isStandalone */
	0, /* hasInternalSubset */
	0, /* hasExternalSubset */
	0, /* resolveEntity */
	0, /* getEntity */
	0, /* entityDecl */
	0, /* notationDecl */
	0, /* attributeDecl */
	0, /* elementDecl */
	0, /* unparsedEntityDecl */
	0, /* setDocumentLocator */
	0, /* startDocument */
	0, /* endDocument */
	(startElementSAXFunc)saxStartElement, /* startElement */
	(endElementSAXFunc)saxEndElement, /* endElement */
	0, /* reference */
	(charactersSAXFunc)saxCharacters, /* characters */
	0, /* ignorableWhitespace */
	0, /* processingInstruction */
	(commentSAXFunc)saxComment, /* comment */
	(warningSAXFunc)saxWarning, /* warning */
	(errorSAXFunc)saxError, /* error */
	(fatalErrorSAXFunc)saxFatal, /* fatalError */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

// ------------------------------------------------------------------------------------------------

void CWatchBSP::Reset(){
	if ( m_pInSocket ) {
		Net_Disconnect( m_pInSocket );
		m_pInSocket = NULL;
	}
	if ( m_pListenSocket ) {
		Net_Disconnect( m_pListenSocket );
		m_pListenSocket = NULL;
	}
	if ( m_xmlInputBuffer ) {
		xmlFreeParserInputBuffer( m_xmlInputBuffer );
		m_xmlInputBuffer = NULL;
	}
	m_eState = EIdle;
	m_monitoring_timer.stop();
}

bool CWatchBSP::SetupListening(){
#ifdef _DEBUG
	if ( m_pListenSocket ) {
		globalErrorStream() << "ERROR: m_pListenSocket != NULL in CWatchBSP::SetupListening\n";
		return false;
	}
#endif
	globalOutputStream() << "Setting up\n";
	Net_Setup();
	m_pListenSocket = Net_ListenSocket( 39000 );
	if ( m_pListenSocket == NULL ) {
		return false;
	}
	globalOutputStream() << "Listening...\n";
	return true;
}

void CWatchBSP::DoEBeginStep(){
	Reset();
	if ( SetupListening() == false ) {
		const char* msg = "Failed to get a listening socket on port 39000.\nTry running with Build monitoring disabled if you can't fix this.\n";
		globalOutputStream() << msg;
		qt_MessageBox( MainFrame_getWindow(), msg, "Build monitoring", EMessageBoxType::Error );
		return;
	}
	// set the timer for timeouts and step cancellation
	m_timeout_timer.start();

	if ( !m_bBSPPlugin ) {
		globalOutputStream() << "=== running build command ===\n"
		                     << m_commands[m_iCurrentStep] << '\n';

		if ( !Q_Exec( NULL, const_cast<char*>( m_commands[m_iCurrentStep].c_str() ), NULL, true, false ) ) {
			const auto msg = StringStream( "Failed to execute the following command: ", m_commands[m_iCurrentStep],
			                               "\nCheck that the file exists and that you don't run out of system resources.\n" );
			globalOutputStream() << msg;
			qt_MessageBox( MainFrame_getWindow(), msg, "Build monitoring", EMessageBoxType::Error );
			return;
		}
		// re-initialise the debug window
		if ( m_iCurrentStep == 0 ) {
			g_DbgDlg.Init();
		}
	}
	m_eState = EBeginStep;
	m_monitoring_timer.start();
}

void CWatchBSP::RoutineProcessing(){
	switch ( m_eState )
	{
	case EBeginStep:
		// timeout: if we don't get an incoming connection fast enough, go back to idle
		if ( m_timeout_timer.elapsed_sec() > g_WatchBSP_Timeout ) {
			qt_MessageBox( MainFrame_getWindow(),  "The connection timed out, assuming the build process failed\n"
			                                       "Make sure you are using a networked version of Q3Map?\n"
			                                       "Otherwise you need to disable BSP Monitoring in prefs.", "BSP process monitoring" );
			EndMonitoringLoop();
#if 0
			if ( m_bBSPPlugin ) {
				// status == 1 : didn't get the connection
				g_BSPFrontendTable.m_pfnEndListen( 1 );
			}
#endif
			return;
		}
#ifdef _DEBUG
		// some debug checks
		if ( !m_pListenSocket ) {
			globalErrorStream() << "ERROR: m_pListenSocket == NULL in CWatchBSP::RoutineProcessing EBeginStep state\n";
			return;
		}
#endif
		// we are not connected yet, accept any incoming connection
		m_pInSocket = Net_Accept( m_pListenSocket );
		if ( m_pInSocket ) {
			globalOutputStream() << "Connected.\n";
			// prepare the message info struct for diving in
			memset( &m_message_info, 0, sizeof( message_info_t ) );
			// a dumb flag to make sure we init the push parser context when first getting a msg
			m_bNeedCtxtInit = true;
			m_eState = EWatching;
		}
		break;
	case EWatching:
		{
#ifdef _DEBUG
			// some debug checks
			if ( !m_pInSocket ) {
				globalErrorStream() << "ERROR: m_pInSocket == NULL in CWatchBSP::RoutineProcessing EWatching state\n";
				return;
			}
#endif

			int ret = Net_Wait( m_pInSocket, 0, 0 );
			if ( ret == -1 ) {
				globalErrorStream() << "SOCKET_ERROR in CWatchBSP::RoutineProcessing\n";
				globalErrorStream() << "Terminating the connection.\n";
				EndMonitoringLoop();
				return;
			}

			if ( ret == 1 ) {
				// the socket has been identified, there's something (message or disconnection)
				// see if there's anything in input
				ret = Net_Receive( m_pInSocket, &msg );
				if ( ret > 0 ) {
					//        unsigned int size = msg.size; //++timo just a check
					strcpy( m_xmlBuf, NMSG_ReadString( &msg ) );
					if ( m_bNeedCtxtInit ) {
						m_xmlParserCtxt = NULL;
						m_xmlParserCtxt = xmlCreatePushParserCtxt( &saxParser, &m_message_info, m_xmlBuf, static_cast<int>( strlen( m_xmlBuf ) ), NULL );

						if ( m_xmlParserCtxt == NULL ) {
							globalErrorStream() << "Failed to create the XML parser (incoming stream began with: " << m_xmlBuf << ")\n";
							EndMonitoringLoop();
						}
						m_bNeedCtxtInit = false;
					}
					else
					{
						xmlParseChunk( m_xmlParserCtxt, m_xmlBuf, static_cast<int>( strlen( m_xmlBuf ) ), 0 );
					}
				}
				else
				{
					// error or connection closed/reset
					// NOTE: if we get an error down the XML stream we don't reach here
					Net_Disconnect( m_pInSocket );
					m_pInSocket = NULL;
					globalOutputStream() << "Connection closed.\n";
#if 0
					if ( m_bBSPPlugin ) {
						EndMonitoringLoop();
						// let the BSP plugin know that the job is done
						g_BSPFrontendTable.m_pfnEndListen( 0 );
						return;
					}
#endif
					// move to next step or finish
					m_iCurrentStep++;
					if ( m_iCurrentStep < m_commands.size() ) {
						DoEBeginStep();
					}
					else
					{
						// launch the engine .. OMG
						if ( g_WatchBSP_RunQuake ) {
							globalOutputStream() << "Running engine...\n";
							// this is game dependant
							const auto [exe, args] = [&](){
								if( string_equal( gamemode_get(), "mp" ) ){
									if( const auto exe = g_engineExecutableMP.string(); !exe.empty() )
										return std::pair( std::move( exe ), g_engineArgsMP.string() );
								}
								return std::pair( g_engineExecutable.string(), g_engineArgs.string() );
							}();

							auto cmd = StringStream( '"', EnginePath_get(), exe, '"', ' ' );

							if( const char *map = strstr( args.c_str(), "%mapname%" ) )
								cmd << StringRange( args.c_str(), map ) << m_sBSPName << ( map + strlen( "%mapname%" ) );
							else
								cmd << args;

							globalOutputStream() << cmd << '\n';

							// execute now
							if ( !Q_Exec( nullptr, cmd.c_str(), EnginePath_get(), false, false ) ) {
								const auto msg = StringStream( "Failed to execute the following command: ", cmd, '\n' );
								globalOutputStream() << msg;
								qt_MessageBox( MainFrame_getWindow(), msg, "Build monitoring", EMessageBoxType::Error );
							}
						}
						EndMonitoringLoop();
					}
				}
			}
		}
		break;
	default:
		break;
	}
}

void CWatchBSP::DoMonitoringLoop( const std::vector<CopiedString>& commands, const char *sBSPName ){
	m_sBSPName = sBSPName;
	if ( m_eState != EIdle ) {
		globalWarningStream() << "WatchBSP got a monitoring request while not idling...\n";
		// prompt the user, should we cancel the current process and go ahead?
//		if ( qt_MessageBox( MainFrame_getWindow(),  "I am already monitoring a Build process.\nDo you want me to override and start a new compilation?",
//							 "Build process monitoring", EMessageBoxType::Question ) == eIDYES ) {
			// disconnect and set EIdle state
			Reset();
//		}
	}
	m_commands = commands;
	m_iCurrentStep = 0;
	DoEBeginStep();
}

void CWatchBSP::ExternalListen(){
	m_bBSPPlugin = true;
	DoEBeginStep();
}

// the part of the watchbsp interface we export to plugins
// NOTE: in the long run, the whole watchbsp.cpp interface needs to go out and be handled at the BSP plugin level
// for now we provide something really basic and limited, the essential is to have something that works fine and fast (for 1.1 final)
void QERApp_Listen(){
	// open the listening socket
	GetWatchBSP()->ExternalListen();
}
