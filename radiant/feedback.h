/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// classes used for describing geometry information from q3map feedback
//

#pragma once

#include "generic/vector.h"
#include "stream/stringstream.h"
#include "xmlstuff.h"
#include "dialog.h"
#include "qerplugin.h"

// we use these classes to let plugins draw inside the Radiant windows
// 2D window like YZ XZ XY
class IGL2DWindow
{
public:
// Increment the number of references to this object
	virtual void IncRef() = 0;
// Decrement the reference count
	virtual void DecRef() = 0;
	virtual void Draw2D( VIEWTYPE vt ) = 0;
};

// 3D window
class IGL3DWindow
{
public:
// Increment the number of references to this object
	virtual void IncRef() = 0;
// Decrement the reference count
	virtual void DecRef() = 0;
	virtual void Draw3D() = 0;
};

// a select message with a brush/entity select information
class CSelectMsg : public ISAXHandler
{
	enum { SELECT_MESSAGE, SELECT_BRUSH } ESelectState;
	StringOutputStream message;
	StringOutputStream brush;
public:
	CSelectMsg() {
		ESelectState = SELECT_MESSAGE;
	}
// SAX interface
	void saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ) override;
	void saxEndElement( message_info_t *ctx, const xmlChar *name ) override;
	void saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ) override;
// for use in the dialog window
	const char* getName() override {
		return message;
	}
	IGL2DWindow* Highlight() override;
	void DropHighlight() override { }
};

class CPointMsg final : public ISAXHandler, public IGL2DWindow
{
	enum { POINT_MESSAGE, POINT_POINT } EPointState;
	StringOutputStream message;
	StringOutputStream point;
	Vector3 pt;
	int refCount;
public:
	CPointMsg() {
		EPointState = POINT_MESSAGE;
		refCount = 0;
	}
// SAX interface
	void Release() override {
		delete this;
	}
	void saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ) override;
	void saxEndElement( message_info_t *ctx, const xmlChar *name ) override;
	void saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ) override;
// for use in the dialog window
	const char* getName() override {
		return message;
	}
	IGL2DWindow* Highlight() override;
	void DropHighlight() override;

// IGL2DWindow interface --------------------------------
// Increment the number of references to this object
	void IncRef() override {
		refCount++;
	}
// Decrement the reference count
	void DecRef() override {
		refCount--;
		if ( refCount <= 0 ) {
			delete this;
		}
	}
	void Draw2D( VIEWTYPE vt ) override;
};

class CWindingMsg final : public ISAXHandler, public IGL2DWindow
{
	enum { WINDING_MESSAGE, WINDING_WINDING } EPointState;
	StringOutputStream message;
	StringOutputStream winding;
	Vector3 wt[256];
	int numpoints;
	int refCount;
public:
	CWindingMsg() {
		EPointState = WINDING_MESSAGE;
		refCount = 0;
		numpoints = 0;
	}
// SAX interface
	void Release() override {
		delete this;
	}
	void saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ) override;
	void saxEndElement( message_info_t *ctx, const xmlChar *name ) override;
	void saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ) override;
// for use in the dialog window
	const char* getName() override {
		return message;
	}
	IGL2DWindow* Highlight() override;
	void DropHighlight() override;

// IGL2DWindow interface --------------------------------
// Increment the number of references to this object
	void IncRef() override {
		refCount++;
	}
// Decrement the reference count
	void DecRef() override {
		refCount--;
		if ( refCount <= 0 ) {
			delete this;
		}
	}
	void Draw2D( VIEWTYPE vt ) override;
};

class CDbgDlg : public Dialog
{
	std::vector<ISAXHandler*> m_feedbackElements;
// the list widget we use in the dialog
	class QTreeWidget *m_clist{};
	ISAXHandler *m_pHighlight{};
	IGL2DWindow* m_pDraw2D{};
public:
// refresh items
	void Push( ISAXHandler * );
// clean the debug window, release all ISAXHanlders we have
	void Init();
private:
	void feedback_selection_changed( class QTreeWidgetItem *current );
	void SetHighlight( std::size_t row );
	void DropHighlight();
public:
	void draw2D( VIEWTYPE viewType ){
		if ( m_pDraw2D != 0 ) {
			m_pDraw2D->Draw2D( viewType );
		}
	}
	void destroyWindow(){
		if ( GetWidget() != 0 ) {
			Destroy();
		}
	}
//  void HideDlg();
protected:
	void BuildDialog() override;
};

extern CDbgDlg g_DbgDlg;

void Feedback_draw2D( VIEWTYPE viewType );
