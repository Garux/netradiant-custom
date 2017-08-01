/**
 * @file GenericDialog.h
 * Declares the GenericDialog class.
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

#if !defined(INCLUDED_GENERICDIALOG_H)
#define INCLUDED_GENERICDIALOG_H

#include <string>
#include <gdk/gdk.h>
#include <glib.h>

#include "RefCounted.h"

#include "qerplugin.h"

/**
 * Macro to get a handle on a widget inside the dialog by name.
 *
 * @param widgetName Name of the contained widget to find.
 */
#define NamedWidget(widgetName) \
   (gtk_object_get_data(GTK_OBJECT(_dialog), widgetName))

/**
 * Macro to enable/disable a widget inside the dialog, selected by name.
 *
 * @param widgetName Name of the contained widget to enable/disable.
 */
#define NamedToggleWidgetActive(widgetName) \
   (GTK_TOGGLE_BUTTON(NamedWidget(widgetName))->active)

/**
 * Macro to read text from a widget inside the dialog, selected by name.
 *
 * @param widgetName Name of the contained widget to read from.
 */
#define NamedEntryWidgetText(widgetName) \
   (gtk_entry_get_text(GTK_ENTRY(NamedWidget(widgetName))))

/**
 * Framework for a basic dialog window with OK/Apply/Cancel actions.
 *
 * A subclass should handle decorating/customizing the window, populating it
 * with contained widgets, customizing the Apply logic, and registering the
 * appropriate OK/Apply/Cancel buttons.
 *
 * @ingroup generic-ui
 */
class GenericDialog : public RefCounted
{
protected: // protected methods

   /// @name Lifecycle
   //@{
   GenericDialog(const std::string& key);
   virtual ~GenericDialog();
   //@}

private: // private methods

   /// @name Unimplemented to prevent copy/assignment
   //@{
   GenericDialog(const GenericDialog&);
   const GenericDialog& operator=(const GenericDialog&);
   //@}

public: // public methods

   /// @name Interrogation
   //@{
   const std::string& GetKey() const;
   //@}
   /// @name Window management
   //@{
   virtual void SetWindow(GtkWidget *window);
   virtual void Raise();
   virtual void Show(const std::string& triggerCommand);
   virtual void Hide();
   //@}
   /// @name Callback implementation
   //@{
   virtual bool Apply();
   virtual gint CloseEventCallback(GtkWidget *widget,
                                   GdkEvent* event,
                                   gpointer callbackID);
   virtual void FinalizeCallback(GtkWidget *widget,
                                 gpointer callbackID);
   //@}
   /// @name Callback creation
   //@{
   void CreateWindowCloseCallback();
   void CreateOkButtonCallback(GtkWidget *button);
   void CreateApplyButtonCallback(GtkWidget *button);
   void CreateCancelButtonCallback(GtkWidget *button);
   //@}

protected: // protected member vars

   /**
    * This dialog widget.
    */
   GtkWidget *_dialog;

   /**
    * Parent window.
    */
   GtkWidget *_window;

   /**
    * Unique key for this dialog.
    */
   const std::string _key;

   /**
    * Command token that most recently summoned this dialog.
    */
   std::string _triggerCommand;

   /**
    * Callback ID associated with an OK button; 0 if none.
    */
   gpointer _okCallbackID;

   /**
    * Callback ID associated with an Apply button; 0 if none.
    */
   gpointer _applyCallbackID;

   /**
    * Callback ID associated with a Cancel button; 0 if none.
    */
   gpointer _cancelCallbackID;
};

#endif // #if !defined(INCLUDED_GENERICDIALOG_H)
