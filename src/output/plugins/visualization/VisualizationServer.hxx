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

#ifndef VISUALIZATION_SERVER_HXX_INCLUDED
#define VISUALIZATION_SERVER_HXX_INCLUDED 1

#include "event/CoarseTimerEvent.hxx"
#include "event/ServerSocket.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

/**
 * \class VisualizationServer
 *
 * \brief Socket server handling visualization clients
 *
 * \sa \ref vis_out_plugin_arch "Architecture"
 * 
 *
 */

#include "VisualizationClient.hxx"

#include "config/Block.hxx"
#include "config/Net.hxx"

#include <memory>

class Visualization;

// TODO(sp1ff): what's the story with all these classes declaring themselves "final"?
// TODO(sp1ff): `HttpdOutput` & `SnapcastOutput` subclass privately-- why?
// TODO(sp1ff): setup a timer, or other mechanism, to sweep our dead clients periodically
class VisualizationServer: public ServerSocket {

	/// maxium number of clients permitted; zero => unlimited
	size_t max_clients;
	// TODO(sp1ff): the authors *really* seem to like intrusive linked lists
	// with manual memory management... which seems nuts to me.
	std::list<std::unique_ptr<VisualizationClient>> clients;
	/// non-owning back-reference to a Visualization instance; caller is
	/// responsible for keeping it valid for the liftime of this server
	Visualization &visualizer;
	/// invoked periodically to clean-up dead clients
	CoarseTimerEvent reaper;

public:
	VisualizationServer(EventLoop &event_loop, const ConfigBlock &config_block,
						Visualization &vis);
	// TODO(sp1ff): dtor-- close all clients
	virtual ~VisualizationServer();

	void ReapClients() noexcept;

protected:
	// Invoked by `ServerSocket`, on its event loop, when a new client connects
    virtual void OnAccept(UniqueSocketDescriptor fd, SocketAddress address, int uid) noexcept override;

};

#endif // VISUALIZATION_SERVER_HXX_INCLUDED
