/*
   PrtView plugin for GtkRadiant
   Copyright (C) 2001 Geoffrey Dewan, Loki software and qeradiant.com

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

#include "AboutDialog.h"
#include "qerplugin.h"
#include "version.h"

#include "prtview.h"
#include "ConfigDialog.h"

void DoAboutDlg(){
	constexpr char msg[] = "Version 1.000<br><br>"
	                       "Gtk port by Leonardo Zide<br>"
	                       "<a href='mailto:leo@lokigames.com'>leo@lokigames.com</a><br><br>"
	                       "Written by Geoffrey DeWan<br>"
	                       "<a href='mailto:gdewan@prairienet.org'>gdewan@prairienet.org</a><br><br>"
	                       "Built against NetRadiant " RADIANT_VERSION "<br>"
	                       __DATE__;
	GlobalRadiant().m_pfnMessageBox( g_pRadiantWnd, msg, "About Portal Viewer", EMessageBoxType::Info, 0 );
}
