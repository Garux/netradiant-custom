/**
 * @file GenericPluginUI.h
 * Declares the GenericPluginUI class.
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

#if !defined(INCLUDED_GENERICPLUGINUI_H)
#define INCLUDED_GENERICPLUGINUI_H

#include <vector>
#include <gdk/gdk.h>
#include <glib.h>

#include "GenericMainMenu.h"
#include "GenericDialog.h"

#include "qerplugin.h"
#include "generic/referencecounted.h"

/**
 * Framework for a manager of a command menu and dialog windows. It is
 * responsible for:
 * - holding a reference on those objects for lifecycle management
 * - providing lookup facilities for those objects
 * - mapping GTK+ event/signal callbacks into method invocations on
 * the dialog objects
 * - providing automatic handling of widgets that should be active or
 * inactive based on the active state of other widgets
 * - providing utility functions for generating message popups
 *
 * A subclass should handle the creation and registration of the UI objects.
 *
 * @ingroup generic-ui
 */
class GenericPluginUI
{
protected: // protected methods

   /// @name Lifecycle
   //@{
   GenericPluginUI();
   virtual ~GenericPluginUI();
   //@}

private: // private methods

   /// @name Unimplemented to prevent copy/assignment
   //@{
   GenericPluginUI(const GenericPluginUI&);
   const GenericPluginUI& operator=(const GenericPluginUI&);
   //@}

public: // public types

   /**
    * Type for GTK+ event callbacks. The callback takes a GtkWidget* argument
    * (widget generating the event), a GdkEvent* argument (the event), and a
    * gpointer argument (the callback ID); it returns gint (success as TRUE or
    * FALSE).
    */
   typedef Callback3<GtkWidget *, GdkEvent *, gpointer, gint> DialogEventCallback;

   /**
    * Type for GTK+ signal callbacks. The callback takes a GtkWidget* argument
    * (widget generating the signal) and a gpointer argument (the callback data);
    * it has no return value.
    */
   typedef Callback2<GtkWidget *, gpointer, void> DialogSignalCallback;

   /**
    * An instance of this class can be used as a
    * GenericPluginUI::DialogEventCallback, in situations where the callback is
    * a method to be invoked on a target object. When invoking this constructor,
    * the target object is the constructor argument, and the target object class
    * and method are template parameters. The target object's method must have
    * an appropriate signature for DialogEventCallback: one GtkWidget* argument,
    * one GdkEvent* argument, one gpointer argument, gint return.
    */
   template<typename ObjectClass, gint (ObjectClass::*member)(GtkWidget *, GdkEvent*, gpointer)>
   class DialogEventCallbackMethod : public BindFirstOpaque3<Member3<ObjectClass, GtkWidget *, GdkEvent*, gpointer, gint, member> >
   {
   public:
      /**
       * Constructor.
       *
       * @param object The object on which to invoke the callback method.
       */
      DialogEventCallbackMethod(ObjectClass& object) :
         BindFirstOpaque3<Member3<ObjectClass, GtkWidget *, GdkEvent *, gpointer, gint, member> >(object) {}
   };

   /**
    * An instance of this class can be used as a
    * GenericPluginUI::DialogSignalCallback, in situations where the callback is
    * a method to be invoked on a target object. When invoking this constructor,
    * the target object is the constructor argument, and the target object class
    * and method are template parameters. The target object's method must have
    * an appropriate signature for DialogSignalCallback: one GtkWidget* argument,
    * one gpointer argument, void return.
    */
   template<typename ObjectClass, void (ObjectClass::*member)(GtkWidget *, gpointer)>
   class DialogSignalCallbackMethod : public BindFirstOpaque2<Member2<ObjectClass, GtkWidget *, gpointer, void, member> >
   {
   public:
      /**
       * Constructor.
       *
       * @param object The object on which to invoke the callback method.
       */
      DialogSignalCallbackMethod(ObjectClass& object) :
         BindFirstOpaque2<Member2<ObjectClass, GtkWidget *, gpointer, void, member> >(object) {}
   };

public: // public methods

   /// @name Setup
   //@{
   void RegisterMainMenu(SmartPointer<GenericMainMenu>& mainMenu);
   void RegisterDialog(SmartPointer<GenericDialog>& dialog);
   void SetWindow(GtkWidget *window);
   //@}
   /// @name Lookup
   //@{
   GenericMainMenu *MainMenu();
   GenericDialog *Dialog(const std::string& key);
   //@}
   /// @name Event/signal dispatch
   //@{
   static gint DialogEventCallbackDispatch(GtkWidget *widget,
                                           GdkEvent* event,
                                           gpointer data);
   static void DialogSignalCallbackDispatch(GtkWidget *widget,
                                            gpointer data);
   gpointer RegisterDialogEventCallback(GtkWidget *widget,
                                        const gchar *name,
                                        const DialogEventCallback& callback);
   gpointer RegisterDialogSignalCallback(GtkWidget *widget,
                                         const gchar *name,
                                         const DialogSignalCallback& callback);
   //@}
   /// @name Widget dependence
   //@{
   void RegisterWidgetDependence(GtkWidget *controller,
                                 GtkWidget *controllee);
   void RegisterWidgetAntiDependence(GtkWidget *controller,
                                     GtkWidget *controllee);
   void WidgetControlCallback(GtkWidget *widget,
                              gpointer callbackID);
   //@}
   /// @name Message popups
   //@{
   static void ErrorReportDialog(const char *title,
                                 const char *message);
   static void WarningReportDialog(const char *title,
                                   const char *message);
   static void InfoReportDialog(const char *title,
                                const char *message);
   //@}

private: // private types

   /**
    * Type for a map between string and reference-counted dialog window.
    */
   typedef std::map<std::string, SmartPointer<GenericDialog> > DialogMap;

   /**
    * Type for a map between gpointer (for callback ID) and event callback.
    */
   typedef std::map<gpointer, DialogEventCallback> DialogEventCallbackMap;

   /**
    * Type for a map between gpointer (for callback ID) and signal callback.
    */
   typedef std::map<gpointer, DialogSignalCallback> DialogSignalCallbackMap;

   /**
    * Type for a map between a widget and a vector of widgets.
    */
   typedef std::map<GtkWidget *, std::vector<GtkWidget *> > WidgetDependenceMap;

private: // private member vars

   /**
    * The parent window.
    */
   GtkWidget *_window;

   /**
    * Pointer to a reference-counted handle on the main menu object.
    */
   SmartPointer<GenericMainMenu> *_mainMenu;

   /**
    * Next ID to use when registering an event or signal callback. Starts at 1;
    * 0 is reserved to mean invalid.
    */
   unsigned _callbackID;

   /**
    * Callback to implement widget-active dependences.
    */
   const DialogSignalCallbackMethod<GenericPluginUI, &GenericPluginUI::WidgetControlCallback>
      _widgetControlCallback;

   /**
    * Associations between keys and dialog windows.
    */
   DialogMap _dialogMap;

   /**
    * Associations between callback IDs and event callbacks.
    */
   DialogEventCallbackMap _dialogEventCallbackMap;

   /**
    * Associations between callback IDs and signal callbacks.
    */
   DialogSignalCallbackMap _dialogSignalCallbackMap;

   /**
    * Associations between controller and controllee widgets for all dependences
    * and anti-dependences.
    */
   WidgetDependenceMap _widgetControlMap;

   /**
    * Associations between controller and controllee widgets for dependences
    * only.
    */
   WidgetDependenceMap _widgetControlledByMap;

   /**
    * Associations between controller and controllee widgets for anti-
    * dependences only.
    */
   WidgetDependenceMap _widgetAntiControlledByMap;
};


/**
 * Get the singleton instance of the UI manager.
 *
 * @internal
 * This function is not implemented in the GenericPluginUI.cpp file. It must
 * be implemented somewhere, because it is invoked from various places even
 * within the generic UI code. The implementation of this function should
 * return a reference to the singleton instance of a GenericPluginUI subclass.
 * @endinternal
 *
 * @return Reference to a singleton that implements GenericPluginUI.
 *
 * @relates GenericPluginUI
 */
GenericPluginUI& UIInstance();

#endif // #if !defined(INCLUDED_GENERICPLUGINUI_H)
