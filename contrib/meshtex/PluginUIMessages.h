/**
 * @file PluginUIMessages.h
 * String constants for messages shown in dialogs.
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

#if !defined(INCLUDED_PLUGINUIMESSAGES_H)
#define INCLUDED_PLUGINUIMESSAGES_H

#include "GenericPluginUIMessages.h"
#include "PluginProperties.h"

/// @name Window titles
//@{
#define DIALOG_GET_INFO_TITLE "Get Info"
#define DIALOG_MESH_INFO_TITLE "Mesh Info"
#define DIALOG_SET_SCALE_TITLE "Set S/T Scale"
#define DIALOG_GEN_FUNC_TITLE "General Function"
#define DIALOG_ABOUT_TITLE "About"
#define DIALOG_HELP_TITLE "Help"
//@}

/// @name Popups
//@{
#define DIALOG_MULTIMESHES_ERROR "Must select only one patch mesh for this function."
#define DIALOG_NOMESHES_MSG "No valid patch meshes selected."
#define DIALOG_ABOUT_MSG PLUGIN_NAME " " PLUGIN_VERSION "\n\n" PLUGIN_DESCRIPTION "\n\n" PLUGIN_AUTHOR " (" PLUGIN_AUTHOR_EMAIL ")"
#define DIALOG_HELP_MSG "The Set S/T Scale, Get Info, and General Function dialogs will affect patch meshes that are selected when OK or Apply is clicked. For the other menu options, select the mesh(es) before selecting the option."
//@}

/// @name Get Info
//@{
#define DIALOG_GET_INFO_S_ROW_HEADER " S ref row:"
#define DIALOG_GET_INFO_T_COL_HEADER " T ref col:"
#define DIALOG_GET_INFO_XFER_OPT_LABEL "Transfer reference scale to Set S/T Scale"
//@}

/// @name Set S/T Scale
//@{
#define DIALOG_SET_SCALE_S_ACTIVE_OPT_LABEL "Set S"
#define DIALOG_SET_SCALE_T_ACTIVE_OPT_LABEL "Set T"
#define DIALOG_SET_SCALE_METHOD_FRAME_TITLE "Scaling"
#define DIALOG_SET_SCALE_TILES_OPT_LABEL "# Tiles"
#define DIALOG_SET_SCALE_NATURAL_OPT_LABEL "Natural *"
#define DIALOG_SET_SCALE_MAX_OPT_LABEL "Max"
#define DIALOG_SET_SCALE_S_ALIGN_FRAME_TITLE "\"Zero\" col"
#define DIALOG_SET_SCALE_T_ALIGN_FRAME_TITLE "\"Zero\" row"
#define DIALOG_SET_SCALE_S_REF_ROW_OPT_LABEL "Use reference row"
#define DIALOG_SET_SCALE_T_REF_COL_OPT_LABEL "Use reference col"
#define DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL "Total length only"
//@}

/// @name General Function
//@{
#define DIALOG_GEN_FUNC_SURFACE_VALUES "Surface values"
#define DIALOG_GEN_FUNC_CONTROL_VALUES "Control values"
#define DIALOG_GEN_FUNC_S_FUNC_LABEL " S =  "
#define DIALOG_GEN_FUNC_T_FUNC_LABEL " T =  "
#define DIALOG_GEN_FUNC_OLD_S_LABEL " * old_S  + "
#define DIALOG_GEN_FUNC_OLD_T_LABEL " * old_T  + "
#define DIALOG_GEN_FUNC_ROW_DIST_LABEL " * row_dist  + "
#define DIALOG_GEN_FUNC_COL_DIST_LABEL " * col_dist  + "
#define DIALOG_GEN_FUNC_ROW_NUM_LABEL " * row_num  + "
#define DIALOG_GEN_FUNC_COL_NUM_LABEL " * col_num  + "
#define DIALOG_GEN_FUNC_MAX_OPT_LABEL DIALOG_SET_SCALE_MAX_OPT_LABEL
#define DIALOG_GEN_FUNC_COL_ALIGN_FRAME_LABEL DIALOG_SET_SCALE_S_ALIGN_FRAME_TITLE
#define DIALOG_GEN_FUNC_ROW_ALIGN_FRAME_LABEL DIALOG_SET_SCALE_T_ALIGN_FRAME_TITLE
#define DIALOG_GEN_FUNC_REF_ROW_FRAME_LABEL DIALOG_SET_SCALE_S_REF_ROW_OPT_LABEL " (for distances)"
#define DIALOG_GEN_FUNC_REF_COL_FRAME_LABEL DIALOG_SET_SCALE_T_REF_COL_OPT_LABEL " (for distances)"
#define DIALOG_GEN_FUNC_REF_TOTAL_OPT_LABEL DIALOG_SET_SCALE_REF_TOTAL_OPT_LABEL
//@}

#endif // #if !defined(INCLUDED_PLUGINUIMESSAGES_H)
