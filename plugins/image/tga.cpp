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

#include "tga.h"

#include "ifilesystem.h"
#include "iarchive.h"
#include "idatastream.h"

typedef unsigned char byte;

#include <cstdlib>

#include "generic/bitfield.h"
#include "imagelib.h"
#include "bytestreamutils.h"

// represents x,y origin of tga image being decoded
class Flip00 {}; // no flip
class Flip01 {}; // vertical flip only
class Flip10 {}; // horizontal flip only
class Flip11 {}; // both

template<typename PixelDecoder>
void image_decode( PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip00& ){
	RGBAPixel* end = image.pixels + ( image.height * image.width );
	for ( RGBAPixel* row = end; row != image.pixels; row -= image.width )
	{
		for ( RGBAPixel* pixel = row - image.width; pixel != row; ++pixel )
		{
			decode( istream, *pixel );
		}
	}
}

template<typename PixelDecoder>
void image_decode( PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip01& ){
	RGBAPixel* end = image.pixels + ( image.height * image.width );
	for ( RGBAPixel* row = image.pixels; row != end; row += image.width )
	{
		for ( RGBAPixel* pixel = row; pixel != row + image.width; ++pixel )
		{
			decode( istream, *pixel );
		}
	}
}

template<typename PixelDecoder>
void image_decode( PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip10& ){
	RGBAPixel* end = image.pixels + ( image.height * image.width );
	for ( RGBAPixel* row = end; row != image.pixels; row -= image.width )
	{
		for ( RGBAPixel* pixel = row; pixel != row - image.width; )
		{
			decode( istream, *--pixel );
		}
	}
}

template<typename PixelDecoder>
void image_decode( PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip11& ){
	RGBAPixel* end = image.pixels + ( image.height * image.width );
	for ( RGBAPixel* row = image.pixels; row != end; row += image.width )
	{
		for ( RGBAPixel* pixel = row + image.width; pixel != row; )
		{
			decode( istream, *--pixel );
		}
	}
}

void image_fix_fully_transparent_alpha( RGBAImage& image ){
	const RGBAPixel* end = image.pixels + ( image.height * image.width );
	for( RGBAPixel* pixel = image.pixels; pixel != end; ++pixel )
		if( pixel->alpha != 0 )
			return;
	for( RGBAPixel* pixel = image.pixels; pixel != end; ++pixel )
		pixel->alpha = 0xff;
}

template<std::size_t BYTES>
inline void istream_read_pixel( PointerInputStream& istream, RGBAPixel& pixel );
template<>
inline void istream_read_pixel<1>( PointerInputStream& istream, RGBAPixel& pixel ){
	pixel.red = pixel.green = pixel.blue = istream_read_byte( istream );
	pixel.alpha = 0xff;
}
template<>
inline void istream_read_pixel<3>( PointerInputStream& istream, RGBAPixel& pixel ){
	istream.read( &pixel.blue, 1 );
	istream.read( &pixel.green, 1 );
	istream.read( &pixel.red, 1 );
	pixel.alpha = 0xff;
}
template<>
inline void istream_read_pixel<4>( PointerInputStream& istream, RGBAPixel& pixel ){
	istream.read( &pixel.blue, 1 );
	istream.read( &pixel.green, 1 );
	istream.read( &pixel.red, 1 );
	istream.read( &pixel.alpha, 1 );
}


template<std::size_t BYTES>
inline void istream_read_paletted( PointerInputStream& istream, RGBAPixel& pixel, const byte* colormap );
template<>
inline void istream_read_paletted<3>( PointerInputStream& istream, RGBAPixel& pixel, const byte* colormap ){
	const byte* color = colormap + istream_read_byte( istream ) * 3;
	pixel.blue = *color++;
	pixel.green = *color++;
	pixel.red = *color;
	pixel.alpha = 0xff;
}
template<>
inline void istream_read_paletted<4>( PointerInputStream& istream, RGBAPixel& pixel, const byte* colormap ){
	const byte* color = colormap + istream_read_byte( istream ) * 4;
	pixel.blue = *color++;
	pixel.green = *color++;
	pixel.red = *color++;
	pixel.alpha = *color;
}


template<std::size_t BYTES>
class TargaDecodePalettedPixel
{
	const byte* m_colormap;
public:
	TargaDecodePalettedPixel( const byte* colormap ) : m_colormap( colormap ){
	}
	void operator()( PointerInputStream& istream, RGBAPixel& pixel ) const {
		istream_read_paletted<BYTES>( istream, pixel, m_colormap );
	}
};

template<typename Flip, std::size_t BYTES>
void targa_decode_paletted( PointerInputStream& istream, RGBAImage& image, const Flip& flip, const byte* colormap ){
	TargaDecodePalettedPixel<BYTES> decode( colormap );
	image_decode( istream, decode, image, flip );
}


template<std::size_t BYTES>
class TargaDecodePixel
{
public:
	void operator()( PointerInputStream& istream, RGBAPixel& pixel ) const {
		istream_read_pixel<BYTES>( istream, pixel );
	}
};

template<typename Flip, std::size_t BYTES>
void targa_decode( PointerInputStream& istream, RGBAImage& image, const Flip& flip ){
	TargaDecodePixel<BYTES> decode;
	image_decode( istream, decode, image, flip );
}


typedef byte TargaPacket;
typedef byte TargaPacketSize;

inline void targa_packet_read_istream( TargaPacket& packet, PointerInputStream& istream ){
	istream.read( &packet, 1 );
}

inline bool targa_packet_is_rle( const TargaPacket& packet ){
	return ( packet & 0x80 ) != 0;
}

inline TargaPacketSize targa_packet_size( const TargaPacket& packet ){
	return 1 + ( packet & 0x7f );
}


template<typename PixelReadFunctor>
class TargaDecodePixelRLE
{
	TargaPacketSize m_packetSize;
	RGBAPixel m_pixel;
	TargaPacket m_packet;
	const PixelReadFunctor& m_pixelRead;
public:
	TargaDecodePixelRLE( const PixelReadFunctor& pixelRead ) : m_packetSize( 0 ), m_pixelRead( pixelRead ){
	}
	void operator()( PointerInputStream& istream, RGBAPixel& pixel ){
		if ( m_packetSize == 0 ) {
			targa_packet_read_istream( m_packet, istream );
			m_packetSize = targa_packet_size( m_packet );

			if ( targa_packet_is_rle( m_packet ) ) {
				m_pixelRead( istream, m_pixel );
			}
		}

		if ( targa_packet_is_rle( m_packet ) ) {
			pixel = m_pixel;
		}
		else
		{
			m_pixelRead( istream, pixel );
		}

		--m_packetSize;
	}
};

template<typename Flip, std::size_t BYTES>
void targa_decode_rle( PointerInputStream& istream, RGBAImage& image, const Flip& flip ){
	const TargaDecodePixel<BYTES> pixelRead;
	TargaDecodePixelRLE<TargaDecodePixel<BYTES>> decode( pixelRead );
	image_decode( istream, decode, image, flip );
}

template<typename Flip, std::size_t BYTES>
void targa_decode_paletted_rle( PointerInputStream& istream, RGBAImage& image, const Flip& flip, const byte* colormap ){
	const TargaDecodePalettedPixel<BYTES> pixelRead( colormap );
	TargaDecodePixelRLE<TargaDecodePalettedPixel<BYTES>> decode( pixelRead );
	image_decode( istream, decode, image, flip );
}


struct TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;

	byte *colormap = nullptr;
	void colormap_read( PointerInputStream& istream ){
		const std::size_t size = colormap_size / 8 * colormap_length;
		colormap = new byte[size];
		istream.read( colormap, size );
	}
	~TargaHeader(){
		delete[] colormap;
	}
};

inline void targa_header_read_istream( TargaHeader& targa_header, PointerInputStream& istream ){
	targa_header.id_length = istream_read_byte( istream );
	targa_header.colormap_type = istream_read_byte( istream );
	targa_header.image_type = istream_read_byte( istream );

	targa_header.colormap_index = istream_read_int16_le( istream );
	targa_header.colormap_length = istream_read_int16_le( istream );
	targa_header.colormap_size = istream_read_byte( istream );
	targa_header.x_origin = istream_read_int16_le( istream );
	targa_header.y_origin = istream_read_int16_le( istream );
	targa_header.width = istream_read_int16_le( istream );
	targa_header.height = istream_read_int16_le( istream );
	targa_header.pixel_size = istream_read_byte( istream );
	targa_header.attributes = istream_read_byte( istream );

	if ( targa_header.id_length != 0 ) {
		istream.seek( targa_header.id_length ); // skip TARGA image comment
	}

	if( ( targa_header.image_type == 1 || targa_header.image_type == 9 ) && targa_header.colormap_type == 1 ){
		targa_header.colormap_read( istream );
	}
}

template<typename Flip>
Image* Targa_decodeImageData( const TargaHeader& targa_header, PointerInputStream& istream, const Flip& flip ){
	auto *image = new RGBAImage( targa_header.width, targa_header.height );

	if ( targa_header.image_type == 2 || targa_header.image_type == 3 ) {
		switch ( targa_header.pixel_size )
		{
		case 8:
			targa_decode<Flip, 1>( istream, *image, flip );
			break;
		case 24:
			targa_decode<Flip, 3>( istream, *image, flip );
			break;
		case 32:
			targa_decode<Flip, 4>( istream, *image, flip );
			image_fix_fully_transparent_alpha( *image );
			break;
		default:
			globalErrorStream() << "LoadTGA: illegal pixel_size " << SingleQuoted( targa_header.pixel_size ) << '\n';
			image->release();
			return 0;
		}
	}
	else if ( targa_header.image_type == 10 || targa_header.image_type == 11 ) {
		switch ( targa_header.pixel_size )
		{
		case 8:
			targa_decode_rle<Flip, 1>( istream, *image, flip );
			break;
		case 24:
			targa_decode_rle<Flip, 3>( istream, *image, flip );
			break;
		case 32:
			targa_decode_rle<Flip, 4>( istream, *image, flip );
			image_fix_fully_transparent_alpha( *image );
			break;
		default:
			globalErrorStream() << "LoadTGA: illegal pixel_size " << SingleQuoted( targa_header.pixel_size ) << '\n';
			image->release();
			return 0;
		}
	}
	else if ( targa_header.image_type == 1 ) {
		switch ( targa_header.colormap_size )
		{
		case 24:
			targa_decode_paletted<Flip, 3>( istream, *image, flip, targa_header.colormap );
			break;
		case 32:
			targa_decode_paletted<Flip, 4>( istream, *image, flip, targa_header.colormap );
			image_fix_fully_transparent_alpha( *image );
			break;
		default:
			globalErrorStream() << "LoadTGA: illegal colormap_size " << SingleQuoted( targa_header.colormap_size ) << '\n';
			image->release();
			return 0;
		}
	}
	else if ( targa_header.image_type == 9 ) {
		switch ( targa_header.colormap_size )
		{
		case 24:
			targa_decode_paletted_rle<Flip, 3>( istream, *image, flip, targa_header.colormap );
			break;
		case 32:
			targa_decode_paletted_rle<Flip, 4>( istream, *image, flip, targa_header.colormap );
			image_fix_fully_transparent_alpha( *image );
			break;
		default:
			globalErrorStream() << "LoadTGA: illegal colormap_size " << SingleQuoted( targa_header.colormap_size ) << '\n';
			image->release();
			return 0;
		}
	}

	return image;
}

const unsigned int TGA_FLIP_HORIZONTAL = 0x10;
const unsigned int TGA_FLIP_VERTICAL = 0x20;

Image* LoadTGABuff( const byte* buffer ){
	PointerInputStream istream( buffer );
	TargaHeader targa_header;

	targa_header_read_istream( targa_header, istream );

	if ( targa_header.image_type != 1 &&
	     targa_header.image_type != 2 &&
	     targa_header.image_type != 3 &&
	     targa_header.image_type != 9 &&
	     targa_header.image_type != 10 &&
	     targa_header.image_type != 11 ) {
		globalErrorStream() << "LoadTGA: TGA type " << targa_header.image_type << " not supported\n";
		globalErrorStream() << "LoadTGA: Only uncompressed types: 1 (paletted), 2 (RGB), 3 (gray) and compressed: 9 (paletted), 10 (RGB), 11 (gray) of TGA images supported\n";
		return 0;
	}

	if ( targa_header.image_type == 1 || targa_header.image_type == 9 ) {
		if( targa_header.colormap_type != 1 ){
			globalErrorStream() << "LoadTGA: only type 1 colormaps are supported\n";
			return 0;
		}
		else if( targa_header.colormap_index != 0 ){
			globalErrorStream() << "LoadTGA: colormap_index not supported\n";
			return 0;
		}
	}

	if ( ( ( targa_header.image_type == 2 || targa_header.image_type == 10 ) && targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) ||
	     ( ( targa_header.image_type == 3 || targa_header.image_type == 11 ) && targa_header.pixel_size != 8 ) ||
	     ( ( targa_header.image_type == 1 || targa_header.image_type == 9 ) && targa_header.pixel_size != 8 ) ) {
		globalErrorStream() << "LoadTGA: Only 32, 24 or 8 bit images supported\n";
		return 0;
	}

	if ( !bitfield_enabled( targa_header.attributes, TGA_FLIP_HORIZONTAL )
	     && !bitfield_enabled( targa_header.attributes, TGA_FLIP_VERTICAL ) ) {
		return Targa_decodeImageData( targa_header, istream, Flip00() );
	}
	if ( !bitfield_enabled( targa_header.attributes, TGA_FLIP_HORIZONTAL )
	     && bitfield_enabled( targa_header.attributes, TGA_FLIP_VERTICAL ) ) {
		return Targa_decodeImageData( targa_header, istream, Flip01() );
	}
	if ( bitfield_enabled( targa_header.attributes, TGA_FLIP_HORIZONTAL )
	     && !bitfield_enabled( targa_header.attributes, TGA_FLIP_VERTICAL ) ) {
		return Targa_decodeImageData( targa_header, istream, Flip10() );
	}
	if ( bitfield_enabled( targa_header.attributes, TGA_FLIP_HORIZONTAL )
	     && bitfield_enabled( targa_header.attributes, TGA_FLIP_VERTICAL ) ) {
		return Targa_decodeImageData( targa_header, istream, Flip11() );
	}

	// unreachable
	return 0;
}

Image* LoadTGA( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	return LoadTGABuff( buffer.buffer );
}
