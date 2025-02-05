/*
    mdfour.c

    An implementation of MD4 designed for use in the samba SMB
    authentication protocol

    Copyright (C) 1997-1998  Andrew Tridgell

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to:

        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA

    $Id: mdfour.c 7689 2007-11-12 14:28:40Z divverent $
 */

#include <cstring>     /* XoXus: needed for memset call */
#include <cstdint>
#include <array>

struct md4 {
	std::uint32_t A = 0x67452301, B = 0xefcdab89, C = 0x98badcfe, D = 0x10325476;
	std::uint32_t totalN = 0;
};

/* NOTE: This code makes no attempt to be fast!

   It assumes that a int is at least 32 bits long
 */

#define F( X, Y, Z ) ( ( ( X ) & ( Y ) ) | ( ( ~( X ) ) & ( Z ) ) )
#define G( X, Y, Z ) ( ( ( X ) & ( Y ) ) | ( ( X ) & ( Z ) ) | ( ( Y ) & ( Z ) ) )
#define H( X, Y, Z ) ( ( X ) ^ ( Y ) ^ ( Z ) )
#define lshift( x , s ) ( ( ( x ) << ( s ) ) | ( ( x ) >> ( 32 - ( s ) ) ) )

#define ROUND1( a, b, c, d, k, s ) a = lshift( a + F( b, c, d ) + X[k], s )
#define ROUND2( a, b, c, d, k, s ) a = lshift( a + G( b, c, d ) + X[k] + 0x5A827999, s )
#define ROUND3( a, b, c, d, k, s ) a = lshift( a + H( b, c, d ) + X[k] + 0x6ED9EBA1, s )

/* this applies md4 to 64 byte chunks */
inline void mdfour64( md4& m, const std::array<std::uint32_t, 16> X ){
	std::uint32_t A = m.A, B = m.B, C = m.C, D = m.D;

	ROUND1( A, B, C, D,  0,  3 );  ROUND1( D, A, B, C,  1,  7 );
	ROUND1( C, D, A, B,  2, 11 );  ROUND1( B, C, D, A,  3, 19 );
	ROUND1( A, B, C, D,  4,  3 );  ROUND1( D, A, B, C,  5,  7 );
	ROUND1( C, D, A, B,  6, 11 );  ROUND1( B, C, D, A,  7, 19 );
	ROUND1( A, B, C, D,  8,  3 );  ROUND1( D, A, B, C,  9,  7 );
	ROUND1( C, D, A, B, 10, 11 );  ROUND1( B, C, D, A, 11, 19 );
	ROUND1( A, B, C, D, 12,  3 );  ROUND1( D, A, B, C, 13,  7 );
	ROUND1( C, D, A, B, 14, 11 );  ROUND1( B, C, D, A, 15, 19 );

	ROUND2( A, B, C, D,  0,  3 );  ROUND2( D, A, B, C,  4,  5 );
	ROUND2( C, D, A, B,  8,  9 );  ROUND2( B, C, D, A, 12, 13 );
	ROUND2( A, B, C, D,  1,  3 );  ROUND2( D, A, B, C,  5,  5 );
	ROUND2( C, D, A, B,  9,  9 );  ROUND2( B, C, D, A, 13, 13 );
	ROUND2( A, B, C, D,  2,  3 );  ROUND2( D, A, B, C,  6,  5 );
	ROUND2( C, D, A, B, 10,  9 );  ROUND2( B, C, D, A, 14, 13 );
	ROUND2( A, B, C, D,  3,  3 );  ROUND2( D, A, B, C,  7,  5 );
	ROUND2( C, D, A, B, 11,  9 );  ROUND2( B, C, D, A, 15, 13 );

	ROUND3( A, B, C, D,  0,  3 );  ROUND3( D, A, B, C,  8,  9 );
	ROUND3( C, D, A, B,  4, 11 );  ROUND3( B, C, D, A, 12, 15 );
	ROUND3( A, B, C, D,  2,  3 );  ROUND3( D, A, B, C, 10,  9 );
	ROUND3( C, D, A, B,  6, 11 );  ROUND3( B, C, D, A, 14, 15 );
	ROUND3( A, B, C, D,  1,  3 );  ROUND3( D, A, B, C,  9,  9 );
	ROUND3( C, D, A, B,  5, 11 );  ROUND3( B, C, D, A, 13, 15 );
	ROUND3( A, B, C, D,  3,  3 );  ROUND3( D, A, B, C, 11,  9 );
	ROUND3( C, D, A, B,  7, 11 );  ROUND3( B, C, D, A, 15, 15 );

	m.A += A; m.B += B; m.C += C; m.D += D;
}

inline std::array<std::uint32_t, 16> copy64( const unsigned char *in ){
	std::array<std::uint32_t, 16> M;
	for ( int i = 0; i < 16; ++i )
		M[i] = ( in[i * 4 + 3] << 24 ) | ( in[i * 4 + 2] << 16 ) |
		       ( in[i * 4 + 1] << 8  ) | ( in[i * 4 + 0] << 0  );
	return M;
}

inline void copy4( unsigned char *out, std::uint32_t x ){
	out[0] = x & 0xFF;
	out[1] = ( x >> 8 ) & 0xFF;
	out[2] = ( x >> 16 ) & 0xFF;
	out[3] = ( x >> 24 ) & 0xFF;
}


inline void mdfour_tail( md4& m, const unsigned char *in, int n ){
	unsigned char buf[128] = {0};

	m.totalN += n;

	const std::uint32_t b = m.totalN * 8;

	if ( n ) {
		memcpy( buf, in, n );
	}
	buf[n] = 0x80;

	if ( n <= 55 ) {
		copy4( buf + 56, b );
		mdfour64( m, copy64( buf ) );
	}
	else {
		copy4( buf + 120, b );
		mdfour64( m, copy64( buf ) );
		mdfour64( m, copy64( buf + 64 ) );
	}
}

inline void mdfour_update( md4& m, const unsigned char *in, int n ){
// start of edit by Forest 'LordHavoc' Hale
// commented out to prevent crashing when length is 0
//	if ( n == 0 ) mdfour_tail( in, n );
// end of edit by Forest 'LordHavoc' Hale

	while ( n >= 64 ) {
		mdfour64( m, copy64( in ) );
		in += 64;
		n -= 64;
		m.totalN += 64;
	}

	mdfour_tail( m, in, n );
}


inline void mdfour_result( const md4& m, unsigned char *out ){
	copy4( out + 0,  m.A );
	copy4( out + 4,  m.B );
	copy4( out + 8,  m.C );
	copy4( out + 12, m.D );
}


inline void mdfour( unsigned char *out, const unsigned char *in, int n ){
	md4 m;
	mdfour_update( m, in, n );
	mdfour_result( m, out );
}

///////////////////////////////////////////////////////////////
//	MD4-based checksum utility functions
//
//	Copyright (C) 2000       Jeff Teunissen <d2deek@pmail.net>
//
//	Author: Jeff Teunissen	<d2deek@pmail.net>
//	Date: 01 Jan 2000

unsigned Com_BlockChecksum( const void *buffer, int length ){
	int digest[4];
	unsigned val;

	mdfour( (unsigned char *) digest, (const unsigned char *) buffer, length );

	val = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];

	return val;
}

void Com_BlockFullChecksum( const void *buffer, int len, unsigned char *outbuf ){
	mdfour( outbuf, (const unsigned char *) buffer, len );
}
