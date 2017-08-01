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

#include <gtk/gtk.h>

#include "GenericPluginUI.h"


/**
 * Default constructor. Initialize object state; the initial callback ID to
 * use is 1 because 0 is reserved for "invalid". Note that as this is a
 * protected method, GenericPluginUI objects cannot be created directly; only
 * subclasses of GenericPluginUI can be created.
 */
GenericPluginUI::GenericPluginUI() :
   _window(NULL),
   _mainMenu(NULL),
   _callbackID(1),
   _widgetControlCallback(*this)
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
GenericPluginUI::SetWindow(GtkWidget *window)
{
   // Remember it.
   _window = window;
   // Set it as the parent for every dialog window.
   DialogMap::const_iterator dialogMapIter = _dialogMap.begin();
   for (; dialogMapIter != _dialogMap.end(); ++dialogMapIter)
   {
      if (dialogMapIter->second.get() != NULL)
      {
         dialogMapIter->second->SetWindow(window);
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
 * Generic event callback used to invoke the specific callback functions
 * registered with this manager. Those specific callbacks are not themselves
 * registered directly with GTK+ because they may be methods that must be
 * invoked on objects. (Unlike this function, which is a static method.)
 *
 * @param widget The widget generating the event.
 * @param event  The event.
 * @param data   ID of the specific callback registered with this manager.
 *
 * @return The return value from the specific callback.
 */
gint
GenericPluginUI::DialogEventCallbackDispatch(GtkWidget *widget,
                                             GdkEvent* event,
                                             gpointer data)
{
   // Look up the callback ID in our registration map.
   DialogEventCallbackMap::iterator dialogEventCallbackMapIter =
      UIInstance()._dialogEventCallbackMap.find(data);
   if (dialogEventCallbackMapIter == UIInstance()._dialogEventCallbackMap.end())
   {
      // If we didn't find it, nothing to do.
      return TRUE;
   }
   // Otherwise invoke that callback.
   return dialogEventCallbackMapIter->second(widget, event, data);
}

/**
 * Generic signal callback used to invoke the specific callback functions
 * registered with this manager. Those specific callbacks are not themselves
 * registered directly with GTK+ because they may be methods that must be
 * invoked on objects. (Unlike this function, which is a static method.)
 *
 * @param widget The widget generating the signal.
 * @param data   ID of the specific callback registered with this manager.
 */
void
GenericPluginUI::DialogSignalCallbackDispatch(GtkWidget *widget,
                                              gpointer data)
{
   // Look up the callback ID in our registration map.
   DialogSignalCallbackMap::iterator dialogSignalCallbackMapIter =
      UIInstance()._dialogSignalCallbackMap.find(data);
   if (dialogSignalCallbackMapIter == UIInstance()._dialogSignalCallbackMap.end())
   {
      // If we didn't find it, nothing to do.
      return;
   }
   // Otherwise invoke that callback.
   dialogSignalCallbackMapIter->second(widget, data);
}

/**
 * Register a function to be invoked when a widget generates an event.
 *
 * @param widget   The widget generating the event.
 * @param name     The name of the event.
 * @param callback The callback function.
 *
 * @return The unique ID for the registered callback function.
 */
gpointer
GenericPluginUI::RegisterDialogEventCallback(GtkWidget *widget,
                                             const gchar *name,
                                             const DialogEventCallback& callback)
{
   // Get the next callback ID to use.
   gpointer callbackID = GUINT_TO_POINTER(_callbackID++);
   // Make that event on that dialog widget trigger our event dispatch.
   g_signal_connect(G_OBJECT(widget), name,
                    G_CALLBACK(DialogEventCallbackDispatch), callbackID);
   // Save the association between callback ID and function.
   _dialogEventCallbackMap.insert(std::make_pair(callbackID, callback));
   // Return the generated unique callback ID.
   return callbackID;
}

/**
 * Register a function to be invoked when a widget generates a signal.
 *
 * @param widget   The widget generating the signal.
 * @param name     The name of the signal.
 * @param callback The callback function.
 *
 * @return The unique ID for the registered callback function.
 */
gpointer
GenericPluginUI::RegisterDialogSignalCallback(GtkWidget *widget,
                                              const gchar *name,
                                              const DialogSignalCallback& callback)
{
   // Get the next callback ID to use.
   gpointer callbackID = GUINT_TO_POINTER(_callbackID++);
   // Make that signal on that dialog widget trigger our signal dispatch.
   g_signal_connect(G_OBJECT(widget), name,
                    G_CALLBACK(DialogSignalCallbackDispatch), callbackID);
   // Save the association between callback ID and function.
   _dialogSignalCallbackMap.insert(std::make_pair(callbackID, callback));
   // Return the generated unique callback ID.
   return callbackID;
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
GenericPluginUI::RegisterWidgetDependence(GtkWidget *controller,
                                          GtkWidget *controllee)
{
   // Make sure we get a callback when the controller is toggled.
   if (_widgetControlMap.find(controller) == _widgetControlMap.end())
   {
      RegisterDialogSignalCallback(controller, "clicked", _widgetControlCallback);
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
GenericPluginUI::RegisterWidgetAntiDependence(GtkWidget *controller,
                                              GtkWidget *controllee)
{
   // Make sure we get a callback when the controller is toggled.
   if (_widgetControlMap.find(controller) == _widgetControlMap.end())
   {
      RegisterDialogSignalCallback(controller, "clicked", _widgetControlCallback);
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
GenericPluginUI::WidgetControlCallback(GtkWidget *widget,
                                       gpointer callbackID)
{
   // Iterate over all controllees registered for this widget.
   std::vector<GtkWidget *>::iterator controlleeIter =
      _widgetControlMap[widget].begin();
   for (; controlleeIter != _widgetControlMap[widget].end(); ++controlleeIter)
   {
      GtkWidget *controllee = *controlleeIter;
      std::vector<GtkWidget *>::iterator controllerIter;
      // Start with an assumption that the controllee widget will be active.
      bool sensitive = true;
      // Look for a dependence on any widget.
      controllerIter = _widgetControlledByMap[controllee].begin();
      for (; controllerIter != _widgetControlledByMap[controllee].end(); ++controllerIter)
      {
         // Dependence found; honor it.
         if (!(GTK_TOGGLE_BUTTON(*controllerIter)->active))
         {
            sensitive = false;
            break;
         }
      }
      // Look for an anti-dependence on any widget.
      controllerIter = _widgetAntiControlledByMap[controllee].begin();
      for (; controllerIter != _widgetAntiControlledByMap[controllee].end(); ++controllerIter)
      {
         // Anti-dependence found; honor it.
         if (GTK_TOGGLE_BUTTON(*controllerIter)->active)
         {
            sensitive = false;
            break;
         }
      }
      // Set the active state of the controllee appropriately.
      gtk_widget_set_sensitive(controllee, sensitive);
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
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, eMB_OK, eMB_ICONERROR);
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
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, eMB_OK, eMB_ICONWARNING);
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
   GlobalRadiant().m_pfnMessageBox(UIInstance()._window, message, title, eMB_OK, eMB_ICONDEFAULT);
}