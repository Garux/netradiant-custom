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

#pragma once

#include <string>

#include "RefCounted.h"

#include "qerplugin.h"

#include <QDialog>
#include <QWidget>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QRadioButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include "gtkutil/spinbox.h"


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
   virtual void SetWindow(QWidget *window);
   virtual void Raise();
   virtual void Show(const std::string& triggerCommand);
   virtual void Hide();
   //@}
   /// @name Callback implementation
   //@{
   virtual bool Apply();
   virtual void FinalizeCallback(QAbstractButton *callbackID);
   //@}
   /// @name Callback creation
   //@{
   void CreateOkButtonCallback(QAbstractButton *button);
   void CreateApplyButtonCallback(QAbstractButton *button);
   void CreateCancelButtonCallback(QAbstractButton *button);
   //@}

protected: // protected member vars

   /**
    * This dialog widget.
    */
   QDialog *_dialog;

   /**
    * Parent window.
    */
   QWidget *_window;

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
   QAbstractButton *_okCallbackID;

   /**
    * Callback ID associated with an Apply button; 0 if none.
    */
   QAbstractButton *_applyCallbackID;

   /**
    * Callback ID associated with a Cancel button; 0 if none.
    */
   QAbstractButton *_cancelCallbackID;
};
