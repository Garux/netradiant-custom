/**
 * @file GenericDialog.cpp
 * Implements the GenericDialog class.
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

#include "GenericDialog.h"
#include "GenericPluginUI.h"


/**
 * Constructor. Create the GTK+ widget for the dialog window (not visible
 * yet). Initialize callback IDs to zero (invalid). Note that as this is a
 * protected method, GenericDialog objects cannot be created directly; only
 * subclasses of GenericDialog can be created.
 *
 * @param key Unique key to identify this dialog widget.
 */
GenericDialog::GenericDialog(const std::string& key) :
   _dialog(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
   _window(NULL),
   _key(key),
   _okCallbackID(0),
   _applyCallbackID(0),
   _cancelCallbackID(0)
{
   // XXX Should we go ahead invoke CreateWindowCloseCallback here (and make it
   // private) rather than leaving that to the subclass constructors? Depends on
   // whether it's plausible that a dialog would ever NOT want the usual
   // handling for the close event.
}

/**
 * Virtual destructor. Destroy the GTK+ dialog widget (and therefore its
 * contained widgets) if necessary.
 */
GenericDialog::~GenericDialog()
{
   if (_dialog != NULL)
   {
      gtk_widget_destroy(_dialog);
   }
}

/**
 * Get the unique key that identifies this dialog widget.
 *
 * @return The key.
 */
const std::string&
GenericDialog::GetKey() const
{
   return _key;
}

/**
 * Mark this window widget as a modal dialog for a parent window.
 *
 * @param window The parent window.
 */
void
GenericDialog::SetWindow(GtkWidget *window)
{
   // Remember the parent window.
   _window = window;
   // Mark this widget as a modal dialog for it.
   if (_dialog != NULL)
   {
      gtk_window_set_transient_for(GTK_WINDOW(_dialog), GTK_WINDOW(_window));
   }
}

/**
 * Raise this dialog window to the top of the window stack.
 */
void
GenericDialog::Raise()
{
   // Don't bother if not visible.
   if (GTK_WIDGET_VISIBLE(_dialog))
   {
      gdk_window_raise(_dialog->window);
   }
}

/**
 * Make this dialog window visible and foreground.
 *
 * @param triggerCommand The command token that summoned the dialog.
 */
void
GenericDialog::Show(const std::string& triggerCommand)
{
   // Remember the command token that summoned the dialog; subclasses can make
   // use of this information.
   _triggerCommand = triggerCommand;
   // Show the window if it is currently hidden.
   if (!GTK_WIDGET_VISIBLE(_dialog))
   {
      gtk_widget_show(_dialog);
   }
   // Raise the window to the top of the stack.
   Raise();
}

/**
 * Hide this dialog window.
 */
void
GenericDialog::Hide()
{
   // Bail out if the window is already invisible.
   if (!GTK_WIDGET_VISIBLE(_dialog))
   {
      return;
   }
   // Hide the window.
   gtk_widget_hide(_dialog);
   // If there's a parent window, raise it to the top of the stack.
   if (_window == NULL)
   {
      return;
   }
   gdk_window_raise(_window->window);
}

/**
 * Default handler for Apply logic. This method should be overriden by
 * subclass implementations that need to execute some logic when OK or Apply
 * is clicked. The return value should be the success of that logic. A
 * successful OK will cause the window to be hidden.
 *
 * @return true if the apply logic executed; always the case in this skeleton
 *         implementation.
 */
bool
GenericDialog::Apply()
{
   // Default logic does nothing.
   return true;
}

/**
 * Callback for window-close event.
 *
 * @param widget     This dialog window widget.
 * @param event      The event that instigated the callback.
 * @param callbackID Unique numerical ID for the callback.
 *
 * @return TRUE as defined by glib.
 */
gint
GenericDialog::CloseEventCallback(GtkWidget *widget,
                                  GdkEvent* event,
                                  gpointer callbackID)
{
   // All we need to do is hide the window.
   Hide();
   return TRUE;
}

/**
 * Callback for clicking on OK/Apply/Cancel button.
 *
 * @param widget     This dialog window widget.
 * @param callbackID Unique numerical ID for the callback.
 */
void
GenericDialog::FinalizeCallback(GtkWidget *widget,
                                gpointer callbackID)
{
   // Assume success until we have to do something.
   bool success = true;
   // If this is not a Cancel callback, run the Apply logic.
   if (callbackID != _cancelCallbackID)
   {
      success = Apply();
   }
   // Hide the window if this is a cancel or a successful OK callback.
   if (success && callbackID != _applyCallbackID)
   {
      Hide();
   }
}

/**
 * Register the callback for the close-window event.
 */
void
GenericDialog::CreateWindowCloseCallback()
{
   // The close-window event will trigger the CloseEventCallback method.
   const GenericPluginUI::DialogEventCallbackMethod
      <GenericDialog, &GenericDialog::CloseEventCallback> closeCallback(*this);
   UIInstance().RegisterDialogEventCallback(_dialog, "delete_event", closeCallback);
}

/**
 * Register the callback for the OK button.
 *
 * @param button The OK button widget.
 */
void
GenericDialog::CreateOkButtonCallback(GtkWidget *button)
{
   // Clicking the OK button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the OK button.
   const GenericPluginUI::DialogSignalCallbackMethod
      <GenericDialog, &GenericDialog::FinalizeCallback> finalizeCallback(*this);
   _okCallbackID =
      UIInstance().RegisterDialogSignalCallback(button, "clicked", finalizeCallback);
}

/**
 * Register the callback for the Apply button.
 *
 * @param button The Apply button widget.
 */
void
GenericDialog::CreateApplyButtonCallback(GtkWidget *button)
{
   // Clicking the Apply button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the Apply button.
   const GenericPluginUI::DialogSignalCallbackMethod
      <GenericDialog, &GenericDialog::FinalizeCallback> finalizeCallback(*this);
   _applyCallbackID =
      UIInstance().RegisterDialogSignalCallback(button, "clicked", finalizeCallback);
}

/**
 * Register the callback for the Cancel button.
 *
 * @param button The Cancel button widget.
 */
void
GenericDialog::CreateCancelButtonCallback(GtkWidget *button)
{
   // Clicking the Cancel button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the Cancel button.
   const GenericPluginUI::DialogSignalCallbackMethod
      <GenericDialog, &GenericDialog::FinalizeCallback> finalizeCallback(*this);
   _cancelCallbackID =
      UIInstance().RegisterDialogSignalCallback(button, "clicked", finalizeCallback);
}
