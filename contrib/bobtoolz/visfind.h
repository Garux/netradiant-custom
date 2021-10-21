
#pragma once

#include <list>
#include "mathlib.h"

class DMetaSurf;
typedef std::list<DMetaSurf*> DMetaSurfaces;

DMetaSurfaces* BuildTrace( char* filename, vec3_t v_origin );
