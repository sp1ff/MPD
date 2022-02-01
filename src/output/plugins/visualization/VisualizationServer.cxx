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

#include "VisualizationServer.hxx"
#include "Visualization.hxx"

#include "Log.hxx"
#include "util/Domain.hxx"

const Domain vis_output_domain("vis_output_server");

VisualizationServer::VisualizationServer(EventLoop &event_loop, const ConfigBlock &config_block, Visualization &vis):
	ServerSocket(event_loop),
	max_clients(config_block.GetPositiveValue("max_clients", 0)),
	visualizer(vis),
	reaper(event_loop, BIND_THIS_METHOD(ReapClients))
{
	ServerSocketAddGeneric(*this, config_block.GetBlockValue("bind_to_address"),
						   config_block.GetBlockValue("port", 8001U));

}

VisualizationServer::~VisualizationServer()
{
	FmtNotice(vis_output_domain, "Shutting down visualization server "
			  "({}, {} clients)", gettid(), clients.size());
	// child dtors will close sockets
}

void VisualizationServer::ReapClients() noexcept
{
	FmtNotice(vis_output_domain, "VisualizationServer::ReapClients({}, "
			  "{} clients)", gettid(), clients.size());

	for (auto p0 = clients.begin(), p1 = clients.end(); p0 != p1; ) {
		auto p = p0++;
		if ((*p)->IsClosed()) {
			LogInfo(vis_output_domain, "Reaping closed client.");
			clients.erase(p);
		}
	}

	if (!clients.empty()) {
		LogInfo(vis_output_domain, "Scheduling another reaping in 3 seconds.");
		reaper.Schedule(std::chrono::seconds(3));
	}
}

void VisualizationServer::OnAccept(UniqueSocketDescriptor fd, SocketAddress /*address*/, int) noexcept
{
	FmtNotice(vis_output_domain, "VisualizationServer::OnAccept({})", gettid());

	/* can we allow additional client */
	if (max_clients && clients.size() > max_clients) {
		FmtError(vis_output_domain, "Rejecting connection request; the maximum "
				 "number of clients ({}) has already been reached.", max_clients);
	} else {
		clients.push_back(std::make_unique<VisualizationClient>(std::move(fd), GetEventLoop(), visualizer));
		reaper.Schedule(std::chrono::seconds(3));
	}
}

