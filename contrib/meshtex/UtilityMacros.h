/**
 * @file UtilityMacros.h
 * Basic macros.
 * @ingroup generic-util
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

#if !defined(INCLUDED_UTILITYMACROS_H)
#define INCLUDED_UTILITYMACROS_H

/**
 * Convert a token to a string at compile-time.
 *
 * @param A The token to stringify.
 */
#define STRINGIFY(A) #A

/**
 * Convert a macro's value to a string at compile-time.
 *
 * @param A The name of the macro to process.
 */
#define STRINGIFY_MACRO(A) STRINGIFY(A)

#endif // #if !defined(INCLUDED_UTILITYMACROS_H)
