/**
 * @file MainMenu.h
 * Declares the MainMenu class.
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

#if !defined(INCLUDED_MAINMENU_H)
#define INCLUDED_MAINMENU_H

#include "GenericMainMenu.h"
#include "MeshVisitor.h"

/**
 * Subclass of GenericMainMenu that constructs the commands for this plugin.
 *
 * @ingroup meshtex-ui
 */
class MainMenu : public GenericMainMenu
{
private: // private types

   /**
    * Visitor for invoking a function on a MeshEntity when that function does
    * not require any arguments other than the texture axes. These operations
    * are triggered immediately on selecting a menu entry, rather than being
    * triggered by applying some other dialog.
    */
   class PresetFuncVisitor : public MeshVisitor
   {
   public:
      /**
       * Function signature for a preset function.
       */
      typedef void(MeshEntity::*VisitorFunctor)(MeshEntity::TextureAxisSelection axes);
   public:
      PresetFuncVisitor(const VisitorFunctor& visitorFunctor,
                        MeshEntity::TextureAxisSelection axes);
   private:
      bool Execute(MeshEntity& meshEntity) const;

   private:
      const VisitorFunctor _visitorFunctor;
      const MeshEntity::TextureAxisSelection _axes;
   };

public: // public methods

   MainMenu(SmartPointer<GenericDialog>& setScaleDialog,
            SmartPointer<GenericDialog>& getInfoDialog,
            SmartPointer<GenericDialog>& genFuncDialog);
   ~MainMenu();
   void CommandMeshVisitor(const std::string& commandString);
   void CommandHelp(const std::string& commandString);
   void CommandAbout(const std::string& commandString);

private: // private methods

   void AddMeshVisitorEntry(const char *commandLabel,
                            const char *command,
                            const SmartPointer<MeshVisitor>& visitor);

private: // private types

   /**
    * Type for a map between a string and a reference-counted visitor.
    */
   typedef std::map<std::string, SmartPointer<MeshVisitor> > VisitorMap;

private:

   /**
    * Associations between commands and visitors that implement them.
    */
   VisitorMap _visitorMap;

   /**
    * Callback for all of the commands that trigger CommandMeshVisitor. This is
    * stored in a member var rather than a local var just because otherwise it
    * would need to be passed around and clutter up an already ugly set of
    * invocations.
    */
   const CommandCallbackMethod
      <MainMenu, &MainMenu::CommandMeshVisitor> _commandMeshVisitor;
};

#endif // #if !defined(INCLUDED_MAINMENU_H)