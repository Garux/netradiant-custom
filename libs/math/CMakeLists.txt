add_library(math
        aabb.h
        curve.h
#        expression.cpp expression.h
        frustum.h
        line.h
        matrix.h
        pi.h
        plane.h
        quaternion.h
        vector.h
        )
set_target_properties(math PROPERTIES LINKER_LANGUAGE CXX)
