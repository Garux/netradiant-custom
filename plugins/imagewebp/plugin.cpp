/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "plugin.h"
#include "debugging/debugging.h"
#include "ifilesystem.h"
#include "iimage.h"

#include "imagelib.h"

// ====== WEBP loader functionality ======

#include "webp/decode.h"

Image* LoadWEBPBuff( unsigned char* buffer, size_t buffer_length ){
	int image_width;
	int image_height;

	if ( !WebPGetInfo( (byte *) buffer, buffer_length, &image_width, &image_height) ){
		globalErrorStream() << "libwebp error: WebPGetInfo: can't get image info\n";
		return 0;
	}
	    
	// allocate the pixel buffer
	RGBAImage* image = new RGBAImage( image_width, image_height );
	int out_stride = image_width  *sizeof(RGBAPixel);
	int out_size =  image_height * out_stride;
	
        if ( !WebPDecodeRGBAInto( (byte *) buffer, buffer_length, image->getRGBAPixels(), out_size, out_stride ) )
        {
                return 0;
        }

	return image;
}

Image* LoadWEBP( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	return LoadWEBPBuff( buffer.buffer, buffer.length );
}


#include "modulesystem/singletonmodule.h"


class ImageDependencies : public GlobalFileSystemModuleRef
{
};

class ImageWEBPAPI
{
_QERPlugImageTable m_imagewebp;
public:
typedef _QERPlugImageTable Type;
STRING_CONSTANT( Name, "webp" );

ImageWEBPAPI(){
	m_imagewebp.loadImage = LoadWEBP;
}
_QERPlugImageTable* getTable(){
	return &m_imagewebp;
}
};

typedef SingletonModule<ImageWEBPAPI, ImageDependencies> ImageWEBPModule;

ImageWEBPModule g_ImageWEBPModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	g_ImageWEBPModule.selfRegister();
}
