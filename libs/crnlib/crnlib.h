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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Sets `x` and `y` to the width and height of the input crn image. Returns false if there is an
// error reading the image.
int GetCRNImageSize( const void *buffer, int length, int *x, int *y );

// Converts a .crn file to RGBA. Stores the pixels in outBuf. Use GetCRNImageSize to get the image
// size to determine how big outBuf should be. The function will return false if the image does not
// fit inside outBuf.
int ConvertCRNtoRGBA( const void *buffer, int length, unsigned int outBufLen, void *outBuf );

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
