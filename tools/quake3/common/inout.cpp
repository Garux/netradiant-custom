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

//-----------------------------------------------------------------------------
//
//
// DESCRIPTION:
// deal with in/out tasks, for either stdin/stdout or network/XML stream
//

#include "cmdlib.h"
#include "inout.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include "generic/vector.h"
#include "timer.h"
#include <thread>
#include <mutex>

#ifdef WIN32
#include <direct.h>
#include <windows.h>
#endif

// network broadcasting
#include "l_net/l_net.h"
#include <libxml/tree.h>

// utf8 conversion
#include <glib.h>

static socket_t *brdcst_socket;

// locks xml doc and mesege_*
// messages may come from various threads, 'force send' signal comes from dedicated thread
static std::recursive_mutex mesege_mutex;

bool verbose = false;

// our main document
// is streamed through the network to Radiant
// possibly written to disk at the end of the run
//++timo FIXME: need to be global, required when creating nodes?
static xmlDocPtr doc;

// some useful stuff
xmlNodePtr xml_NodeForVec( const Vector3& v ){
	xmlNodePtr ret;
	char buf[1024];

	sprintf( buf, "%f %f %f", v[0], v[1], v[2] );
	ret = xmlNewNode( NULL, (const xmlChar*)"point" );
	xmlNodeAddContent( ret, (const xmlChar*)buf );
	return ret;
}

static void xml_message_flush();

// send a node down the stream, add it to the document
void xml_SendNode( xmlNodePtr node ){
	std::lock_guard lock( mesege_mutex );

	xml_message_flush(); /* flush regular print messages buffer, so that special ones will appear at correct spot */

	xmlAddChild( doc->children, node );

	if ( brdcst_socket ) {
		xmlBufferPtr xml_buf = xmlBufferCreate();
		xmlNodeDump( xml_buf, doc, node, 0, 0 );

		// the XML node might be too big to fit in a single network message
		// l_net library defines an upper limit of MAX_NETMESSAGE
		// there are some size check errors, so we use MAX_NETMESSAGE-10 to be safe
		// if the size of the buffer exceeds MAX_NETMESSAGE-10 we'll send in several network messages
		for ( int pos = 0; pos < (int)xml_buf->use; )
		{
			// what size are we gonna send now?
			const int size = std::min( (int)xml_buf->use - pos, MAX_NETMESSAGE - 10 );
			netmessage_t msg;
			NMSG_Clear( &msg );
			NMSG_WriteString_n( &msg, reinterpret_cast<const char*>( xml_buf->content + pos ), size );
			Net_Send( brdcst_socket, &msg );
			// now that the thing is sent prepare to loop again
			pos += size;
		}

		xmlBufferFree( xml_buf );
	}
}

void xml_Select( const char *msg, int entitynum, int brushnum, bool bError ){
	xmlNodePtr node, select;
	char buf[1024];
	char level[2];

	// now build a proper "select" XML node
	sprintf( buf, "Entity %i, Brush %i: %s", entitynum, brushnum, msg );
	node = xmlNewNode( NULL, (const xmlChar*)"select" );
	xmlNodeAddContent( node, (const xmlChar*)buf );
	level[0] = (int)'0' + ( bError ? SYS_ERR : SYS_WRN );
	level[1] = 0;
	xmlSetProp( node, (const xmlChar*)"level", (const xmlChar *)level );
	// a 'select' information
	sprintf( buf, "%i %i", entitynum, brushnum );
	select = xmlNewNode( NULL, (const xmlChar*)"brush" );
	xmlNodeAddContent( select, (const xmlChar*)buf );
	xmlAddChild( node, select );
	xml_SendNode( node );

	sprintf( buf, "Entity %i, Brush %i: %s", entitynum, brushnum, msg );
	if ( bError ) {
		Error( buf );
	}
	else{
		Sys_FPrintf( SYS_NOXMLflag | SYS_WRN, "%s\n", buf );
	}
}

void xml_Point( const char *msg, const Vector3& pt ){
	xmlNodePtr node, point;
	char buf[1024];
	char level[2];

	node = xmlNewNode( NULL, (const xmlChar*)"pointmsg" );
	xmlNodeAddContent( node, (const xmlChar*)msg );
	level[0] = (int)'0' + SYS_ERR;
	level[1] = 0;
	xmlSetProp( node, (const xmlChar*)"level", (const xmlChar *)level );
	// a 'point' node
	sprintf( buf, "%g %g %g", pt[0], pt[1], pt[2] );
	point = xmlNewNode( NULL, (const xmlChar*)"point" );
	xmlNodeAddContent( point, (const xmlChar*)buf );
	xmlAddChild( node, point );
	xml_SendNode( node );

	sprintf( buf, "%s (%g %g %g)", msg, pt[0], pt[1], pt[2] );
	Error( buf );
}

#define WINDING_BUFSIZE 2048
void xml_Winding( const char *msg, const Vector3 p[], int numpoints, bool die ){
	xmlNodePtr node, winding;
	char buf[WINDING_BUFSIZE];
	char smlbuf[128];
	char level[2];

	node = xmlNewNode( NULL, (const xmlChar*)"windingmsg" );
	xmlNodeAddContent( node, (const xmlChar*)msg );
	level[0] = (int)'0' + SYS_ERR;
	level[1] = 0;
	xmlSetProp( node, (const xmlChar*)"level", (const xmlChar *)level );
	// a 'winding' node
	sprintf( buf, "%i ", numpoints );
	for ( int i = 0; i < numpoints; ++i )
	{
		sprintf( smlbuf, "(%g %g %g)", p[i][0], p[i][1], p[i][2] );
		// don't overflow
		if ( strlen( buf ) + strlen( smlbuf ) >= WINDING_BUFSIZE ) {
			break;
		}
		strcat( buf, smlbuf );
	}

	winding = xmlNewNode( NULL, (const xmlChar*)"winding" );
	xmlNodeAddContent( winding, (const xmlChar*)buf );
	xmlAddChild( node, winding );
	xml_SendNode( node );

	if ( die ) {
		Error( msg );
	}
	else
	{
		Sys_Printf( msg );
		Sys_Printf( "\n" );
	}
}

static void set_console_colour_for_flag( int flag ){
#ifdef WIN32
	static int curFlag = SYS_STD;
	static bool ok = true;
	static bool initialized = false;
	static HANDLE hConsole;
	static WORD colour_saved;
	if( !ok )
		return;
	if( !initialized ){
		hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
		CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
		if( hConsole == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo( hConsole, &consoleInfo ) ){
			ok = false;
			return;
		}
		colour_saved = consoleInfo.wAttributes;
		initialized = true;
	}
	if( curFlag != flag ){
		curFlag = flag;
		SetConsoleTextAttribute( hConsole, flag == SYS_WRN ? FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
		                                 : flag == SYS_ERR ? FOREGROUND_RED | FOREGROUND_INTENSITY
		                                 : colour_saved );
	}
#endif
}


#define MAX_MESEGE      MAX_NETMESSAGE / 2
static char mesege[MAX_MESEGE];
static size_t mesege_len = 0;
static int mesege_flag = SYS_STD;
static Timer mesege_send_timer;


// in include
#include "stream_version.h"

void Broadcast_Setup( const char *dest ){
	address_t address;

	Net_Setup();
	Net_StringToAddress( dest, &address );
	brdcst_socket = Net_Connect( &address, 0 );
	if ( brdcst_socket ) {
		// send in a header
		const char string[] = "<?xml version=\"1.0\"?><q3map_feedback version=\"" Q3MAP_STREAM_VERSION "\">";
		netmessage_t msg;
		NMSG_Clear( &msg );
		NMSG_WriteString( &msg, string );
		Net_Send( brdcst_socket, &msg );

		std::thread ( [](){
			while( true ){
				std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
				std::lock_guard lock( mesege_mutex );
				if( mesege_send_timer.elapsed_msec() + 1000 * mesege_len / MAX_MESEGE > 2000 ){ // force send if >1-2 seconds has passed
					xml_message_flush();
				}
			}
		} ).detach();
	}
}

void Broadcast_Shutdown(){
	if ( brdcst_socket ) {
		Sys_Printf( "Disconnecting\n" );
		xml_message_flush();
		Net_Disconnect( brdcst_socket );
		brdcst_socket = NULL;
	}
	set_console_colour_for_flag( SYS_STD ); //restore default on exit
}

static void xml_message_flush(){
	std::lock_guard lock( mesege_mutex );

	if( mesege_len == 0 )
		return;

	xmlNodePtr node = xmlNewNode( NULL, (const xmlChar*)"message" );
	{
		mesege[mesege_len] = '\0';
		mesege_len = 0;
		gchar* utf8 = g_locale_to_utf8( mesege, -1, NULL, NULL, NULL );
		xmlNodeAddContent( node, (const xmlChar*)utf8 );
		g_free( utf8 );
	}
	char level[2];
	level[0] = (int)'0' + mesege_flag;
	level[1] = 0;
	xmlSetProp( node, (const xmlChar*)"level", (const xmlChar *)level );

	xml_SendNode( node );

	mesege_send_timer.start();
}

static void xml_message_push( int flag, const char* characters, size_t length ){
	std::lock_guard lock( mesege_mutex );

	if( flag != mesege_flag ){
		xml_message_flush();
		mesege_flag = flag;
	}

	const char* end = characters + length;
	while ( characters != end )
	{
		const size_t space = MAX_MESEGE - 1 - mesege_len;
		if ( space == 0 ) {
			xml_message_flush();
		}
		else
		{
			const size_t size = std::min( space, static_cast<size_t>( end - characters ) );
			memcpy( mesege + mesege_len, characters, size );
			mesege_len += size;
			characters += size;
		}
	}
}

// all output ends up through here
static void FPrintf( int flag, char *buf ){
	static bool bGotXML = false;

	set_console_colour_for_flag( flag & ~( SYS_NOXMLflag | SYS_VRBflag ) );
	printf( "%s", buf );

	// the following part is XML stuff only.. but maybe we don't want that message to go down the XML pipe?
	if ( flag & SYS_NOXMLflag ) {
		return;
	}

	// output an XML file of the run
	// use the DOM interface to build a tree
	/*
	   <message level='flag'>
	   message string
	   .. various nodes to describe corresponding geometry ..
	   </message>
	 */
	if ( !bGotXML ) {
		// initialize
		doc = xmlNewDoc( (const xmlChar*)"1.0" );
		doc->children = xmlNewDocRawNode( doc, NULL, (const xmlChar*)"q3map_feedback", NULL );
		bGotXML = true;
	}
	xml_message_push( flag & ~( SYS_NOXMLflag | SYS_VRBflag ), buf, strlen( buf ) );
}

#ifdef DBG_XML
void DumpXML(){
	xmlSaveFile( "XMLDump.xml", doc );
}
#endif

void Sys_FPrintf( int flag, const char *format, ... ){
	char out_buffer[4096];
	va_list argptr;

	if ( ( flag & SYS_VRBflag ) && !verbose ) {
		return;
	}

	va_start( argptr, format );
	vsprintf( out_buffer, format, argptr );
	va_end( argptr );

	FPrintf( flag, out_buffer );
}

void Sys_Printf( const char *format, ... ){
	char out_buffer[4096];
	va_list argptr;

	va_start( argptr, format );
	vsprintf( out_buffer, format, argptr );
	va_end( argptr );

	FPrintf( SYS_STD, out_buffer );
}

void Sys_Warning( const char *format, ... ){
	char out_buffer[4096];
	va_list argptr;

	va_start( argptr, format );
	sprintf( out_buffer, "WARNING: " );
	vsprintf( out_buffer + strlen( "WARNING: " ), format, argptr );
	va_end( argptr );

	FPrintf( SYS_WRN, out_buffer );
}

/*
   =================
   Error

   For abnormal program terminations
   =================
 */
void Error( const char *error, ... ){
	char out_buffer[4096];
	char tmp[4096];
	va_list argptr;

	va_start( argptr, error );
	vsprintf( tmp, error, argptr );
	va_end( argptr );

	sprintf( out_buffer, "************ ERROR ************\n%s\n", tmp );

	FPrintf( SYS_ERR, out_buffer );
	xml_message_flush();

#ifdef DBG_XML
	DumpXML();
#endif

	//++timo HACK ALERT .. if we shut down too fast the xml stream won't reach the listener.
	// a clean solution is to send a sync request node in the stream and wait for an answer before exiting
	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

	exit( 1 );
}
