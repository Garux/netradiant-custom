/*
   BobToolz plugin for GtkRadiant
   Copyright (C) 2001 Gordon Biggans

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#include "qerplugin.h"
#include "signal/isignal.h"
#include "string/string.h"

#include "DEntity.h"
#include "ScriptParser.h"
#include "mathlib.h"
#include "misc.h"

#define MAX_QPATH 64

typedef struct treeModel_s {
	char name[MAX_QPATH];
} treeModel_t;

#define MAX_TP_MODELS 256

class DTreePlanter {
	MouseEventHandlerId m_mouseDown;
	SignalHandlerId m_destroyed;
public:
	SignalHandlerResult mouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers );
	typedef Member<DTreePlanter, SignalHandlerResult(const WindowVector&, ButtonIdentifier, ModifierFlags), &DTreePlanter::mouseDown> MouseDownCaller;
	void destroyed(){
		m_mouseDown = MouseEventHandlerId();
		m_destroyed = SignalHandlerId();
	}
	typedef Member<DTreePlanter, void(), &DTreePlanter::destroyed> DestroyedCaller;

	DTreePlanter() {
		m_world.LoadSelectedBrushes();
		if( m_world.brushList.size() == 0 )
			globalErrorStream() << "bobToolz::TreePlanter requires selected brushes to plant on!\n";

		char buffer[256];
		GetFilename( buffer, "bt/tp_ent.txt" );

		FILE* file = fopen( buffer, "rb" );
		if ( file ) {
			fseek( file, 0, SEEK_END );
			int len = ftell( file );
			fseek( file, 0, SEEK_SET );

			if ( len ) {
				char* buf = new char[len + 1];
				buf[len] = '\0';
				// parser will do the cleanup, dont delete.

				fread( buf, len, 1, file );

				CScriptParser parser;
				parser.SetScript( buf );

				ReadConfig( &parser );
			}

			fclose( file );
		}

		if( string_empty( m_entType ) )
			globalErrorStream() << "bobToolz::TreePlanter parsed no entity name from " << makeQuoted( buffer ) << '\n';

		m_mouseDown = GlobalRadiant().XYWindowMouseDown_connect( makeSignalHandler3( MouseDownCaller(), *this ) );
		m_destroyed = GlobalRadiant().XYWindowDestroyed_connect( makeSignalHandler( DestroyedCaller(), *this ) );
	}

	virtual ~DTreePlanter(){
		if ( !m_mouseDown.isNull() ) {
			GlobalRadiant().XYWindowMouseDown_disconnect( m_mouseDown );
		}
		if ( !m_destroyed.isNull() ) {
			GlobalRadiant().XYWindowDestroyed_disconnect( m_destroyed );
		}
	}

#define MT( t )   string_equal_nocase( pToken, t )
#define GT      pToken = pScriptParser->GetToken( true )
#define CT      if ( !*pToken ) { return; }

	void ReadConfig( CScriptParser* pScriptParser ) {
		const char* GT;
		CT;

		do {
			GT;
			if ( *pToken == '}' ) {
				break;
			}

			if ( MT( "model" ) ) {
				if ( m_numModels >= MAX_TP_MODELS ) {
					return;
				}

				GT;
				CT;

				strncpy( m_trees[m_numModels++].name, pToken, std::size( m_trees[0].name ) - 1 );
				m_trees[m_numModels].name[ std::size( m_trees[0].name ) - 1 ] = '\0';
			}
			else if ( MT( "link" ) ) {
				GT;
				CT;

				strncpy( m_linkName, pToken, std::size( m_linkName ) - 1 );
				m_linkName[ std::size( m_linkName ) - 1 ] = '\0';

				m_autoLink = true;
			}
			else if ( MT( "entity" ) ) {
				GT;
				CT;

				strncpy( m_entType, pToken, std::size( m_entType ) - 1 );
				m_entType[ std::size( m_entType ) - 1 ] = '\0';
			}
			else if ( MT( "offset" ) ) {
				GT;
				CT;

				m_offset = atoi( pToken );
			}
			else if ( MT( "pitch" ) ) {
				GT;
				CT;

				m_minPitch = atoi( pToken );

				GT;
				CT;

				m_maxPitch = atoi( pToken );

				m_setAngles = true;
			}
			else if ( MT( "yaw" ) ) {
				GT;
				CT;

				m_minYaw = atoi( pToken );

				GT;
				CT;

				m_maxYaw = atoi( pToken );

				m_setAngles = true;
			}
			else if ( MT( "scale" ) ) {
				GT;
				CT;

				m_minScale = static_cast<float>( atof( pToken ) );

				GT;
				CT;

				m_maxScale = static_cast<float>( atof( pToken ) );

				m_useScale = true;
			}
			else if ( MT( "numlinks" ) ) {
				GT;
				CT;

				m_linkNum = atoi( pToken );
			}
		} while ( true );
	}

	bool FindDropPoint( vec3_t in, vec3_t out ) const;
	void DropEntsToGround();

private:
	DEntity m_world;

	treeModel_t m_trees[MAX_TP_MODELS];

	int m_numModels = 0;
	int m_offset = 0;
	int m_maxPitch = 0;
	int m_minPitch = 0;
	int m_maxYaw = 0;
	int m_minYaw = 0;

	char m_entType[MAX_QPATH] = {};
	char m_linkName[MAX_QPATH] = {};
	int m_linkNum = 0;

	float m_minScale = 0;
	float m_maxScale = 0;

	bool m_useScale = false;
	bool m_setAngles = false;
	bool m_autoLink = false;
};

void MakeChain( int linkNum, const char* linkName );
