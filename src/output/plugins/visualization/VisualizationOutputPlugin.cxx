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

#include "VisualizationOutputPlugin.hxx"
#include "VisualizationServer.hxx"
#include "Visualization.hxx"

/**
 * \page vis_out_plugin_arch Layout of the Visualization Output Plugin
 *
 * \section vis_out_plugin_arch_intro Introduction
 *
 * There are, at the time of this writing, two other output plugins that provide
 * socket servers: HttpdOutput & SnapcastOutput. They both follow a similar
 * pattern in which the plugin subclasses both AudioOutput \e and
 * ServerSocket. Since I have chosen a different approach, I should both
 * describe the layout of VisualizationOutput and explain my choice.
 *
 * \section vis_out_plugin_arch_cyclic Cyclic Dependencies
 *
 * While they subclass privately (implying an "implemented-in-terms-of" rather
 * than "is-a" relationship with their superclasses), HttpdOutput &
 * SnapcastOutput in practice handle the duties of being both an AudioOutput and
 * a ServerSocket. This introduces not one but two cyclic dependencies in their
 * implementations:
 *
 *     1. the ServerSocket half of them is responsible for creating new clients,
 *     but the clients are the ones who detect that their socket has been
 *     closed; they then need a back-reference to signal their parent that they
 *     should be destroyed (by calling RemoveClient() through their
 *     back-reference).
 *
 *     2. the AudioOutput half of them is responsible for pushing new data
 *     derived from PCM data out to all their clients, while their clients
 *     request information & service from their parent, again requiring a back
 *     reference (GetCodecName() on the Snapcast client, e.g.)
 *
 * Cyclic dependencies carry with them drawbacks:
 *
 *     - they increase compilation times because when one file in the cycle is
 *       changed, all the other translation units need to be recompiled
 *
 *     - they increase coupling, increasing the chances that a change in
 *       one place will break others
 *
 *     - code reuse becomes more difficult-- trying to hoist one file out involves
 *       bring all the other files in the cycle along with it
 *
 *     - unit testing becomes harder-- the smallest unit of testable
 *       funcationality becomes the union all the the translation units in the
 *       cycle
 *
 * \section vis_out_plugin_arch_threads Too Many Threads!
 *
 * This arrangement entails another problem: HttpdOutput & SnapcastOutput
 * instances have their methods invoked on two threads; the main I/O thread as
 * well as the player control thread. This means that access to some state needs
 * to be guarded by a mutex (in the case of HttpdOutput, the client list & the
 * pages), but \e not others (again in the case of HttpdOutput, content or
 * genre).
 *
 * \section vis_out_plugin_arch_demotion Breaking Dependency Cyles Through Demotion
 *
 * This arrangement breaks things up in a few ways.
 *
 * Cycle 1 is broken up by having a one-way relationship only between the socker
 * server & clients. When a client detects that its socket has been closed, it
 * marks itself "dead" and will eventually be swept by the server.
 *
 * Cycle 2 is broken by Lakos' method of demotion: the responsibility required
 * by both the output plugin & the various clients is pushed down into a
 * separate class VisualizationOutput. It is owned by the plugin, and referenced
 * by clients. When the plugin is disa bled, the plugin is responsible for
 * cleaning-up the server, which will in turn clean-up all the clients, and only
 * then destroying the VisualizationOutput instance.
 *
 * In ASCII art:
 *
 \verbatim
    sound       +---------------------+               +---------------------+
 -- data ---->  | VisualizationOutput | --- owns ---> | VisualizationServer |
                +---------------------+               +---------------------+
                | Play()              |               | OnAccept()          |
                +---------------------+               +---------------------+
                         1 |                                     | 1
                           |                         +---owns----+
                           |                         |
                           |                         v *
                           |               +---------------------+
                          owns             | VisualizationClient |
                           |               +---------------------+
                           |                         | *
                           |    +----references------+                     
                           |    |
						 1 v    v 1
                	+---------------+
                	| Visualization |
                	+---------------+
 \endverbatim
 *
 * This arrangement also addresses the threading issue: other than creation &
 * destruction, the socket server has all of its methods invoked on the I/O
 * thread, and the plugin on the player control thread. The state that needs to
 * be guarded against access from multiple threads is localized in
 * VisualizationOutput.
 *
 *
 */

#include "Log.hxx"
#include "config/Block.hxx"
#include "event/Call.hxx"
#include "output/Interface.hxx"
#include "output/OutputPlugin.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/Domain.hxx"

/**
 * \class VisualizationOutput
 *
 * \brief An output plugin that serves data useful for visualizers
 *
 * \sa \ref vis_out_plugin_arch "Architecture"
 *
 *
 * Both the fifo & pipe output plugins can be used to directly access the PCM
 * audio data, and so can (and have been) used to implement music visualizers
 * for MPD. They are, however, limited to clients running on the same host as
 * MPD. This output plugin will stream the PCM data along with derived
 * information useful for visualizers (the Fourier transform, bass/mids/trebs,
 * perhaps beat detection) over one or more network connections, to allow true
 * MPD client visualizers.
 *
 *
 */

// TODO(sp1ff): what's the story with all these classes declaring themselves "final"?
// TODO(sp1ff): `HttpdOutput` & `SnapcastOutput` subclass privately-- why?
class VisualizationOutput: public AudioOutput {

	std::unique_ptr<Visualization> pvis;
	std::unique_ptr<VisualizationServer> pserver;

public:
	static AudioOutput* Create(EventLoop &event_loop, const ConfigBlock &cfg_block) {
		return new VisualizationOutput(event_loop, cfg_block);
	}
	VisualizationOutput(EventLoop &event_loop, const ConfigBlock &cfg_block);
	// TODO(sp1ff): dtor? Copy semantics (I suppose not, but in that case they should be explicitly disabled)?
	virtual ~VisualizationOutput(); // We have virtuals, so...

public:

	////////////////////////////////////////////////////////////////////////////
	//						 AudioOutput Interface                            //
	////////////////////////////////////////////////////////////////////////////

	// not required, tho I wonder what happens if I set FLAG_ENABLE_DISABLE and don't provide it
	void Enable() override;
	// not required, tho I wonder what happens if I set FLAG_ENABLE_DISABLE and don't provide it
	void Disable() noexcept override;
	// required
	void Open(AudioFormat &audio_format) override;
	// required
	void Close() noexcept override;
	// TODO(sp1ff): do I support?
	// bool ChangeAudioFormat(AudioFormat &);
	// void Interrupt() noexcept;
	// std::chrono::steady_clock::duration Delay() const noexcept;
	// void SendTag(const Tag &);
	// required
	size_t Play(const void *chunk, size_t size) override;
	// TODO(sp1ff): do I support?
	// void Drain();
	// void Cancel();
	// bool Pause();

};

const Domain vis_output_domain("vis_output");

VisualizationOutput::VisualizationOutput(EventLoop &event_loop, const ConfigBlock &config_block):
	AudioOutput(FLAG_ENABLE_DISABLE|FLAG_PAUSE),
	pvis(new Visualization(config_block)),
	pserver(new VisualizationServer(event_loop, config_block, *pvis))
{ }

VisualizationOutput::~VisualizationOutput()
{
	// Ensure that the server is destroyed before the shared state...
	pserver = 0;
	pvis = 0;
}

void VisualizationOutput::Enable()
{
	FmtNotice(vis_output_domain, "VisualizationOutput::Enable({})", gettid());

	// The `SockerServer` part of us runs on a different event loop than the one
	// we're being invoked on; this is a little syntactic sugar that arranges to
	// run `Open()` on that loop, instead.
	BlockingCall(pserver->GetEventLoop(), [this]() { pserver->Open(); });
}

void VisualizationOutput::Disable() noexcept
{
	FmtNotice(vis_output_domain, "VisualizationOutput::Disable({})", gettid());

	// The `SockerServer` part of us runs on a different event loop than the one
	// we're being invoked on; this is a little syntactic sugar that arranges to
	// run `Open()` on that loop, instead.
	BlockingCall(pserver->GetEventLoop(), [this]() { pserver->Close(); });
}

void VisualizationOutput::Open(AudioFormat &audio_format)
{
	FmtNotice(vis_output_domain, "VisualizationOutput::Open({})", gettid());
	pvis->Open(audio_format);
}

void VisualizationOutput::Close() noexcept
{
	FmtNotice(vis_output_domain, "VisualizationOutput::Close({})", gettid());
	pvis->Close();
}

size_t VisualizationOutput::Play(const void *data, size_t size)
{
	return pvis->Play(data, size);
}

const struct AudioOutputPlugin visualization_output_plugin = {
	"visualization",
	nullptr, // cannot serve as the default output
	&VisualizationOutput::Create,
	nullptr, // no particular mixer
};
