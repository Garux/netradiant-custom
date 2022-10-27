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

#pragma once

#include <chrono>


class Timer
{
	std::chrono::time_point<std::chrono::steady_clock> m_start;
public:
	Timer(){
		start();
	}
	void start(){
		m_start = std::chrono::steady_clock::now();
	}
	int elapsed_msec() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - m_start ).count();
	}
	double elapsed_sec() const {
		return std::chrono::duration<double>( std::chrono::steady_clock::now() - m_start ).count();
	}
};



class DoubleClickTimer
{
	Timer m_timer;
	bool m_fired{};
public:
	void click(){
		m_fired = m_timer.elapsed_msec() < 250;
		m_timer.start();
	}
	bool fired() const {
		return m_fired;
	}
};
