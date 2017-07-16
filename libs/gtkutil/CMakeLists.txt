add_library(gtkutil
        accelerator.cpp accelerator.h
        button.cpp button.h
        clipboard.cpp clipboard.h
        closure.h
        container.h
        cursor.cpp cursor.h
        dialog.cpp dialog.h
        entry.h
        filechooser.cpp filechooser.h
        frame.cpp frame.h
        glfont.cpp glfont.h
        glwidget.cpp glwidget.h
        idledraw.h
        image.cpp image.h
        menu.cpp menu.h
        messagebox.cpp messagebox.h
        nonmodal.h
        paned.cpp paned.h
        pointer.h
        toolbar.cpp toolbar.h
        widget.h
        window.cpp window.h
        xorrectangle.h
        )

target_include_directories(gtkutil PRIVATE uilib)
target_link_libraries(gtkutil PRIVATE uilib)

target_include_directories(gtkutil PRIVATE ${${GTK_NS}_INCLUDE_DIRS})
target_link_libraries(gtkutil PRIVATE ${${GTK_NS}_LIBRARIES})

find_package(GtkGLExt REQUIRED)
target_include_directories(gtkutil PRIVATE ${GTKGL_INCLUDE_DIRS})
target_link_libraries(gtkutil PRIVATE ${GTKGL_LIBRARIES})
