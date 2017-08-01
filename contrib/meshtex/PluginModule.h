/**
 * @file PluginModule.h
 * Declares the PluginModule class.
 * @ingroup generic-plugin
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

#if !defined(INCLUDED_PLUGINMODULE_H)
#define INCLUDED_PLUGINMODULE_H

#include "PluginProperties.h"

#include "iplugin.h"
#include "typesystem.h"
#include "qerplugin.h"

/**
 * A singleton object that handles registering as a plugin with Radiant.
 *
 * @ingroup generic-plugin
 */
class PluginModule : public TypeSystemRef
{
public: // public types

   /// @cond DOXYGEN_INTERNAL_RADIANT_ODDITIES_SKIP

   /**
    * Prepare for uses of the token Type in singletonmodule.h; indicate
    * that _QERPluginTable should be substituted there.
    */
   typedef _QERPluginTable Type;

   /**
    * Radiant constant wrapper for the plugin's name string.
    */
   STRING_CONSTANT(Name, PLUGIN_NAME);

   /// @endcond

public: // public methods

   /// @name Lifecycle
   //@{
   PluginModule();
   ~PluginModule();
   //@}
   /// @name Plugin function table interface
   //@{
   _QERPluginTable *getTable();
   //@}
   /// @name Plugin function table members
   //@{
   static const char *QERPluginInit(void *hApp,
                                    void *pMainWidget);
   static const char *QERPluginGetName();
   static const char *QERPluginGetCommandList();
   static const char *QERPluginGetCommandTitleList();
   static void QERPluginDispatch(const char *command,
                                 float *vMin,
                                 float *vMax,
                                 bool bSingleBrush);
   //@}

private: // private member vars

   static _QERPluginTable _pluginAPI;
};

#endif // #if !defined(INCLUDED_PLUGINMODULE_H)
