/**
 * @file PluginUI.h
 * Declares the PluginUI class.
 * @ingroup meshtex-ui
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

#if !defined(INCLUDED_PLUGINUI_H)
#define INCLUDED_PLUGINUI_H

#include "GenericPluginUI.h"

/**
 * Subclass of GenericPluginUI that instantiates and registers the UI
 * elements (main menu and dialogs).
 *
 * @ingroup meshtex-ui
 */
class PluginUI : public GenericPluginUI
{
private: // private methods

   /// @name Private to prevent external instantiation
   //@{
   PluginUI();
   ~PluginUI();
   //@}

public: // public methods

   static PluginUI& Instance();
};

#endif // #if !defined(INCLUDED_PLUGINUI_H)