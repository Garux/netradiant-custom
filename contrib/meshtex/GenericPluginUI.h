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

#pragma once

#include <vector>

#include "GenericMainMenu.h"
#include "GenericDialog.h"

#include "qerplugin.h"
#include "generic/referencecounted.h"

/**
 * Framework for a manager of a command menu and dialog windows. It is
 * responsible for:
 * - holding a reference on those objects for lifecycle management
 * - providing lookup facilities for those objects
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

public: // public methods

   /// @name Setup
   //@{
   void RegisterMainMenu(SmartPointer<GenericMainMenu>& mainMenu);
   void RegisterDialog(SmartPointer<GenericDialog>& dialog);
   void SetWindow(QWidget *window);
   //@}
   /// @name Lookup
   //@{
   GenericMainMenu *MainMenu();
   GenericDialog *Dialog(const std::string& key);
   //@}
   /// @name Widget dependence
   //@{
   void RegisterWidgetDependence(QAbstractButton *controller,
                                 QWidget *controllee);
   void RegisterWidgetAntiDependence(QAbstractButton *controller,
                                     QWidget *controllee);
   void WidgetControlCallback( QAbstractButton *button );
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
    * Type for a map between a widget and a vector of widgets.
    */
   typedef std::map<QAbstractButton *, std::vector<QWidget *> > WidgetDependenceMap;
   typedef std::map<QWidget *, std::vector<QAbstractButton *> > WidgetDependenceByMap;

private: // private member vars

   /**
    * The parent window.
    */
   QWidget *_window;

   /**
    * Pointer to a reference-counted handle on the main menu object.
    */
   SmartPointer<GenericMainMenu> *_mainMenu;

   /**
    * Associations between keys and dialog windows.
    */
   DialogMap _dialogMap;

   /**
    * Associations between controller and controllee widgets for all dependences
    * and anti-dependences.
    */
   WidgetDependenceMap _widgetControlMap;

   /**
    * Associations between controllee and controller widgets for dependences
    * only.
    */
   WidgetDependenceByMap _widgetControlledByMap;

   /**
    * Associations between controller and controllee widgets for anti-
    * dependences only.
    */
   WidgetDependenceByMap _widgetAntiControlledByMap;
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
