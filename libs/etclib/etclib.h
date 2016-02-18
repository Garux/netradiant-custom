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

#ifndef INCLUDED_ETCLIB_H
#define INCLUDED_ETCLIB_H

#include "bytebool.h"

#ifdef __cplusplus
extern "C"
{
#endif

void ETC_DecodeETC1Block( const byte* in, byte* out, qboolean outRGBA );

#ifdef __cplusplus
}
#endif

#endif
