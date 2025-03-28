
#pragma once

#include "DVisDrawer.h"
#include "bsploader.h"
#include "DWinding.h"

void SetupVisView( const char* filename, vec3_t v_origin );
DMetaSurfaces* BuildTrace( int leafnum, bool colorPerSurf );
std::vector<DWinding> BuildLeafWindings( int leafnum );
