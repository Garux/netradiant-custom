// Copyright 2009 Google Inc.
//
// Based on the code from Android ETC1Util.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "etclib.h"

static void ETC_DecodeETC1SubBlock( byte *out, qboolean outRGBA, int r, int g, int b, int tableIndex, unsigned int low, qboolean second, qboolean flipped ){
	int baseX = 0, baseY = 0;
	const int modifierTable[] = {
		2, 8, -2, -8,
		5, 17, -5, -17,
		9, 29, -9, -29,
		13, 42, -13, -42,
		18, 60, -18, -60,
		24, 80, -24, -80,
		33, 106, -33, -106,
		47, 183, -47, -183
	};
	const int *table = modifierTable + tableIndex * 4;
	int i;	

	if ( second ) {
		if ( flipped ) {
			baseY = 2;
		}
		else {
			baseX = 2;
		}
	}

	for ( i = 0; i < 8; i++ )
	{
		int x, y, k, delta;
		int qr, qg, qb;
		byte *q;

		if ( flipped ) {
			x = baseX + ( i >> 1 );
			y = baseY + ( i & 1 );
		}
		else {
			x = baseX + ( i >> 2 );
			y = baseY + ( i & 3 );
		}
		k = y + ( x * 4 );
		delta = table[( ( low >> k ) & 1 ) | ( ( low >> ( k + 15 ) ) & 2 )];

		qr = r + delta;
		qg = g + delta;
		qb = b + delta;
		if ( outRGBA ) {
			q = out + 4 * ( x + 4 * y );
		}
		else {
			q = out + 3 * ( x + 4 * y );
		}
		*( q++ ) = ( ( qr > 0 ) ? ( ( qr < 255 ) ? qr : 255 ) : 0 );
		*( q++ ) = ( ( qg > 0 ) ? ( ( qg < 255 ) ? qg : 255 ) : 0 );
		*( q++ ) = ( ( qb > 0 ) ? ( ( qb < 255 ) ? qb : 255 ) : 0 );
		if ( outRGBA ) {
			*( q++ ) = 255;
		}
	}
}

void ETC_DecodeETC1Block( const byte* in, byte* out, qboolean outRGBA ){
	unsigned int high = ( in[0] << 24 ) | ( in[1] << 16 ) | ( in[2] << 8 ) | in[3];
	unsigned int low = ( in[4] << 24 ) | ( in[5] << 16 ) | ( in[6] << 8 ) | in[7];
	int r1, r2, g1, g2, b1, b2;
	qboolean flipped = ( ( high & 1 ) != 0 );

	if ( high & 2 ) {
		int rBase, gBase, bBase;
		const int lookup[] = { 0, 1, 2, 3, -4, -3, -2, -1 };

		rBase = ( high >> 27 ) & 31;
		r1 = ( rBase << 3 ) | ( rBase >> 2 );
		rBase = ( rBase + ( lookup[( high >> 24 ) & 7] ) ) & 31;
		r2 = ( rBase << 3 ) | ( rBase >> 2 );

		gBase = ( high >> 19 ) & 31;
		g1 = ( gBase << 3 ) | ( gBase >> 2 );
		gBase = ( gBase + ( lookup[( high >> 16 ) & 7] ) ) & 31;
		g2 = ( gBase << 3 ) | ( gBase >> 2 );

		bBase = ( high >> 11 ) & 31;
		b1 = ( bBase << 3 ) | ( bBase >> 2 );
		bBase = ( bBase + ( lookup[( high >> 8 ) & 7] ) ) & 31;
		b2 = ( bBase << 3 ) | ( bBase >> 2 );
	}
	else {
		r1 = ( ( high >> 24 ) & 0xf0 ) | ( ( high >> 28 ) & 0xf );
		r2 = ( ( high >> 20 ) & 0xf0 ) | ( ( high >> 24 ) & 0xf );
		g1 = ( ( high >> 16 ) & 0xf0 ) | ( ( high >> 20 ) & 0xf );
		g2 = ( ( high >> 12 ) & 0xf0 ) | ( ( high >> 16 ) & 0xf );
		b1 = ( ( high >> 8 ) & 0xf0 ) | ( ( high >> 12 ) & 0xf );
		b2 = ( ( high >> 4 ) & 0xf0 ) | ( ( high >> 8 ) & 0xf );
	}

	ETC_DecodeETC1SubBlock( out, outRGBA, r1, g1, b1, ( high >> 5 ) & 7, low, qfalse, flipped );
	ETC_DecodeETC1SubBlock( out, outRGBA, r2, g2, b2, ( high >> 2 ) & 7, low, qtrue, flipped );
}
