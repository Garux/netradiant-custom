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


#ifdef WIN32
#include <windows.h>

#define socklen_t int
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l_net.h"
#include "l_net_wins.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#define SOCKET_ERROR   -1
#define INVALID_SOCKET -1
#define ioctlsocket    ioctl
#define closesocket    close

#define LPSTR const char *

int WSAGetLastError(){
	return errno;
}
#endif

#define WinError WinPrint

typedef struct tag_error_struct
{
	int errnum;
	LPSTR errstr;
} ERROR_STRUCT;

#define NET_NAMELEN         64
#define DEFAULTnet_hostport 26000
#define MAXHOSTNAMELEN      256

static unsigned long myAddr;

#ifdef WIN32
WSADATA winsockdata;
#endif

ERROR_STRUCT errlist[] = {
#ifdef WIN32
	{WSAEINTR,           "WSAEINTR - Interrupted"                                               },
	{WSAEBADF,           "WSAEBADF - Bad file number"                                           },
	{WSAEFAULT,          "WSAEFAULT - Bad address"                                              },
	{WSAEINVAL,          "WSAEINVAL - Invalid argument"                                         },
	{WSAEMFILE,          "WSAEMFILE - Too many open files"                                      },

	/*
	 *    Windows Sockets definitions of regular Berkeley error constants
	 */

	{WSAEWOULDBLOCK,     "WSAEWOULDBLOCK - Socket marked as non-blocking"                       },
	{WSAEINPROGRESS,     "WSAEINPROGRESS - Blocking call in progress"                           },
	{WSAEALREADY,        "WSAEALREADY - Command already completed"                              },
	{WSAENOTSOCK,        "WSAENOTSOCK - Descriptor is not a socket"                             },
	{WSAEDESTADDRREQ,    "WSAEDESTADDRREQ - Destination address required"                       },
	{WSAEMSGSIZE,        "WSAEMSGSIZE - Data size too large"                                    },
	{WSAEPROTOTYPE,      "WSAEPROTOTYPE - Protocol is of wrong type for this socket"            },
	{WSAENOPROTOOPT,     "WSAENOPROTOOPT - Protocol option not supported for this socket type"  },
	{WSAEPROTONOSUPPORT, "WSAEPROTONOSUPPORT - Protocol is not supported"                       },
	{WSAESOCKTNOSUPPORT, "WSAESOCKTNOSUPPORT - Socket type not supported by this address family"},
	{WSAEOPNOTSUPP,      "WSAEOPNOTSUPP - Option not supported"                                 },
	{WSAEPFNOSUPPORT,    "WSAEPFNOSUPPORT - "                                                   },
	{WSAEAFNOSUPPORT,    "WSAEAFNOSUPPORT - Address family not supported by this protocol"      },
	{WSAEADDRINUSE,      "WSAEADDRINUSE - Address is in use"                                    },
	{WSAEADDRNOTAVAIL,   "WSAEADDRNOTAVAIL - Address not available from local machine"          },
	{WSAENETDOWN,        "WSAENETDOWN - Network subsystem is down"                              },
	{WSAENETUNREACH,     "WSAENETUNREACH - Network cannot be reached"                           },
	{WSAENETRESET,       "WSAENETRESET - Connection has been dropped"                           },
	{WSAECONNABORTED,    "WSAECONNABORTED - Connection aborted"                                 },
	{WSAECONNRESET,      "WSAECONNRESET - Connection reset"                                     },
	{WSAENOBUFS,         "WSAENOBUFS - No buffer space available"                               },
	{WSAEISCONN,         "WSAEISCONN - Socket is already connected"                             },
	{WSAENOTCONN,        "WSAENOTCONN - Socket is not connected"                                },
	{WSAESHUTDOWN,       "WSAESHUTDOWN - Socket has been shut down"                             },
	{WSAETOOMANYREFS,    "WSAETOOMANYREFS - Too many references"                                },
	{WSAETIMEDOUT,       "WSAETIMEDOUT - Command timed out"                                     },
	{WSAECONNREFUSED,    "WSAECONNREFUSED - Connection refused"                                 },
	{WSAELOOP,           "WSAELOOP - "                                                          },
	{WSAENAMETOOLONG,    "WSAENAMETOOLONG - "                                                   },
	{WSAEHOSTDOWN,       "WSAEHOSTDOWN - Host is down"                                          },
	{WSAEHOSTUNREACH,    "WSAEHOSTUNREACH - "                                                   },
	{WSAENOTEMPTY,       "WSAENOTEMPTY - "                                                      },
	{WSAEPROCLIM,        "WSAEPROCLIM - "                                                       },
	{WSAEUSERS,          "WSAEUSERS - "                                                         },
	{WSAEDQUOT,          "WSAEDQUOT - "                                                         },
	{WSAESTALE,          "WSAESTALE - "                                                         },
	{WSAEREMOTE,         "WSAEREMOTE - "                                                        },

	/*
	 *    Extended Windows Sockets error constant definitions
	 */

	{WSASYSNOTREADY,     "WSASYSNOTREADY - Network subsystem not ready"                         },
	{WSAVERNOTSUPPORTED, "WSAVERNOTSUPPORTED - Version not supported"                           },
	{WSANOTINITIALISED,  "WSANOTINITIALISED - WSAStartup() has not been successfully called"    },

	/*
	 *    Other error constants.
	 */

	{WSAHOST_NOT_FOUND,  "WSAHOST_NOT_FOUND - Host not found"                                   },
	{WSATRY_AGAIN,       "WSATRY_AGAIN - Host not found or SERVERFAIL"                          },
	{WSANO_RECOVERY,     "WSANO_RECOVERY - Non-recoverable error"                               },
	{WSANO_DATA,         "WSANO_DATA - (or WSANO_ADDRESS) - No data record of requested type"   },
	{-1,                 NULL                                                                   }
#else
	{EACCES, "EACCES - The address is protected, user is not root"},
	{EAGAIN, "EAGAIN - Operation on non-blocking socket that cannot return immediately"},
	{EBADF, "EBADF - sockfd is not a valid descriptor"},
	{EFAULT, "EFAULT - The parameter is not in a writable part of the user address space"},
	{EINVAL, "EINVAL - The socket is already bound to an address"},
	{ENOBUFS, "ENOBUFS - not enough memory"},
	{ENOMEM, "ENOMEM - not enough memory"},
	{ENOTCONN, "ENOTCONN - not connected"},
	{ENOTSOCK, "ENOTSOCK - Argument is file descriptor not a socket"},
	{EOPNOTSUPP, "ENOTSUPP - The referenced socket is not of type SOCK_STREAM"},
	{EPERM, "EPERM - Firewall rules forbid connection"},
	{-1, NULL}
#endif
};


const char *WINS_ErrorMessage( int error ){
	int search = 0;

	if ( !error ) {
		return "No error occurred";
	}

	for ( search = 0; errlist[search].errstr; search++ ) {
		if ( error == errlist[search].errnum ) {
			return errlist[search].errstr;
		}
	}

	return "Unknown error";
}

bool WINS_Init( void ){
	int i;
	struct hostent *local;
	char buff[MAXHOSTNAMELEN];
	char *p;
#ifdef WIN32
	{
		int r;
		WORD wVersionRequested;

		wVersionRequested = MAKEWORD( 1, 1 );

		r = WSAStartup( wVersionRequested, &winsockdata );

		if ( r ) {
			WinPrint( "Winsock initialization failed.\n" );
			return false;
		}
	}
#endif

	// determine my name & address
	gethostname( buff, MAXHOSTNAMELEN );
	local = gethostbyname( buff );
	if ( local && local->h_addr_list && local->h_addr_list[0] ) {
		myAddr = *(int *)local->h_addr_list[0];
	}
	else{
		myAddr = inet_addr( "127.0.0.1" );
	}

	{
		// see if it's a text IP address (well, close enough)
		for ( p = buff; *p; p++ )
			if ( ( *p < '0' || *p > '9' ) && *p != '.' ) {
				break;
			}

		// if it is a real name, strip off the domain; we only want the host
		if ( *p ) {
			for ( i = 0; i < 15; i++ )
				if ( buff[i] == '.' ) {
					break;
				}
			buff[i] = 0;
		}
	}

	WinPrint( "Winsock Initialized\n" );

	return true;
}

void WINS_Shutdown( void ){
#ifdef WIN32
	WSACleanup();
#endif
	//
	//WinPrint( "Winsock Shutdown\n" );
}

int WINS_OpenReliableSocket( int port ){
	int newsocket;
	struct sockaddr_in address;
	int _true = 0xFFFFFFFF;

	// IPPROTO_TCP
	if ( ( newsocket = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
		WinPrint( "WINS_OpenReliableSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}

#ifndef WIN32
	// set SO_REUSEADDR to prevent "Address already in use"
	if ( setsockopt( newsocket, SOL_SOCKET, SO_REUSEADDR, (void *)&_true, sizeof( int ) ) == -1 ) {
		WinPrint( "WINS_OpenReliableSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		WinPrint( "setsockopt so_reuseaddr error\n" );
	}
#endif

	memset( (char *)&address, 0, sizeof( address ) );
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl( INADDR_ANY );
	address.sin_port = htons( (unsigned short)port );
	if ( bind( newsocket, (void *)&address, sizeof( address ) ) == -1 ) {
		WinPrint( "WINS_OpenReliableSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		closesocket( newsocket );
		return -1;
	}

	if ( setsockopt( newsocket, IPPROTO_TCP, TCP_NODELAY, (void *)&_true, sizeof( int ) ) == SOCKET_ERROR ) {
		WinPrint( "WINS_OpenReliableSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		WinPrint( "setsockopt tcp_nodelay error\n" );
	}

	return newsocket;
}

int WINS_Listen( int socket ){
	unsigned long _true = 1;

	if ( ioctlsocket( socket, FIONBIO, &_true ) == -1 ) {
		WinPrint( "WINS_Listen: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}
	if ( listen( socket, SOMAXCONN ) == SOCKET_ERROR ) {
		WinPrint( "WINS_Listen: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}
	return 0;
}

int WINS_Accept( int socket, struct sockaddr_s *addr ){
	socklen_t addrlen = sizeof( struct sockaddr_s );
	int newsocket;
	int _true = 1;

	newsocket = accept( socket, (struct sockaddr *)addr, &addrlen );
	if ( newsocket == INVALID_SOCKET ) {
#ifdef WIN32
		if ( WSAGetLastError() == WSAEWOULDBLOCK ) {
			return -1;
		}
#else
		if ( WSAGetLastError() == EAGAIN ) {
			return -1;
		}
#endif
		WinPrint( "WINS_Accept: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}

	if ( setsockopt( newsocket, IPPROTO_TCP, TCP_NODELAY, (void *)&_true, sizeof( int ) ) == SOCKET_ERROR ) {
		WinPrint( "WINS_Accept: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		WinPrint( "setsockopt error\n" );
	}
	return newsocket;
}

int WINS_CloseSocket( int socket ){
#ifndef WIN32
	// cleanly shutdown socket communication
	if ( !shutdown( socket, SHUT_RDWR ) ) {
		WinPrint( "WINS_CloseSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		WinPrint( "shutdown socket error\n" );
	}
#endif

	if ( closesocket( socket ) == SOCKET_ERROR ) {
		WinPrint( "WINS_CloseSocket: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return SOCKET_ERROR;
	}
	return 0;
}

int WINS_Connect( int socket, struct sockaddr_s *addr ){
	int ret;
	unsigned long _true2 = 0xFFFFFFFF;

	ret = connect( socket, (struct sockaddr *)addr, sizeof( struct sockaddr_s ) );
	if ( ret == SOCKET_ERROR ) {
		WinPrint( "WINS_Connect: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}
	if ( ioctlsocket( socket, FIONBIO, &_true2 ) == -1 ) {
		WinPrint( "WINS_Connect: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
		return -1;
	}
	return 0;
}

/**
 * @return number of bytes to read
 *    0 if no bytes available
 *    -1 on failure
 */
int WINS_Read( int socket, byte *buf, int len, struct sockaddr_s *addr ){
	socklen_t addrlen = sizeof( struct sockaddr_s );
	int ret, error;

	if ( addr ) {
		ret = recvfrom( socket, buf, len, 0, (struct sockaddr *)addr, &addrlen );
	}
	else {
		ret = recv( socket, buf, len, 0 );
#ifndef WIN32
		// if there's no data on the socket ret == -1 and errno == EAGAIN
		// MSDN states that if ret == 0 the socket has been closed
		// man recv doesn't say anything
		if ( ret == 0 ) {
			return -1;
		}
#endif
	}

	// handle socket read error
	if ( ret == SOCKET_ERROR ) {
		error = WSAGetLastError();
		WinPrint( "WINS_Read: %s\n", WINS_ErrorMessage( error ) );

#ifdef WIN32
		if ( error == WSAEWOULDBLOCK || error == WSAECONNREFUSED ) {
			return 0;
		}
#else
		if ( error == EAGAIN || error == ENOTCONN ) {
			return 0;
		}
#endif
	}

	return ret;
}

/**
 * @return true on success or false on failure
 */
bool WINS_Write( int socket, byte *buf, int len, struct sockaddr_s *addr ){
	int ret = 0, written = 0;

	while ( written < len ) {
		if ( addr ) {
			ret = sendto( socket, &buf[written], len - written, 0, (struct sockaddr *)addr, sizeof( struct sockaddr_s ) );
		}
		else{
			ret = send( socket, buf, len, 0 );
		}

		if ( ret == SOCKET_ERROR ) {
#ifdef WIN32
			if ( WSAGetLastError() != WSAEWOULDBLOCK ) {
				return false;
			}
			Sleep( 1000 );
#else
			if ( WSAGetLastError() != EAGAIN ) {
				return false;
			}
#endif
		}
		else {
			written += ret;
		}
	}
	if ( ret == SOCKET_ERROR ) {
		WinPrint( "WINS_Write: %s\n", WINS_ErrorMessage( WSAGetLastError() ) );
	}
	return ( ret == len );
}

char *WINS_AddrToString( struct sockaddr_s *addr ){
	static char buffer[22];
	int haddr;

	haddr = ntohl( ( (struct sockaddr_in *)addr )->sin_addr.s_addr );
	sprintf( buffer, "%d.%d.%d.%d:%d", ( haddr >> 24 ) & 0xff, ( haddr >> 16 ) & 0xff, ( haddr >> 8 ) & 0xff, haddr & 0xff, ntohs( ( (struct sockaddr_in *)addr )->sin_port ) );
	return buffer;
}

int WINS_StringToAddr( char *string, struct sockaddr_s *addr ){
	int ha1, ha2, ha3, ha4, hp;
	int ipaddr;

	sscanf( string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp );
	ipaddr = ( ha1 << 24 ) | ( ha2 << 16 ) | ( ha3 << 8 ) | ha4;

	addr->sa_family = AF_INET;
	( (struct sockaddr_in *)addr )->sin_addr.s_addr = htonl( ipaddr );
	( (struct sockaddr_in *)addr )->sin_port = htons( (unsigned short)hp );
	return 0;
}

int WINS_GetSocketAddr( int socket, struct sockaddr_s *addr ){
	socklen_t addrlen = sizeof( struct sockaddr_s );
	unsigned int a;

	memset( addr, 0, sizeof( struct sockaddr_s ) );
	getsockname( socket, (struct sockaddr *)addr, &addrlen );
	a = ( (struct sockaddr_in *)addr )->sin_addr.s_addr;
	if ( a == 0 || a == inet_addr( "127.0.0.1" ) ) {
		( (struct sockaddr_in *)addr )->sin_addr.s_addr = myAddr;
	}

	return 0;
}
