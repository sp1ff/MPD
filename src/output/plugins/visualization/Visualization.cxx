/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Visualization.hxx"

#include "Log.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/Domain.hxx"

#include <unistd.h> // for gettid()

const Domain vis_output_domain("vis_output_state");

Visualization::Visualization(const ConfigBlock &/*config_block*/)
{ }

void Visualization::Close()
{
	ptimer = 0;
}

void Visualization::Open(AudioFormat &audio_format)
{
	ptimer.reset(new Timer(audio_format));
}

size_t Visualization::Play(const void*, size_t size)
{
	static size_t num_calls = 0;

	if (!ptimer->IsStarted()) {
		ptimer->Start();
	}

	ptimer->Add(size);


	// TODO(sp1ff): HACK: just trying to gauge how fast we're getting data from
	// the audio source
	++num_calls;
	if (0 == (num_calls % 500)) {
		FmtNotice(vis_output_domain, "VisualizationOutput::Play: tid: {}, "
				  "lead: {}sec", gettid(),
				  std::chrono::duration<double>(ptimer->GetDelay()).count());
	}

	return size;
}

