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

find_package(GTK2 REQUIRED)
target_include_directories(gtkutil PRIVATE ${GTK2_INCLUDE_DIRS} ${GTK2_GDK_INCLUDE_DIR} ${GTK2_GLIB_INCLUDE_DIR})

find_package(GtkGLExt REQUIRED)
target_include_directories(gtkutil PRIVATE ${GTKGL_INCLUDE_DIRS})
