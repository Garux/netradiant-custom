/**
 * @file MeshEntityMessages.h
 * String constants for messages shown in dialogs.
 * @ingroup meshtex-core
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

#if !defined(INCLUDED_MESHENTITYMESSAGES_H)
#define INCLUDED_MESHENTITYMESSAGES_H

/// @name Informational messages
//@{
#define INFO_ROW_FORMAT "On row %d:\n  S natural scale: %f\n  S tiles: %f\n  S min: %f\n  S max: %f\n\n"
#define INFO_ROW_INFSCALE_FORMAT "On row %d:\n  S scale: (infinite)\n  S tiles: %f\n  S min: %f\n  S max: %f\n\n"
#define INFO_COL_FORMAT "On column %d:\n  T natural scale: %f\n  T tiles: %f\n  T min: %f\n  T max: %f\n\n"
#define INFO_COL_INFSCALE_FORMAT "On column %d:\n  T scale: (infinite)\n  T tiles: %f\n  T min: %f\n  T max: %f\n\n"
#define INFO_MESH_FORMAT "Over entire mesh:\n  Rows: %d\n  S min: %f\n  S max: %f\n  Columns: %d\n  T min: %f\n  T max: %f\n\nWorldspace extents: (%.2f/%.2f,%.2f/%.2f,%.2f/%.2f)"
//@}

/// @name Warning messages
//@{
#define WARNING_ROW_INFSCALE "The selected reference row has an infinite scale (S does not vary along the row). The row information cannot be automatically transferred to the Set S/T Scale dialog."
#define WARNING_COL_INFSCALE "The selected reference column has an infinite scale (T does not vary along the column). The column information cannot be automatically transferred to the Set S/T Scale dialog."
//@}

/// @name Error messages
//@{
#define ERROR_BAD_MESH "Unable to read patch mesh."
#define ERROR_BAD_ROW "Illegal row # specified."
#define ERROR_BAD_COL "Illegal column # specified."
#define ERROR_ROW_ZEROSCALE "A scale of zero cannot be applied; the S values of the row will not be changed."
#define ERROR_COL_ZEROSCALE "A scale of zero cannot be applied; the T values of the column will not be changed."
#define ERROR_ROW_ZEROTILES "A tile count of zero cannot be applied; the S values of the row will not be changed."
#define ERROR_COL_ZEROTILES "A tile count of zero cannot be applied; the T values of the column will not be changed."
//@}

#endif // #if !defined(INCLUDED_MESHENTITYMESSAGES_H)