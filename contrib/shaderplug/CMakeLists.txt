radiant_plugin(shaderplug
        shaderplug.cpp shaderplug.h
        )

find_package(LibXml2 REQUIRED)
target_include_directories(shaderplug PRIVATE ${LIBXML2_INCLUDE_DIR})

find_package(GTK2 REQUIRED)
target_include_directories(shaderplug PRIVATE ${GTK2_INCLUDE_DIRS})
