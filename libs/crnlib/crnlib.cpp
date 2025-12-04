/*
   Copyright (C) 2018, Unvanquished Developers
   All Rights Reserved.

   This file is part of NetRadiant.

   NetRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   NetRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with NetRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "crnlib.h"

#include <cstring>

#include <memory>
#include <algorithm>

#include "ddslib.h"
#include "crunch/inc/crn_decomp.h"

inline int LittleLong( int l ){
#if defined( __BIG_ENDIAN__ )
	std::reverse( reinterpret_cast<unsigned char*>( &l ), reinterpret_cast<unsigned char*>( &l ) + sizeof( int ) );
#endif
	return l;
}

// Sets `x` and `y` to the width and height of the input crn image. Returns false if there is an
// error reading the image.
extern "C" int GetCRNImageSize( const void *buffer, int length, int *x, int *y ) {
	crnd::crn_texture_info ti;
	if( !crnd::crnd_get_texture_info( buffer, length, &ti ) ||
	  // Ensure we are not trying to load a cubemap (which has 6 faces...)
	  ( ti.m_faces != 1 ) ) {
		return false;
	}
	if ( x ) *x = ti.m_width;
	if ( y ) *y = ti.m_height;
	return true;
}

// Converts a .crn file to RGBA. Stores the pixels in outBuf. Use GetCRNImageSize to get the image
// size to determine how big outBuf should be. The function will return false if the image does not
// fit inside outBuf.
extern "C" int ConvertCRNtoRGBA( const void *buffer, int length, unsigned int outBufLen, void* outBuf ) {
	crnd::crn_texture_info ti;
	if( !crnd::crnd_get_texture_info( buffer, length, &ti ) ||
	  // Ensure we are not trying to load a cubemap (which has 6 faces...)
	  ( ti.m_faces != 1 ) ) {
		return false;
	}

	// Sanity check mipmaps.
	if ( ti.m_levels <= 0 ) {
		return false;
	}

	// The largest layer is always layer 0, so load that one.
	crnd::crn_level_info li;
	if ( !crnd::crnd_get_level_info( buffer, length, 0, &li ) ) {
		return false;
	}

	// Ensure we can fit the final image in outBuf.
	if ( outBufLen < ti.m_width * ti.m_height ) {
		return false;
	}

	crnd::crnd_unpack_context ctx = crnd::crnd_unpack_begin( buffer, length );
	if ( !ctx ) {
		return false;
	}

	// Since the texture is compressed and the crunch library doesn't provide the code to convert the code
	// to RGBAImage, we'll need to convert it to DDS first and use the DDS decompression routines to get
	// the raw pixels (theoretically, we could refactor the DDS functions to be generalized, but for now,
	// this seems much more maintainable...). This code is cribbed from the example code in
	// the crunch repo: https://github.com/DaemonEngine/crunch/blob/master/example2/example2.cpp
	// Compute the face's width, height, number of DXT blocks per row/col, etc.
	// This is not a proper DDS conversion; it's only enough to get the ddslib decompressor to be happy.
	const crn_uint32 blocks_x = std::max( 1U, ( ti.m_width + 3 ) >> 2 );
	const crn_uint32 blocks_y = std::max( 1U, ( ti.m_height + 3 ) >> 2 );
	const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block( ti.m_format );
	const crn_uint32 total_face_size = row_pitch * blocks_y;
	const crn_uint32 ddsSize = sizeof( ddsBuffer_t ) + total_face_size;
	std::unique_ptr<char[]> ddsBuffer( new char[ddsSize]{} );


	ddsBuffer_t* dds = reinterpret_cast<ddsBuffer_t*>( ddsBuffer.get() );

	memcpy( &dds->magic, "DDS ", sizeof( dds->magic ) );
	dds->size = LittleLong( 124 );  // Size of the DDS header.
	dds->height = LittleLong( ti.m_height );
	dds->width = LittleLong( ti.m_width );
	dds->mipMapCount = LittleLong( 1 );

	dds->pixelFormat.size = LittleLong( sizeof( ddsPixelFormat_t ) );

	crn_format fundamental_fmt = crnd::crnd_get_fundamental_dxt_format( ti.m_format );
	dds->pixelFormat.fourCC = LittleLong( crnd::crnd_crn_format_to_fourcc( fundamental_fmt ) );
	if ( fundamental_fmt != ti.m_format ) {
		// It's a funky swizzled DXTn format - write its FOURCC to RGBBitCount.
		dds->pixelFormat.rgbBitCount = LittleLong( crnd::crnd_crn_format_to_fourcc( ti.m_format ) );
	}
	char* imageArray[1];
	imageArray[0] = reinterpret_cast<char*>( &dds->data );
	if ( !crnd::crnd_unpack_level( ctx, reinterpret_cast<void**>( &imageArray ), total_face_size, row_pitch, 0 ) ) {
		return false;
	}

	if ( DDSDecompress( dds, reinterpret_cast<unsigned char*>( outBuf ) ) == -1) {
		return false;
	}
	return true;
}
