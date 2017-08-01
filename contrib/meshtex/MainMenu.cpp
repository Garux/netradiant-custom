/**
 * @file MainMenu.cpp
 * Implements the MainMenu class.
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

#include "MainMenu.h"
#include "PluginUI.h"
#include "PluginUIMessages.h"

#include "scenelib.h"
#include "iundo.h"


/**
 * Constructor.
 *
 * @param visitorFunctor The functor for the mesh preset function.
 * @param axes           The texture axes to affect.
 */
MainMenu::PresetFuncVisitor::PresetFuncVisitor(
   const VisitorFunctor& visitorFunctor,
   MeshEntity::TextureAxisSelection axes) :
   _visitorFunctor(visitorFunctor),
   _axes(axes)
{
}

/**
 * Visitor action; invoke a preset function on a mesh.
 *
 * @param [in,out] meshEntity The mesh entity.
 *
 * @return true.
 */
bool
MainMenu::PresetFuncVisitor::Execute(MeshEntity& meshEntity) const
{
   (meshEntity.*_visitorFunctor)(_axes);
   return true;
}

/**
 * Constructor. Instantiate the command callbacks and construct the command
 * list.
 *
 * @param setScaleDialog Reference-counted handle on the Set S/T Scale dialog.
 * @param getInfoDialog  Reference-counted handle on the Get Info dialog.
 * @param genFuncDialog  Reference-counted handle on the General Function
 *                       dialog.
 */
MainMenu::MainMenu(SmartPointer<GenericDialog>& setScaleDialog,
                   SmartPointer<GenericDialog>& getInfoDialog,
                   SmartPointer<GenericDialog>& genFuncDialog) :
   _commandMeshVisitor(*this)
{
   // Like _commandMeshVisitor, the callbacks for the Help and About commands
   // also specify functions on this object. 
   const CommandCallbackMethod
      <MainMenu, &MainMenu::CommandHelp> commandHelp(*this);
   const CommandCallbackMethod
      <MainMenu, &MainMenu::CommandAbout> commandAbout(*this);

   // Start constructing the command list.
   BeginEntries();
   // First two commands summon dialogs.
   AddDialogShowEntry("Set S/T Scale...", "SetScale", setScaleDialog);
   AddDialogShowEntry("Get Info...", "GetInfo", getInfoDialog);
   AddSeparator();
   // Next command summons a dialog.
   AddDialogShowEntry("General Function...", "GeneralFunction", genFuncDialog);
   AddSeparator();
   // The next few groups of commands are all similar "preset function"
   // commands. They will each trigger the CommandMeshVisitor callback when
   // selected in the menu. CommandMeshVisitor needs a visitor object to apply
   // to the selected meshes, which we are specifying here. 
   AddMeshVisitorEntry("S&T Align Auto", "STMinMaxAlignAuto",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignAutoScale,
                                                MeshEntity::ALL_TEX_AXES)));
   AddMeshVisitorEntry("S Align Auto", "SMinMaxAlignAuto",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignAutoScale,
                                                MeshEntity::S_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("T Align Auto", "TMinMaxAlignAuto",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignAutoScale,
                                                MeshEntity::T_TEX_AXIS_ONLY)));
   AddSeparator();
   AddMeshVisitorEntry("S Align Stretch", "SMinMaxAlignStretch",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignStretch,
                                                MeshEntity::S_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("S Align Shrink", "SMinMaxAlignShrink",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignShrink,
                                                MeshEntity::S_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("T Align Stretch", "TMinMaxAlignStretch",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinMaxAlignStretch,
                                                MeshEntity::T_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("T Align Shrink", "TMinMaxAlignShrink",
                   SmartPointer<MeshVisitor>(
                      new PresetFuncVisitor(&MeshEntity::MinMaxAlignShrink,
                                            MeshEntity::T_TEX_AXIS_ONLY)));
   AddSeparator();
   AddMeshVisitorEntry("S Min Align", "SMinAlign",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinAlign,
                                                MeshEntity::S_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("S Max Align", "SMaxAlign",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MaxAlign,
                                                MeshEntity::S_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("T Min Align", "TMinAlign",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MinAlign,
                                                MeshEntity::T_TEX_AXIS_ONLY)));
   AddMeshVisitorEntry("T Max Align", "TMaxAlign",
                       SmartPointer<MeshVisitor>(
                          new PresetFuncVisitor(&MeshEntity::MaxAlign,
                                                MeshEntity::T_TEX_AXIS_ONLY)));
   AddSeparator();
   // These commands each invoke a unique callback when selected.
   AddEntry("Help...", "Help", commandHelp);
   AddEntry("About...", "About", commandAbout);
   // Done!
   EndEntries();
}

/**
 * Destructor.
 */
MainMenu::~MainMenu()
{
}

/**
 * Callback common to all of the commands that trigger a processing by mesh
 * visitor when selected. This callback does its own internal dispatch to
 * distinctly handle the various commands.
 *
 * @param commandString The command token.
 */
void
MainMenu::CommandMeshVisitor(const std::string& commandString)
{
   // Look up the registered mesh visitor for this command.
   VisitorMap::const_iterator visitorMapIter = _visitorMap.find(commandString);
   MeshVisitor *meshVisitor;
   if (visitorMapIter == _visitorMap.end() ||
       (meshVisitor = visitorMapIter->second) == NULL)
   {
      // That's odd, there isn't one. Bail out.
      std::string message(commandString + ": " + DIALOG_INTERNAL_ERROR);
      GenericPluginUI::ErrorReportDialog(DIALOG_ERROR_TITLE, message.c_str());
      return;
   }
   // Let Radiant know the name of the operation responsible for the changes
   // that are about to happen.
   UndoableCommand undo(commandString.c_str());
   // Apply the visitor to every selected mesh.
   meshVisitor->ResetVisitedCount();
   GlobalSelectionSystem().foreachSelected(*meshVisitor);
   if (meshVisitor->GetVisitedCount() == 0)
   {
      // Warn if there weren't any meshes selected (so nothing happened). 
      GenericPluginUI::WarningReportDialog(DIALOG_WARNING_TITLE,
                                           DIALOG_NOMESHES_MSG);
   }
}

/**
 * Callback triggered when the Help menu entry is selected.
 *
 * @param commandString The command token.
 */
void
MainMenu::CommandHelp(const std::string& commandString)
{
   // Pop up a hopefully somewhat helpful message dialog.
   GenericPluginUI::InfoReportDialog(DIALOG_HELP_TITLE,
                                     DIALOG_HELP_MSG);
}

/**
 * Callback triggered when the About menu entry is selected.
 *
 * @param commandString The command token.
 */
void
MainMenu::CommandAbout(const std::string& commandString)
{
   // Pop up a message dialog that describes the plugin.
   GenericPluginUI::InfoReportDialog(DIALOG_ABOUT_TITLE,
                                     DIALOG_ABOUT_MSG);
}

/**
 * Register a mesh visitor to be used to implement a specified command.
 *
 * @param commandLabel The command label.
 * @param command      The command token.
 * @param visitor      The mesh visitor.
 */
void
MainMenu::AddMeshVisitorEntry(const char *commandLabel,
                              const char *command,
                              const SmartPointer<MeshVisitor>& visitor)
{
   // Add to the command list, and indicate that CommandMeshVisitor is the
   // callback for this command. Then save the association between command and
   // visitor, for CommandMeshVisitor to reference.
   _visitorMap.insert(
      std::make_pair(AddEntry(commandLabel, command, _commandMeshVisitor),
                     visitor));
}