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

#include "feedback.h"

#include "debugging/debugging.h"

#include "igl.h"
#include "iselection.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>

#include "map.h"
#include "dialog.h"
#include "mainframe.h"


CDbgDlg g_DbgDlg;

void Feedback_draw2D( VIEWTYPE viewType ){
	g_DbgDlg.draw2D( viewType );
}

void CSelectMsg::saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "select" ) ) {
		// read the message
		ESelectState = SELECT_MESSAGE;
	}
	else
	{
		// read the brush
		ASSERT_MESSAGE( string_equal( reinterpret_cast<const char*>( name ), "brush" ), "FEEDBACK PARSE ERROR" );
		ASSERT_MESSAGE( ESelectState == SELECT_MESSAGE, "FEEDBACK PARSE ERROR" );
		ESelectState = SELECT_BRUSH;
		globalWarningStream() << message << '\n';
	}
}

void CSelectMsg::saxEndElement( message_info_t *ctx, const xmlChar *name ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "select" ) ) {
	}
}

void CSelectMsg::saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ){
	if ( ESelectState == SELECT_MESSAGE ) {
		message.write( reinterpret_cast<const char*>( ch ), len );
	}
	else
	{
		brush.write( reinterpret_cast<const char*>( ch ), len );
	}
}

IGL2DWindow* CSelectMsg::Highlight(){
	GlobalSelectionSystem().setSelectedAll( false );
	int entitynum, brushnum;
	if ( sscanf( brush, "%i %i", &entitynum, &brushnum ) == 2 ) {
		SelectBrush( entitynum, brushnum );
	}
	return 0;
}

void CPointMsg::saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "pointmsg" ) ) {
		// read the message
		EPointState = POINT_MESSAGE;
	}
	else
	{
		// read the brush
		ASSERT_MESSAGE( string_equal( reinterpret_cast<const char*>( name ), "point" ), "FEEDBACK PARSE ERROR" );
		ASSERT_MESSAGE( EPointState == POINT_MESSAGE, "FEEDBACK PARSE ERROR" );
		EPointState = POINT_POINT;
		globalWarningStream() << message << '\n';
	}
}

void CPointMsg::saxEndElement( message_info_t *ctx, const xmlChar *name ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "pointmsg" ) ) {
	}
	else if ( string_equal( reinterpret_cast<const char*>( name ), "point" ) ) {
		sscanf( point, "%g %g %g", &( pt[0] ), &( pt[1] ), &( pt[2] ) );
		point.clear();
	}
}

void CPointMsg::saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ){
	if ( EPointState == POINT_MESSAGE ) {
		message.write( reinterpret_cast<const char*>( ch ), len );
	}
	else
	{
		ASSERT_MESSAGE( EPointState == POINT_POINT, "FEEDBACK PARSE ERROR" );
		point.write( reinterpret_cast<const char*>( ch ), len );
	}
}

IGL2DWindow* CPointMsg::Highlight(){
	return this;
}

void CPointMsg::DropHighlight(){
}

void CPointMsg::Draw2D( VIEWTYPE vt ){
	NDIM1NDIM2( vt )
	gl().glPointSize( 4 );
	gl().glColor3f( 1.0f, 0.0f, 0.0f );
	gl().glBegin( GL_POINTS );
	gl().glVertex2f( pt[nDim1], pt[nDim2] );
	gl().glEnd();
	gl().glBegin( GL_LINE_LOOP );
	gl().glVertex2f( pt[nDim1] - 8, pt[nDim2] - 8 );
	gl().glVertex2f( pt[nDim1] + 8, pt[nDim2] - 8 );
	gl().glVertex2f( pt[nDim1] + 8, pt[nDim2] + 8 );
	gl().glVertex2f( pt[nDim1] - 8, pt[nDim2] + 8 );
	gl().glEnd();
}

void CWindingMsg::saxStartElement( message_info_t *ctx, const xmlChar *name, const xmlChar **attrs ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "windingmsg" ) ) {
		// read the message
		EPointState = WINDING_MESSAGE;
	}
	else
	{
		// read the brush
		ASSERT_MESSAGE( string_equal( reinterpret_cast<const char*>( name ), "winding" ), "FEEDBACK PARSE ERROR" );
		ASSERT_MESSAGE( EPointState == WINDING_MESSAGE, "FEEDBACK PARSE ERROR" );
		EPointState = WINDING_WINDING;
		globalWarningStream() << message << '\n';
	}
}

void CWindingMsg::saxEndElement( message_info_t *ctx, const xmlChar *name ){
	if ( string_equal( reinterpret_cast<const char*>( name ), "windingmsg" ) ) {
	}
	else if ( string_equal( reinterpret_cast<const char*>( name ), "winding" ) ) {
		const char* c = winding;
		sscanf( c, "%i ", &numpoints );

		int i = 0;
		for (; i < numpoints; i++ )
		{
			c = strchr( c + 1, '(' );
			if ( c ) { // even if we are given the number of points when the cycle begins .. don't trust it too much
				sscanf( c, "(%g %g %g)", &wt[i][0], &wt[i][1], &wt[i][2] );
			}
			else{
				break;
			}
		}
		numpoints = i;
	}
}

void CWindingMsg::saxCharacters( message_info_t *ctx, const xmlChar *ch, int len ){
	if ( EPointState == WINDING_MESSAGE ) {
		message.write( reinterpret_cast<const char*>( ch ), len );
	}
	else
	{
		ASSERT_MESSAGE( EPointState == WINDING_WINDING, "FEEDBACK PARSE ERROR" );
		winding.write( reinterpret_cast<const char*>( ch ), len );
	}
}

IGL2DWindow* CWindingMsg::Highlight(){
	return this;
}

void CWindingMsg::DropHighlight(){
}

void CWindingMsg::Draw2D( VIEWTYPE vt ){
	int i;

	NDIM1NDIM2( vt )
	gl().glColor3f( 1.0f, 0.f, 0.0f );

	gl().glPointSize( 4 );
	gl().glBegin( GL_POINTS );
	for ( i = 0; i < numpoints; i++ )
		gl().glVertex2f( wt[i][nDim1], wt[i][nDim2] );
	gl().glEnd();
	gl().glPointSize( 1 );

	gl().glEnable( GL_BLEND );
	gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	gl().glColor4f( 0.133f, 0.4f, 1.0f, 0.5f );
	gl().glBegin( GL_POLYGON );
	for ( i = 0; i < numpoints; i++ )
		gl().glVertex2f( wt[i][nDim1], wt[i][nDim2] );
	gl().glEnd();
	gl().glDisable( GL_BLEND );
}

// triggered when the user selects an entry in the feedback box
void CDbgDlg::feedback_selection_changed( QTreeWidgetItem *current ){
	DropHighlight();

	if( current != nullptr ){
		SetHighlight( m_clist->indexOfTopLevelItem( current ) );
	}
}

void CDbgDlg::DropHighlight(){
	if ( m_pHighlight != 0 ) {
		m_pHighlight->DropHighlight();
		m_pHighlight = 0;
		m_pDraw2D = 0;
	}
}

void CDbgDlg::SetHighlight( std::size_t row ){
	if ( ISAXHandler *h = m_feedbackElements.at( row ) ) {
		m_pDraw2D = h->Highlight();
		m_pHighlight = h;
	}
}

void CDbgDlg::Init(){
	DropHighlight();

	// free all the ISAXHandler*, clean it
	for( auto *e : m_feedbackElements )
		e->Release();
	m_feedbackElements.clear();

	if ( m_clist != NULL ) {
		m_clist->clear();
	}
}

void CDbgDlg::Push( ISAXHandler *pHandler ){
	// push in the list
	m_feedbackElements.push_back( pHandler );

	if ( GetWidget() == 0 ) {
		Create( MainFrame_getWindow() );
	}

	// put stuff in the list
	m_clist->clear();
	for ( auto *element : m_feedbackElements )
	{
		auto *item = new QTreeWidgetItem( m_clist );
		item->setText( 0, element->getName() );
	}

	ShowDlg();
}

void CDbgDlg::BuildDialog(){
	GetWidget()->setWindowTitle( "Q3Map debug window" );

	auto *tree = m_clist = new QTreeWidget;
	( new QVBoxLayout( GetWidget() ) )->addWidget( tree );
	tree->setColumnCount( 1 );
	tree->setUniformRowHeights( true ); // optimization
	tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
	tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
	tree->header()->setStretchLastSection( false ); // non greedy column sizing
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
	tree->setHeaderHidden( true );
	tree->setRootIsDecorated( false );
	tree->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );

	QObject::connect( tree, &QTreeWidget::currentItemChanged, [this]( QTreeWidgetItem *current ){ feedback_selection_changed( current ); } );
}
