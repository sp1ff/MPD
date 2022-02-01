/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef VISUALIZATION_HXX_INCLUDED
#define VISUALIZATION_HXX_INCLUDED 1

#include "config/Block.hxx"
#include "output/Timer.hxx"
#include "thread/Mutex.hxx"

#include <memory>

/**
 *
 */

class Visualization {
	// TODO(sp1ff): currently used to track how much data we've accumulated
	std::unique_ptr<Timer> ptimer;
	/// mutex protecting state that can be accessed by either thread
	mutable Mutex mutex;

public:
	Visualization(const ConfigBlock &config_block);


public:
	void Close();
	void Open(AudioFormat &audio_format);
	size_t Play(const void*, size_t size);
};

#endif // VISUALIZATION_HXX_INCLUDED
