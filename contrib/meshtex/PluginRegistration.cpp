/**
 * @file PluginRegistration.cpp
 * Declares and implements the MeshTexPluginDependencies class, and implements
 * the plugin library's exported function Radiant_RegisterModules.
 * @ingroup meshtex-plugin
 */

/*
 * Copyright 2012 Joel Baxter
 *
 * This file is part of MeshTex.
 *
 * MeshTex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * MeshTex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshTex.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginModule.h"

#include "iundo.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaders.h"
#include "ipatch.h"
#include "modulesystem/singletonmodule.h"


/**
 * Definition (in the declared superclasses) of the Radiant systems that this
 * plugin depends on. GlobalRadiantModule, GlobalUndoModule,
 * GlobalSceneGraphModule, GlobalSelectionModule, GlobalShadersModule, and of
 * course GlobalPatchModule.
 *
 * @ingroup meshtex-plugin.
 */
class MeshTexPluginDependencies :
   public GlobalRadiantModuleRef,
   public GlobalUndoModuleRef,
   public GlobalSceneGraphModuleRef,
   public GlobalSelectionModuleRef,
   public GlobalShadersModuleRef,
   public GlobalPatchModuleRef
{
public:
   /**
    * Default constructor. This function's only responsibility is to pass
    * arguments to superclass constructors.
    */
   MeshTexPluginDependencies() :
      GlobalShadersModuleRef(GlobalRadiant().getRequiredGameDescriptionKeyValue("shaders")),
      GlobalPatchModuleRef(GlobalRadiant().getRequiredGameDescriptionKeyValue("patchtypes"))
   {
   }
};


/**
 * Register as a plugin with Radiant.
 *
 * @param server Radiant's module (library) manager.
 *
 * @relatesalso MeshTexPluginDependencies
 */

 typedef SingletonModule<PluginModule, MeshTexPluginDependencies> SingletonMeshTexPluginModule;

SingletonMeshTexPluginModule g_MeshTexPluginModule;

extern "C" void RADIANT_DLLEXPORT
Radiant_RegisterModules(ModuleServer& server)
{
   // Set ourselves up as a plugin with the necessary dependences.
   //static SingletonModule<PluginModule, MeshTexPluginDependencies> singleton;
   // Alert Radiant that there's at least one module to be managed, and
   // initialize some necessary stuff.
   // XXX As far as I can tell it's not necessary for EVERY library to do this
   // but it doesn't hurt, and this way we ensure it gets done.
   initialiseModule(server);
   // Register this library. Now we're active as a plugin.
   g_MeshTexPluginModule.selfRegister();
}
