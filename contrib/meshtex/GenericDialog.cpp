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

#include "GenericDialog.h"
#include "GenericPluginUI.h"


/**
 * Constructor. Create the Qt widget for the dialog window (not visible
 * yet). Initialize callback IDs to zero (invalid). Note that as this is a
 * protected method, GenericDialog objects cannot be created directly; only
 * subclasses of GenericDialog can be created.
 *
 * @param key Unique key to identify this dialog widget.
 */
GenericDialog::GenericDialog(const std::string& key) :
   _dialog(new QDialog( nullptr, Qt::Dialog | Qt::WindowCloseButtonHint )),
   _window(nullptr),
   _key(key),
   _okCallbackID(0),
   _applyCallbackID(0),
   _cancelCallbackID(0)
{
}

/**
 * Virtual destructor.
 */
GenericDialog::~GenericDialog()
{
   // destroyed by parent
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
GenericDialog::SetWindow(QWidget *window)
{
   // Remember the parent window.
   _window = window;
   // Mark this widget as a modal dialog for it.
   if (_dialog != nullptr)
   {
      static_cast<QObject*>( _dialog )->setParent( _window );
   }
}

/**
 * Raise this dialog window to the top of the window stack.
 */
void
GenericDialog::Raise()
{
   // Don't bother if not visible.
   if (_dialog->isVisible())
   {
      _dialog->raise();
      _dialog->activateWindow();
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
   if (!_dialog->isVisible())
   {
      _dialog->show();
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
   if (_dialog->isVisible())
   {
      _dialog->hide();
   }
}

/**
 * Default handler for Apply logic. This method should be overridden by
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
 * Callback for clicking on OK/Apply/Cancel button.
 *
 * @param widget     This dialog window widget.
 * @param callbackID Unique numerical ID for the callback.
 */
void
GenericDialog::FinalizeCallback(QAbstractButton *callbackID)
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
 * Register the callback for the OK button.
 *
 * @param button The OK button widget.
 */
void
GenericDialog::CreateOkButtonCallback(QAbstractButton *button)
{
   // Clicking the OK button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the OK button.
   _okCallbackID = button;
   QObject::connect( button, &QAbstractButton::clicked, [this, button](){ FinalizeCallback( button ); } );
}

/**
 * Register the callback for the Apply button.
 *
 * @param button The Apply button widget.
 */
void
GenericDialog::CreateApplyButtonCallback(QAbstractButton *button)
{
   // Clicking the Apply button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the Apply button.
   _applyCallbackID = button;
   QObject::connect( button, &QAbstractButton::clicked, [this, button](){ FinalizeCallback( button ); } );
}

/**
 * Register the callback for the Cancel button.
 *
 * @param button The Cancel button widget.
 */
void
GenericDialog::CreateCancelButtonCallback(QAbstractButton *button)
{
   // Clicking the Cancel button will trigger the FinalizeCallback method. Since
   // FinalizeCallback can be used for multiple buttons, we'll save the specific
   // callback ID associated with the Cancel button.
   _cancelCallbackID = button;
   QObject::connect( button, &QAbstractButton::clicked, [this, button](){ FinalizeCallback( button ); } );
}
