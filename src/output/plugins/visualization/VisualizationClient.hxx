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

#ifndef VISUALIZATION_CLIENT_HXX_INCLUDED
#define VISUALIZATION_CLIENT_HXX_INCLUDED 1

#include "event/BufferedSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

class Visualization;

/**
 * \class VisualizationClient
 *
 * \brief Represents a TCP connection to one visualization client
 *
 *
 */

// TODO(sp1ff): what about metadata?
// TODO(sp1ff): what about versioning the protocol?

class VisualizationClient: BufferedSocket {

	/// non-owning reference to a Visualization instance; caller is responsible
	/// for keeping it valid for the lifetime of this client instance
	Visualization &visualizer;
	/// true => this socket is closed & this instance may be reaped
	bool closed;

	// TODO(sp1ff): implement a simple echo protocol

	/// empty => we're expecting input from our client, non-empty => we have
	/// data to echo
	std::vector<uint8_t> buffer;

	
public:
	VisualizationClient(UniqueSocketDescriptor fd, EventLoop &event_loop,
						Visualization &vis);
	virtual ~VisualizationClient();
	bool IsClosed() const
	{ return closed; }


protected:

	////////////////////////////////////////////////////////////////////////////
	//						BufferedSocket interface                          //
	////////////////////////////////////////////////////////////////////////////

	// TODO(sp1ff): Having a lot of trouble getting my head around this
	// interface; keeping notes here, for now.

	// 
	virtual BufferedSocket::InputResult
	OnSocketInput(void *data, size_t length) noexcept override;


	virtual void OnSocketReady(unsigned flags) noexcept override;
	virtual void OnSocketError(std::exception_ptr ep) noexcept override;
	virtual void OnSocketClosed() noexcept override;


};

#endif // VISUALIZATION_CLIENT_HXX_INCLUDED
