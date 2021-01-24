/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#if !defined( INCLUDED_BITFLAGS_H )
#define INCLUDED_BITFLAGS_H

#include <type_traits>

template <typename T, typename D>
class BitFlags
{
public:
	static_assert( std::is_integral<T>::value, "Integer wrapper" );

	constexpr BitFlags( T flags = 0 ) noexcept
		: flags_( flags )
	{
	}

	constexpr explicit operator T() const
	{
		return flags_;
	}

	constexpr operator D() const
	{
		return D{ flags_ };
	}

	constexpr BitFlags& operator|=( BitFlags flags )
	{
		flags_ |= flags.flags_;
		return *this;
	}

	constexpr BitFlags& operator&=( BitFlags flags )
	{
		flags_ &= flags.flags_;
		return *this;
	}

	constexpr BitFlags operator~() const
	{
		return BitFlags( ~flags_ );
	}

	constexpr explicit operator bool() const
	{
		return flags_;
	}

	friend constexpr BitFlags operator|( BitFlags flags1, BitFlags flags2 )
	{
		return BitFlags( flags1.flags_ | flags2.flags_ );
	}

	friend constexpr BitFlags operator&( BitFlags flags1, BitFlags flags2 )
	{
		return BitFlags( flags1.flags_ & flags2.flags_ );
	}

private:
	T flags_ = 0;
};

#endif
