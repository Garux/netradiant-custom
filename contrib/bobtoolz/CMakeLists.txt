radiant_plugin(bobtoolz
        dialogs/dialogs-gtk.cpp dialogs/dialogs-gtk.h

        bobToolz.h
        bobToolz-GTK.cpp
        bsploader.cpp bsploader.h
        cportals.cpp CPortals.h
        ctfresource_gtk.h
        DBobView.cpp DBobView.h
        DBrush.cpp DBrush.h
        DEntity.cpp DEntity.h
        DEPair.cpp DEPair.h
        DMap.cpp DMap.h
        DPatch.cpp DPatch.h
        DPlane.cpp DPlane.h
        DPoint.cpp DPoint.h
        DShape.cpp DShape.h
        DTrainDrawer.cpp DTrainDrawer.h
        DTreePlanter.cpp DTreePlanter.h
        DVisDrawer.cpp DVisDrawer.h
        DWinding.cpp DWinding.h
        funchandlers-GTK.cpp
        lists.cpp lists.h
        misc.cpp misc.h
        resource.h
        resource-gtk.h
        ScriptParser.cpp ScriptParser.h
        shapes.cpp shapes.h
        visfind.cpp visfind.h
        )

target_include_directories(bobtoolz PRIVATE uilib)
target_link_libraries(bobtoolz PRIVATE uilib)

target_include_directories(bobtoolz PRIVATE mathlib)
target_link_libraries(bobtoolz PRIVATE mathlib)
