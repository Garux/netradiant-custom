/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <QMenu>
#include <QToolBar>
#include "math/vectorfwd.h"
#include "string/stringfwd.h"

void process_gui();

void GlobalShortcuts_setWidget( QWidget *widget );

void command_connect_accelerator( const char* commandName );
void command_disconnect_accelerator( const char* commandName );
void toggle_add_accelerator( const char* commandName );
void toggle_remove_accelerator( const char* name );

// this also sets up the shortcut using command_connect_accelerator
QAction* create_menu_item_with_mnemonic( QMenu *menu, const char *mnemonic, const char* commandName );
// this also sets up the shortcut using command_connect_accelerator
QAction* create_check_menu_item_with_mnemonic( QMenu* menu, const char* mnemonic, const char* commandName );

// this also sets up the shortcut using command_connect_accelerator
QAction* toolbar_append_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName );
QAction* toolbar_append_button( QToolBar* toolbar, const char* description, const class QIcon& icon, const char* commandName );
// this also sets up the shortcut using command_connect_accelerator
QAction* toolbar_append_toggle_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName );

void toolbar_append_separator( QToolBar* toolbar );

void toolbar_construct_control_menu( QMenu *menu );
void toolbar_importState( const char *commandNames );


bool color_dialog( QWidget *parent, Vector3& color, const char* title = "Choose Color" );

bool OpenGLFont_dialog( QWidget *parent, const char* font, const int size, CopiedString &newfont, int &newsize );

class QLineEdit;
void button_clicked_entry_browse_file( QLineEdit* entry );
void button_clicked_entry_browse_directory( QLineEdit* entry );
