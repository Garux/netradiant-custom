radiant_plugin(bobtoolz
        #     dialogs/AboutDialog.cpp dialogs/AboutDialog.h
        #     dialogs/AutoCaulkDialog.cpp dialogs/AutoCaulkDialog.h
        #     dialogs/AutoCaulkStartDialog.cpp dialogs/AutoCaulkStartDialog.h
        #     dialogs/BrushCheckDialog.cpp dialogs/BrushCheckDialog.h
        dialogs/dialogs-gtk.cpp dialogs/dialogs-gtk.h
        #     dialogs/DoorDialog.cpp dialogs/DoorDialog.h
        #     dialogs/IntersectDialog.cpp dialogs/IntersectDialog.h
        #     dialogs/IntersectInfoDialog.cpp dialogs/IntersectInfoDialog.h
        #     dialogs/pathplotterdialog.cpp dialogs/pathplotterdialog.h
        #     dialogs/PolygonDialog.cpp dialogs/PolygonDialog.h
        #     dialogs/StairDialog.cpp dialogs/StairDialog.h
        #     dialogs/TextureResetDialog.cpp dialogs/TextureResetDialog.h
        CPortals.cpp CPortals.h
        DBobView.cpp DBobView.h
        DBrush.cpp DBrush.h
        DEPair.cpp DEPair.h
        DEntity.cpp DEntity.h
        DMap.cpp DMap.h
        DPatch.cpp DPatch.h
        DPlane.cpp DPlane.h
        DPoint.cpp DPoint.h
        DShape.cpp DShape.h
        DTrainDrawer.cpp DTrainDrawer.h
        DTreePlanter.cpp DTreePlanter.h
        DVisDrawer.cpp DVisDrawer.h
        DWinding.cpp DWinding.h
        ScriptParser.cpp ScriptParser.h
        bobToolz-GTK.cpp bobToolz.h
        bsploader.cpp bsploader.h
        #     ctfToolz-GTK.cpp
        #     ctfresource_gtk.h
        funchandlers-GTK.cpp
        #     funchandlers-ctf-GTK.cpp
        #     funchandlers.cpp funchandlers.h
        lists.cpp lists.h
        misc.cpp misc.h
        #     resource.h
        #     resource-gtk.h
        shapes.cpp shapes.h
        visfind.cpp visfind.h
        )

target_include_directories(bobtoolz PRIVATE uilib)
target_link_libraries(bobtoolz PRIVATE uilib)

target_include_directories(bobtoolz PRIVATE mathlib)
target_link_libraries(bobtoolz PRIVATE mathlib)
