add_library(gtkutil
        accelerator.cpp accelerator.h
        button.cpp button.h
        clipboard.cpp clipboard.h
        closure.cpp closure.h
        container.cpp container.h
        cursor.cpp cursor.h
        dialog.cpp dialog.h
        entry.cpp entry.h
        filechooser.cpp filechooser.h
        frame.cpp frame.h
        glfont.cpp glfont.h
        glwidget.cpp glwidget.h
        idledraw.cpp idledraw.h
        image.cpp image.h
        menu.cpp menu.h
        messagebox.cpp messagebox.h
        nonmodal.cpp nonmodal.h
        paned.cpp paned.h
        pointer.cpp pointer.h
        toolbar.cpp toolbar.h
        widget.cpp widget.h
        window.cpp window.h
        xorrectangle.cpp xorrectangle.h
        )

target_include_directories(gtkutil PRIVATE uilib)
target_link_libraries(gtkutil PRIVATE uilib)

target_include_directories(gtkutil PRIVATE ${GTK2_PANGO_INCLUDE_DIR})
target_link_libraries(gtkutil PRIVATE ${GTK2_PANGO_LIBRARY})

find_package(GtkGLExt REQUIRED)
target_include_directories(gtkutil PRIVATE ${GTKGL_INCLUDE_DIRS})
target_link_libraries(gtkutil PRIVATE ${GTKGL_LIBRARIES})
