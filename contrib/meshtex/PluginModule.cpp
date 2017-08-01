/**
 * @file PluginModule.cpp
 * Implements the PluginModule class.
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

#include "PluginModule.h"
#include "GenericPluginUI.h"


/**
 * Plugin function table.
 */
_QERPluginTable PluginModule::_pluginAPI;

/**
 * Default constructor. Initialize the function table.
 */
PluginModule::PluginModule()
{
   _pluginAPI.m_pfnQERPlug_Init = &QERPluginInit;
   _pluginAPI.m_pfnQERPlug_GetName = &QERPluginGetName;
   _pluginAPI.m_pfnQERPlug_GetCommandList = &QERPluginGetCommandList;
   _pluginAPI.m_pfnQERPlug_GetCommandTitleList = &QERPluginGetCommandTitleList;
   _pluginAPI.m_pfnQERPlug_Dispatch = &QERPluginDispatch;
}

/**
 * Destructor.
 */
PluginModule::~PluginModule()
{
}

/**
 * Fetch a pointer to the function table.
 */
_QERPluginTable *
PluginModule::getTable()
{
   return &_pluginAPI;
}

/**
 * Initialize plugin.
 *
 * @param hApp        Dummy arg; ignored.
 * @param pMainWidget Main window widget.
 *
 * @return The plugin name.
 */
const char *
PluginModule::QERPluginInit(void *hApp,
                            void *pMainWidget)
{
   // Inform the UI of the main app window.
   UIInstance().SetWindow((GtkWidget *)pMainWidget);
   // Return the plugin name.
   return PLUGIN_NAME;
}

/**
 * Get the plugin's name.
 *
 * @return The plugin name.
 */
const char *
PluginModule::QERPluginGetName()
{
   // Return the plugin name.
   return PLUGIN_NAME;
}

/**
 * Get the command list for the plugin menu, as a semicolon-separated string
 * of tokens representing each command.
 *
 * @return The command list string.
 */
const char *
PluginModule::QERPluginGetCommandList()
{
   // Bail out if the plugin menu doesn't exist.
   if (UIInstance().MainMenu() == NULL)
   {
      return "";
   }
   // Get the command list from the menu.
   return UIInstance().MainMenu()->GetCommandList().c_str();
}

/**
 * Get the command label list for the plugin menu, as a semicolon-separated
 * string of labels to appear in the menu.
 *
 * @return The command label list string.
 */
const char *
PluginModule::QERPluginGetCommandTitleList()
{
   // Bail out if the plugin menu doesn't exist.
   if (UIInstance().MainMenu() == NULL)
   {
      return "";
   }
   // Get the command label list from the menu.
   return UIInstance().MainMenu()->GetCommandLabelList().c_str();
}

/**
 * Invoke a plugin command.
 *
 * @param command      The command token.
 * @param vMin         3-element float vector definining min corner of
 *                     selection.
 * @param vMax         3-element float vector definining max corner of
 *                     selection.
 * @param bSingleBrush Dummy arg; ignored.
 */
void
PluginModule::QERPluginDispatch(const char *command,
                                float *vMin,
                                float *vMax,
                                bool bSingleBrush)
{
   // Bail out if the plugin menu doesn't exist.
   if (UIInstance().MainMenu() == NULL)
   {
      // XXX This shouldn't happen; might as well drop an ASSERT or error
      // message in here. First make sure there's no odd Radiant-exiting
      // corner case race that could trigger it though.
      return;
   }
   // Send the command dispatch to the menu.
   // XXX For my particular use case I don't need vMin or vMax, but for
   // generality's sake those values should be passed along here, and then I
   // can drop them when the flow gets to MeshTex-specific code... that will
   // require changes for several types/signatures though.
   UIInstance().MainMenu()->Dispatch(command);
}
