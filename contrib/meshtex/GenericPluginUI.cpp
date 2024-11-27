/**
 * @file GenericPluginUI.cpp
 * Implements the GenericPluginUI class.
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

#include "GenericPluginUI.h"


/**
 * Default constructor. Initialize object state; the initial callback ID to
 * use is 1 because 0 is reserved for "invalid". Note that as this is a
 * protected method, GenericPluginUI objects cannot be created directly; only
 * subclasses of GenericPluginUI can be created.
 */
GenericPluginUI::GenericPluginUI() :
   _window(NULL),
   _mainMenu(NULL)
{
}

/**
 * Virtual destructor. Remove references to UI elements (which should
 * trigger garbage collection).
 */
GenericPluginUI::~GenericPluginUI()
{
   // Remove any reference to the menu object.
   if (_mainMenu != NULL)
   {
      delete _mainMenu;
   }
   // The _dialogMap will also be deleted by the normal destructor operation,
   // which will clear any references we hold on dialog windows.
}

/**
 * Register the command menu.
 *
 * @param mainMenu The command menu.
 */
void
GenericPluginUI::RegisterMainMenu(SmartPointer<GenericMainMenu>& mainMenu)
{
   // Remember the menu object, and hold a reference on it so it won't be
   // garbage-collected while this object exists.
   _mainMenu = new SmartPointer<GenericMainMenu>(mainMenu);
}

/**
 * Register a dialog window.
 *
 * @param dialog The dialog.
 */
void
GenericPluginUI::RegisterDialog(SmartPointer<GenericDialog>& dialog)
{
   // Remember the association between key and dialog, and hold a reference
   // on it so it won't be garbage-collected while this object exists.
   _dialogMap.insert(std::make_pair(dialog->GetKey(), dialog));
}

/**
 * Specify the main application window.
 *
 * @param window The main window.
 */
void
GenericPluginUI::SetWindow(QWidget *window)
{
   // Remember it.
   _window = window;
   // Set it as the parent for every dialog window.
   for ( const auto& [name, ptr] : _dialogMap )
   {
      if (ptr.get() != NULL)
      {
         ptr->SetWindow(window);
      }
   }
}

/**
 * Get the command menu.
 *
 * @return The command menu, or NULL if none has been registered.
 */
GenericMainMenu *
GenericPluginUI::MainMenu()
{
   if (_mainMenu == NULL)
   {
      return NULL;
   }
   return _mainMenu->get();
}

/**
 * Get the dialog identified by the specified key.
 *
 * @param key The key.
 *
 * @return The dialog, or NULL if none found for that key.
 */
GenericDialog *
GenericPluginUI::Dialog(const std::string& key)
{
   DialogMap::const_iterator dialogMapIter = _dialogMap.find(key);
   if (dialogMapIter == _dialogMap.end())
   {
      return NULL;
   }
   return dialogMapIter->second;
}

/**
 * Declare that the controllee widget should be inactive when the
 * controller widget is inactive. The controllee will be active only
 * when all of its controllers allow it to be so.
 *
 * @param controller The controller widget.
 * @param controllee The controllee widget.
 */
void
GenericPluginUI::RegisterWidgetDependence(QAbstractButton *controller,
                                          QWidget *controllee)
{
   // Make sure we get a callback when the controller is toggled.
   if (!_widgetControlMap.contains(controller))
   {
      QObject::connect( controller, &QAbstractButton::toggled, [this, controller]( bool checked ){
         WidgetControlCallback( controller );
      } );
   }
   // Save the association.
   _widgetControlMap[controller].push_back(controllee);
   _widgetControlledByMap[controllee].push_back(controller);
}

/**
 * Declare that the controllee widget should be inactive when the
 * controller widget is active. The controllee will be active only
 * when all of its controllers allow it to be so.
 *
 * @param controller The controller widget.
 * @param controllee The controllee widget.
 */
void
GenericPluginUI::RegisterWidgetAntiDependence(QAbstractButton *controller,
                                              QWidget *controllee)
{
   // Make sure we get a callback when the controller is toggled.
   if (!_widgetControlMap.contains(controller))
   {
      QObject::connect( controller, &QAbstractButton::toggled, [this, controller]( bool checked ){
         WidgetControlCallback( controller );
      } );
   }
   // Save the association.
   _widgetControlMap[controller].push_back(controllee);
   _widgetAntiControlledByMap[controllee].push_back(controller);
}

/**
 * Manage the state of controllee widgets when a controller widget is clicked.
 *
 * @param widget     The controller widget.
 * @param callbackID Unique numerical ID for the callback.
 */
void
GenericPluginUI::WidgetControlCallback( QAbstractButton *button )
{
   // Iterate over all controllees registered for this widget.
   for ( QWidget *controllee : _widgetControlMap[button] )
   {
      // Start with an assumption that the controllee widget will be active.
      bool sensitive = true;
      // Look for a dependence on any widget.
      for ( QAbstractButton *controller : _widgetControlledByMap[controllee] )
      {
         // Dependence found; honor it.
         if ( !controller->isChecked() )
         {
            sensitive = false;
            break;
         }
      }
      // Look for an anti-dependence on any widget.
      for ( QAbstractButton *controller : _widgetAntiControlledByMap[controllee] )
      {
         // Anti-dependence found; honor it.
         if ( controller->isChecked() )
         {
            sensitive = false;
            break;
         }
      }
      // Set the active state of the controllee appropriately.
      controllee->setEnabled( sensitive);
   }
}

/**
 * Generate an error dialog.
 *
 * @param title   The dialog title.
 * @param message The error message.
 */
void
GenericPluginUI::ErrorReportDialog(const char *title,
                                   const char *message)
{
   // Pass this operation to Radiant.
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, EMessageBoxType::Error, 0);
}

/**
 * Generate a warning dialog.
 *
 * @param title   The dialog title.
 * @param message The warning message.
 */
void
GenericPluginUI::WarningReportDialog(const char *title,
                                     const char *message)
{
   // Pass this operation to Radiant.
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, EMessageBoxType::Warning, 0);
}

/**
 * Generate an info dialog.
 *
 * @param title   The dialog title.
 * @param message The info message.
 */
void
GenericPluginUI::InfoReportDialog(const char *title,
                                  const char *message)
{
   // Pass this operation to Radiant.
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, EMessageBoxType::Info, 0);
}
