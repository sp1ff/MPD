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

#include "VisualizationClient.hxx"

#include "Log.hxx"
#include "net/SocketError.hxx"
#include "util/Domain.hxx"

const Domain d_vis_client("vis_client");

VisualizationClient::VisualizationClient(UniqueSocketDescriptor fd, EventLoop &event_loop,
										 Visualization &vis):
	BufferedSocket(fd.Release(), event_loop),
	visualizer(vis),
	closed(false)
{
	FmtInfo(d_vis_client, "VisualizationClient::VisualizationClient({})", gettid());
}

VisualizationClient::~VisualizationClient()
{
	if (!closed) {
		BufferedSocket::Close();
		closed = true;
	}
}

void VisualizationClient::OnSocketReady(unsigned flags) noexcept
{
	FmtInfo(d_vis_client, "VisualizationClient::OnSocketReady({}, {}, {})",
			gettid(), buffer.size(), flags);

	// If we were invoked because a read became signalled, we delegate to the
	// `BufferedSocket` implementation.
	if (0 != (flags & SocketEvent::READ)) {
		BufferedSocket::OnSocketReady(flags);
	} else if (0 != (flags & SocketEvent::WRITE)){
		// We were called because we're ready to write.
		LogInfo(d_vis_client, "Echoing current input back to client...");
		ssize_t nbytes = GetSocket().Write(&buffer[0], buffer.size());
		if (nbytes < 0) {
			auto e = GetSocketError();
			if (IsSocketErrorSendWouldBlock(e)) {
				LogNotice(d_vis_client, "OnSocketReady invoked, but write would block(!)");
				event.ScheduleWrite();
				return;
			} else if (!IsSocketErrorClosed(e)) {
				SocketErrorMessage msg(e);
				FmtWarning(d_vis_client, "Failed to write to client: {}",
						   (const char *)msg);
			}
			closed = true;
			Close();
			return;
		}
		buffer.erase(buffer.begin(), buffer.end());
		event.CancelWrite();
		LogInfo(d_vis_client, "Echoing current input back to client...done.");
	} else if (0 != (flags & SocketEvent::HANGUP)){
		LogNotice(d_vis_client, "Client went away!");
		closed = true;
		event.CancelRead();
		event.CancelWrite();
		Close();
		return;
	} else {
		FmtNotice(d_vis_client, "Got flags {} (!)", flags);
	}
}

BufferedSocket::InputResult
VisualizationClient::OnSocketInput(void *buf, size_t length) noexcept
{
	FmtInfo(d_vis_client, "VisualizationClient::OnSocketInput({}, {}, {} bytes)",
			gettid(), buffer.size(), length);

	// We have data available to be read, and it's present in `buf`. Copy it to
	// our `buffer`...
	std::copy((const uint8_t*) buf,
			  (const uint8_t*) buf + length,
			  std::back_inserter(buffer));
	ConsumeInput(length);
	// schedule a write...
	event.ScheduleWrite();
	// and indicate to `BufferedSocket` that we're done reading for the moment.
	// Reading the code, it seems a read *will* be scheduled, so I'm not sure on
	// the difference between returning PAUSE & MORE, here (a second call to
	// `Cancel()` will be issued in the latter case, but it's not clear to me
	// that has any effect).
	return InputResult::PAUSE;
}

void VisualizationClient::OnSocketError(std::exception_ptr) noexcept
{
	LogInfo(d_vis_client, "VisualizationClient::OnSocketError");
}

void VisualizationClient::OnSocketClosed() noexcept
{
	LogInfo(d_vis_client, "VisualizationClient::OnSocketClosed");
	event.CancelRead();
	event.CancelWrite();
	closed = true;
	Close();
}


