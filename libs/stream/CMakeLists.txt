add_library(stream
        filestream.h
        memstream.h
        stringstream.h
        textfilestream.h
        textstream.h
        )
set_target_properties(stream PROPERTIES LINKER_LANGUAGE CXX)
