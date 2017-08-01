/**
 * @file PluginUI.cpp
 * Implements the PluginUI class.
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

#include "PluginUI.h"
#include "SetScaleDialog.h"
#include "GetInfoDialog.h"
#include "GeneralFunctionDialog.h"
#include "MainMenu.h"


/**
 * Default constructor. Instantiate and register the UI elements (main menu
 * and dialogs)
 */
PluginUI::PluginUI()
{
   // Instantiate and register the Set S/T Scale dialog. We need a non-generic
   // handle on this one too, because it will be used as input to the Get Info
   // dialog constructor below.
   SmartPointer<SetScaleDialog> setScaleDialog(
      new SetScaleDialog("SetScale"));
   SmartPointer<GenericDialog> setScaleDialogGeneric(setScaleDialog);
   RegisterDialog(setScaleDialogGeneric);
   // Instantiate and register the Get Info dialog. Constructor needs a handle
   // on the Set S/T Scale dialog (since it may need to send texture info to
   // it).
   SmartPointer<GenericDialog> getInfoDialogGeneric(
      new GetInfoDialog("GetInfo", setScaleDialog));
   RegisterDialog(getInfoDialogGeneric);
   // Instantiate and register the General Function dialog.
   SmartPointer<GenericDialog> genFuncDialogGeneric(
      new GeneralFunctionDialog("GeneralFunction"));
   RegisterDialog(genFuncDialogGeneric);
   // Instantiate and register the main menu. Constructor needs generic
   // handles on all dialogs (since it may need to raise them).
   SmartPointer<GenericMainMenu> mainMenuGeneric(
      new ::MainMenu(setScaleDialogGeneric,
                     getInfoDialogGeneric,
                     genFuncDialogGeneric));
   RegisterMainMenu(mainMenuGeneric);
}

/**
 * Destructor.
 */
PluginUI::~PluginUI()
{
}

/**
 * Get the singleton instance of the UI manager. Note that callers should
 * almost certainly invoke the UIInstance global function instead of using
 * this method.
 *
 * @return Handle to the UI manager instance.
 */
PluginUI&
PluginUI::Instance()
{
   static PluginUI singleton;
   return singleton;
}

/**
 * Get the singleton instance of the UI manager.
 *
 * @return Reference to a singleton that implements GenericPluginUI.
 */
GenericPluginUI& UIInstance()
{
   return PluginUI::Instance();
}
