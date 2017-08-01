/**
 * @file GenericMainMenu.h
 * Declares the GenericMainMenu class.
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

#if !defined(INCLUDED_GENERICMAINMENU_H)
#define INCLUDED_GENERICMAINMENU_H

#include <string>
#include <map>

#include "RefCounted.h"
#include "GenericDialog.h"

#include "generic/callback.h"
#include "generic/referencecounted.h"

/**
 * String used in lists of command tokens or command labels to separate groups
 * of commands.
 */
#define MENU_SEPARATOR "-"

/**
 * Framework for a Radiant plugin's main menu. This object handles the menu
 * logic: what commands exist and how to execute them. The actual menu
 * display is handled by Radiant.
 * 
 * A subclass should handle creating the command list.
 *
 * @ingroup generic-ui
 */
class GenericMainMenu : public RefCounted
{
protected: // protected methods

   /// @name Lifecycle
   //@{
   GenericMainMenu();
   virtual ~GenericMainMenu();
   //@}

private: // private methods

   /// @name Unimplemented to prevent copy/assignment
   //@{
   GenericMainMenu(const GenericMainMenu&);
   const GenericMainMenu& operator=(const GenericMainMenu&);
   //@}

public: // public methods

   /// @name Service the plugin interface
   //@{
   virtual void Dispatch(const char *command);
   virtual void CommandDialogShow(const std::string& commandString);
   const std::string& GetCommandList() const;
   const std::string& GetCommandLabelList() const;
   //@}

protected: // protected types
   /**
    * Function signature for a menu command callback. The callback takes a
    * string argument (the command token); it has no return value.
    */
   typedef Callback1<const std::string&, void> CommandCallback;

   /**
    * An instance of this class can be used as a
    * GenericMainMenu::CommandCallback, in situations where the callback is a
    * method to be invoked on a target object. When invoking this constructor,
    * the target object is the constructor argument, and the target object class
    * and method are template parameters. The target object's method must have
    * an appropriate signature for CommandCallback: one string argument, void
    * return.
    */
   template<typename ObjectClass, void (ObjectClass::*member)(const std::string&)>
   class CommandCallbackMethod :
      public MemberCaller1<ObjectClass, const std::string&, member>
   {
   public:
      /**
       * Constructor.
       *
       * @param object The object on which to invoke the callback method.
       */
      CommandCallbackMethod(ObjectClass& object) :
         MemberCaller1<ObjectClass, const std::string&, member>(object) {}
   };

protected: // protected methods

   /// @name Command list construction
   //@{
   virtual void BeginEntries();
   virtual void AddSeparator();
   virtual std::string AddEntry(const char *commandLabel,
                                const char *command,
                                const CommandCallback& commandCallback);
   virtual void AddDialogShowEntry(const char *commandLabel,
                                   const char *command,
                                   const SmartPointer<GenericDialog>& dialog);
   virtual void EndEntries();
   //@}

private: // private types

   /**
    * Type for a map between string and reference-counted dialog window.
    */
   typedef std::map<std::string, SmartPointer<GenericDialog> > DialogMap;

private: // private member vars

   /**
    * Semicolon-separated string of command tokens.
    */
   std::string _menuCommands;

   /**
    * Semicolon-separated string of command labels.
    */
   std::string _menuCommandLabels;

   /**
    * Associations between command tokens and callbacks.
    */
   std::map<std::string, CommandCallback> _dispatchMap;

   /**
    * Associations between command tokens and dialog windows.
    */
   DialogMap _dialogMap;
};

#endif // #if !defined(INCLUDED_GENERICMAINMENU_H)