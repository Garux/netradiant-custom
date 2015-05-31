/*
   Copyright (C) 2015, SiPlus, Chasseur de bots.
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

#include "ktx.h"

#include <string.h>

#include "bytestreamutils.h"
#include "etclib.h"
#include "ifilesystem.h"
#include "imagelib.h"


#define KTX_TYPE_UNSIGNED_BYTE				0x1401
#define KTX_TYPE_UNSIGNED_SHORT_4_4_4_4		0x8033
#define KTX_TYPE_UNSIGNED_SHORT_5_5_5_1		0x8034
#define KTX_TYPE_UNSIGNED_SHORT_5_6_5		0x8363

#define KTX_FORMAT_ALPHA					0x1906
#define KTX_FORMAT_RGB						0x1907
#define KTX_FORMAT_RGBA						0x1908
#define KTX_FORMAT_LUMINANCE				0x1909
#define KTX_FORMAT_LUMINANCE_ALPHA			0x190A
#define KTX_FORMAT_BGR						0x80E0
#define KTX_FORMAT_BGRA						0x80E1

#define KTX_FORMAT_ETC1_RGB8				0x8D64

class KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ) = 0;
	virtual unsigned int GetPixelSize() = 0;
};

class KTX_Decoder_A8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		out[0] = out[1] = out[2] = 0;
		out[3] = istream_read_byte( istream );
	}
	virtual unsigned int GetPixelSize(){
		return 1;
	}
};

class KTX_Decoder_RGB8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		istream.read( out, 3 );
		out[3] = 255;
	}
	virtual unsigned int GetPixelSize(){
		return 3;
	}
};

class KTX_Decoder_RGBA8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		istream.read( out, 4 );
	}
	virtual unsigned int GetPixelSize(){
		return 4;
	}
};

class KTX_Decoder_L8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		byte l = istream_read_byte( istream );
		out[0] = out[1] = out[2] = l;
		out[3] = 255;
	}
	virtual unsigned int GetPixelSize(){
		return 1;
	}
};

class KTX_Decoder_LA8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		byte la[2];
		istream.read( la, 2 );
		out[0] = out[1] = out[2] = la[0];
		out[3] = la[1];
	}
	virtual unsigned int GetPixelSize(){
		return 2;
	}
};

class KTX_Decoder_BGR8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		byte bgr[3];
		istream.read( bgr, 3 );
		out[0] = bgr[2];
		out[1] = bgr[1];
		out[2] = bgr[0];
		out[3] = 255;
	}
	virtual unsigned int GetPixelSize(){
		return 3;
	}
};

class KTX_Decoder_BGRA8 : public KTX_Decoder
{
public:
	virtual void Decode( PointerInputStream& istream, byte* out ){
		byte bgra[4];
		istream.read( bgra, 4 );
		out[0] = bgra[2];
		out[1] = bgra[1];
		out[2] = bgra[0];
		out[3] = bgra[3];
	}
	virtual unsigned int GetPixelSize(){
		return 4;
	}
};

class KTX_Decoder_RGBA4 : public KTX_Decoder
{
protected:
	bool m_bigEndian;
public:
	KTX_Decoder_RGBA4( bool bigEndian ) : m_bigEndian( bigEndian ){}
	virtual void Decode( PointerInputStream& istream, byte* out ){
		uint16_t rgba;
		if ( m_bigEndian ) {
			rgba = istream_read_uint16_be( istream );
		}
		else {
			rgba = istream_read_uint16_le( istream );
		}
		int r = ( rgba >> 12 ) & 0xf;
		int g = ( rgba >> 8 ) & 0xf;
		int b = ( rgba >> 4 ) & 0xf;
		int a = rgba & 0xf;
		out[0] = ( r << 4 ) | r;
		out[1] = ( g << 4 ) | g;
		out[2] = ( b << 4 ) | b;
		out[3] = ( a << 4 ) | a;
	}
	virtual unsigned int GetPixelSize(){
		return 2;
	}
};

class KTX_Decoder_RGBA5 : public KTX_Decoder
{
protected:
	bool m_bigEndian;
public:
	KTX_Decoder_RGBA5( bool bigEndian ) : m_bigEndian( bigEndian ){}
	virtual void Decode( PointerInputStream& istream, byte* out ){
		uint16_t rgba;
		if ( m_bigEndian ) {
			rgba = istream_read_uint16_be( istream );
		}
		else {
			rgba = istream_read_uint16_le( istream );
		}
		int r = ( rgba >> 11 ) & 0x1f;
		int g = ( rgba >> 6 ) & 0x1f;
		int b = ( rgba >> 1 ) & 0x1f;
		out[0] = ( r << 3 ) | ( r >> 2 );
		out[1] = ( g << 3 ) | ( g >> 2 );
		out[2] = ( b << 3 ) | ( b >> 2 );
		out[3] = ( rgba & 1 ) * 255;
	}
	virtual unsigned int GetPixelSize(){
		return 2;
	}
};

class KTX_Decoder_RGB5 : public KTX_Decoder
{
protected:
	bool m_bigEndian;
public:
	KTX_Decoder_RGB5( bool bigEndian ) : m_bigEndian( bigEndian ){}
	virtual void Decode( PointerInputStream& istream, byte* out ){
		uint16_t rgb;
		if ( m_bigEndian ) {
			rgb = istream_read_uint16_be( istream );
		}
		else {
			rgb = istream_read_uint16_le( istream );
		}
		int r = ( rgb >> 11 ) & 0x1f;
		int g = ( rgb >> 5 ) & 0x3f;
		int b = rgb & 0x1f;
		out[0] = ( r << 3 ) | ( r >> 2 );
		out[1] = ( g << 2 ) | ( g >> 4 );
		out[2] = ( b << 3 ) | ( b >> 2 );
		out[3] = 255;
	}
	virtual unsigned int GetPixelSize(){
		return 2;
	}
};

static void KTX_DecodeETC1( PointerInputStream& istream, Image& image ){
	unsigned int width = image.getWidth(), height = image.getHeight();
	unsigned int stride = width * 4;
	byte* pixbuf = image.getRGBAPixels();
	byte etc[8], rgba[64];

	for ( unsigned int y = 0; y < height; y += 4, pixbuf += stride * 4 )
	{
		unsigned int blockrows = height - y;
		if ( blockrows > 4 ) {
			blockrows = 4;
		}

		byte* p = pixbuf;
		for ( unsigned int x = 0; x < width; x += 4, p += 16 )
		{
			istream.read( etc, 8 );
			ETC_DecodeETC1Block( etc, rgba, qtrue );

			unsigned int blockrowsize = width - x;
			if ( blockrowsize > 4 ) {
				blockrowsize = 4;
			}
			blockrowsize *= 4;
			for ( unsigned int blockrow = 0; blockrow < blockrows; blockrow++ )
			{
				memcpy( p + blockrow * stride, rgba + blockrow * 16, blockrowsize );
			}
		}
	}
}

Image* LoadKTXBuff( PointerInputStream& istream ){
	byte identifier[12];
	istream.read( identifier, 12 );
	if ( memcmp( identifier, "\xABKTX 11\xBB\r\n\x1A\n", 12 ) ) {
		globalErrorStream() << "LoadKTX: Image has the wrong identifier\n";
		return 0;
	}

	bool bigEndian = ( istream_read_uint32_le( istream ) == 0x01020304 );

	unsigned int type;
	if ( bigEndian ) {
		type = istream_read_uint32_be( istream );
	}
	else {
		type = istream_read_uint32_le( istream );
	}

	// For compressed textures, the format is in glInternalFormat.
	// For uncompressed textures, it's in glBaseInternalFormat.
	istream.seek( ( type ? 3 : 2 ) * sizeof( uint32_t ) );
	unsigned int format;
	if ( bigEndian ) {
		format = istream_read_uint32_be( istream );
	}
	else {
		format = istream_read_uint32_le( istream );
	}
	if ( !type ) {
		istream.seek( sizeof( uint32_t ) );
	}

	unsigned int width, height;
	if ( bigEndian ) {
		width = istream_read_uint32_be( istream );
		height = istream_read_uint32_be( istream );
	}
	else {
		width = istream_read_uint32_le( istream );
		height = istream_read_uint32_le( istream );
	}
	if ( !width ) {
		globalErrorStream() << "LoadKTX: Image has zero width\n";
		return 0;
	}
	if ( !height ) {
		height = 1;
	}

	// Skip the key/values and load the first 2D image in the texture.
	// Since KTXorientation is only a hint and has no effect on the texture data and coordinates, it must be ignored.
	istream.seek( 4 * sizeof( uint32_t ) );
	unsigned int bytesOfKeyValueData;
	if ( bigEndian ) {
		bytesOfKeyValueData = istream_read_uint32_be( istream );
	}
	else {
		bytesOfKeyValueData = istream_read_uint32_le( istream );
	}
	istream.seek( bytesOfKeyValueData + sizeof( uint32_t ) );

	RGBAImage* image = new RGBAImage( width, height );

	if ( type ) {
		KTX_Decoder* decoder = NULL;
		switch ( type )
		{
		case KTX_TYPE_UNSIGNED_BYTE:
			switch ( format )
			{
			case KTX_FORMAT_ALPHA:
				decoder = new KTX_Decoder_A8();
				break;
			case KTX_FORMAT_RGB:
				decoder = new KTX_Decoder_RGB8();
				break;
			case KTX_FORMAT_RGBA:
				decoder = new KTX_Decoder_RGBA8();
				break;
			case KTX_FORMAT_LUMINANCE:
				decoder = new KTX_Decoder_L8();
				break;
			case KTX_FORMAT_LUMINANCE_ALPHA:
				decoder = new KTX_Decoder_LA8();
				break;
			case KTX_FORMAT_BGR:
				decoder = new KTX_Decoder_BGR8();
				break;
			case KTX_FORMAT_BGRA:
				decoder = new KTX_Decoder_BGRA8();
				break;
			}
			break;
		case KTX_TYPE_UNSIGNED_SHORT_4_4_4_4:
			if ( format == KTX_FORMAT_RGBA ) {
				decoder = new KTX_Decoder_RGBA4( bigEndian );
			}
			break;
		case KTX_TYPE_UNSIGNED_SHORT_5_5_5_1:
			if ( format == KTX_FORMAT_RGBA ) {
				decoder = new KTX_Decoder_RGBA5( bigEndian );
			}
			break;
		case KTX_TYPE_UNSIGNED_SHORT_5_6_5:
			if ( format == KTX_FORMAT_RGB ) {
				decoder = new KTX_Decoder_RGB5( bigEndian );
			}
			break;
		}

		if ( !decoder ) {
			globalErrorStream() << "LoadKTX: Image has an unsupported pixel type " << type << " or format " << format << "\n";
			image->release();
			return 0;
		}

		unsigned int inRowLength = width * decoder->GetPixelSize();
		unsigned int inPadding = ( ( inRowLength + 3 ) & ~3 ) - inRowLength;
		byte* out = image->getRGBAPixels();
		for ( unsigned int y = 0; y < height; y++ )
		{
			for ( unsigned int x = 0; x < width; x++, out += 4 )
			{
				decoder->Decode( istream, out );
			}

			if ( inPadding ) {
				istream.seek( inPadding );
			}
		}

		delete decoder;
	}
	else {
		switch ( format )
		{
		case KTX_FORMAT_ETC1_RGB8:
			KTX_DecodeETC1( istream, *image );
			break;
		default:
			globalErrorStream() << "LoadKTX: Image has an unsupported compressed format " << format << "\n";
			image->release();
			return 0;
		}
	}

	return image;
}

Image* LoadKTX( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	PointerInputStream istream( buffer.buffer );
	return LoadKTXBuff( istream );
}
