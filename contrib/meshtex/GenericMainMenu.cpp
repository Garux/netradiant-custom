/**
 * @file GenericMainMenu.cpp
 * Implements the GenericMainMenu class.
 * @ingroup generic-ui
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

#include "GenericMainMenu.h"
#include "GenericPluginUI.h"
#include "GenericPluginUIMessages.h"
#include "PluginProperties.h"


/**
 * Default constructor. Note that as this is a protected method,
 * GenericMainMenu objects cannot be created directly; only subclasses of
 * GenericMainMenu can be created.
 */
GenericMainMenu::GenericMainMenu()
{
}

/**
 * Virtual destructor.
 */
GenericMainMenu::~GenericMainMenu()
{
}

/**
 * Invoke the handler for the given command token.
 *
 * @param command The command token.
 */
void
GenericMainMenu::Dispatch(const char *command)
{
   // Convert token to string.
   std::string commandString(command);
   // Invoke the function from the dispatch map that corresponds to the command.
   // The command key should always be in the map, since the set of commands
   // advertised to Radiant is the same as the set used to make the map.
#if defined(_DEBUG)
   ASSERT_MESSAGE(_dispatchMap.find(commandString) != _dispatchMap.end(),
                  "dispatched plugin command unknown");
#endif
   _dispatchMap[commandString](commandString);
}

/**
 * Handle a command that summons a dialog window.
 *
 * @param commandString The command token.
 */
void
GenericMainMenu::CommandDialogShow(const std::string& commandString)
{
   // Look up the dialog window for this command.
   DialogMap::const_iterator dialogMapIter = _dialogMap.find(commandString);
   // Seems somewhat more plausible that we could hit an error condition here
   // where there is no associated dialog, so handle that more gracefully
   // than an assert.
   GenericDialog *dialog;
   if (dialogMapIter == _dialogMap.end() ||
       (dialog = dialogMapIter->second) == NULL)
   {
      std::string message(commandString + ": " + DIALOG_INTERNAL_ERROR);
      GenericPluginUI::ErrorReportDialog(DIALOG_ERROR_TITLE, message.c_str());
      return;
   }
   // If we have the dialog though, let's summon it.
   dialog->Show(commandString);
}

/**
 * Get the command list for the plugin menu, as a semicolon-separated string
 * of tokens representing each command.
 *
 * @return The command list string.
 */
const std::string&
GenericMainMenu::GetCommandList() const
{
   return _menuCommands;
}

/**
 * Get the command label list for the plugin menu, as a semicolon-separated
 * string of labels to appear in the menu.
 *
 * @return The command label list string.
 */
const std::string&
GenericMainMenu::GetCommandLabelList() const
{
   return _menuCommandLabels;
}

/**
 * Invoked before beginning construction of the command list (by subsequent
 * Add* invocations). In this base class, BeginEntries does nothing.
 */
void
GenericMainMenu::BeginEntries()
{
}

/**
 * Append a command-group separator to the command list. This should be done
 * between an invocation of BeginEntries and EndEntries. (And presumably in
 * the neighborhood of invocations of AddEntry and/or AddDialogShowEntry.)
 */
void
GenericMainMenu::AddSeparator()
{
   // Our internal command and command-label strings actually grow backwards,
   // so prepend the separator to them.
   static std::string separatorString(MENU_SEPARATOR);
   _menuCommandLabels = separatorString + ";" + _menuCommandLabels;
   _menuCommands = separatorString + ";" + _menuCommands;
}

/**
 * Append a command to the command list that will trigger a callback function
 * when the menu entry is selected. Invoking AddEntry should be done between
 * an invocation of BeginEntries and EndEntries.
 *
 * @param commandLabel    The command label.
 * @param command         The command token, unique for this plugin.
 *                        Emptystring is interpreted as: re-use the label
 *                        for the token.
 * @param commandCallback The command callback function.
 *
 * @return The globally-unique command token: plugin name + "." + command.
 */
std::string
GenericMainMenu::AddEntry(const char *commandLabel,
                          const char *command,
                          const CommandCallback& commandCallback)
{
   // Our internal command-label string actually grows backwards, so prepend the
   // command label to it.
   std::string commandLabelString(commandLabel);
   _menuCommandLabels = commandLabelString + ";" + _menuCommandLabels;
   // Use the label for the token if no token specified.
   std::string commandString(command);
   if (commandString.empty())
   {
      commandString = commandLabelString;
   }
   // Add the plugin name in front of the command token to globally uniquify it.
   commandString = std::string(PLUGIN_NAME) + "." + commandString;
   // Our internal command string actually grows backwards, so prepend the
   // command token to it.
   _menuCommands = commandString + ";" + _menuCommands;
   // Save the association between the globally-unique token and callback.
   _dispatchMap[commandString] = commandCallback;
   // Return the globally-unique command token.
   return commandString;
}

/**
 * Append a command to the command list that will summon a dialog when the
 * menu entry is selected. Invoking AddDialogShowEntry should be done between
 * an invocation of BeginEntries and EndEntries.
 *
 * @param commandLabel The command label.
 * @param command      The command token, unique for this plugin. Emptystring
 *                     is interpreted as: re-use the label for the token.
 * @param dialog       The dialog this command should summon.
 */
void
GenericMainMenu::AddDialogShowEntry(const char *commandLabel,
                                    const char *command,
                                    const SmartPointer<GenericDialog>& dialog)
{
   // Create a new command callback specifically for summoning that dialog.
   const CommandCallbackMethod
      <GenericMainMenu, &GenericMainMenu::CommandDialogShow> commandDialogShow(*this);
   // Register the command and its callback via AddEntry, and save the
   // association between the globally-unique token and dialog.
   _dialogMap.insert(
      std::make_pair(AddEntry(commandLabel, command, commandDialogShow),
                     dialog));
}

/**
 * Invoked after ending construction of the command list. In this base class,
 * EndEntries only removes spurious semicolons left behind by the way we
 * constructed the lists.
 */
void
GenericMainMenu::EndEntries()
{
   if (_menuCommandLabels.length() > 0)
   {
      _menuCommandLabels.resize(_menuCommandLabels.length() - 1);
      _menuCommands.resize(_menuCommands.length() - 1);
   }
}
