/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior writeitten authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "wsa.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <cassert> // assert()

template <> const bool event_has_hangup<EventWSA>::value = true;
template <> const bool event_has_connect<EventWSA>::value = true;
template <> const bool event_has_accept<EventWSA>::value = true;
template <> const long event_read<EventWSA>::value = FD_READ;
template <> const long event_write<EventWSA>::value = FD_WRITE;
template <> const long event_hangup<EventWSA>::value = FD_CLOSE;
template <> const long event_accept<EventWSA>::value = FD_ACCEPT;
template <> const long event_connect<EventWSA>::value = FD_CONNECT;
template <> const std::string event_system<EventWSA>::value("wsa");

EventWSA::EventWSA() throw()
	: nfds(0), last_event(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsa()" << endl
		<< "Max events: " << max_events << endl
		<< "FD_MAX_EVENTS: " << FD_MAX_EVENTS << endl;
	#endif
	
	for (uint i=0; i != max_events; i++)
		w_ev[i] = WSA_INVALID_EVENT;
}

EventWSA::~EventWSA() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~wsa()" << endl;
	#endif
}

// Errors: ENOMEM, WSAENETDOWN, WSAEINPROGRESS
int EventWSA::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsa.wait()" << endl;
	#endif
	
	assert(fd_to_ev.size() != 0);
	
	/* params: event_count, event_array, wait_for_all, timeout, alertable */
	nfds = WSAWaitForMultipleEvents(fd_to_ev.size(), w_ev, false, _timeout, true);
	if (nfds == WSA_WAIT_FAILED)
	{
		error = WSAGetLastError();
		
		assert(error != WSANOTINITIALISED);
		assert(error != WSA_INVALID_HANDLE);
		assert(error != WSA_INVALID_PARAMETER);
		
		if (error == WSA_NOT_ENOUGH_MEMORY)
			error = ENOMEM;
		
		return -1;
	}
	else
	{
		switch (nfds)
		{
		case WSA_WAIT_IO_COMPLETION:
		case WSA_WAIT_TIMEOUT:
			return 0;
		default:
			return 1;
		}
	}
}

// Errors: WSAENETDOWN
int EventWSA::add(fd_t fd, long events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsa.add(fd: " << fd << ")" << endl;
	#endif
	
	assert( fd >= 0 );
	
	fSet(events, event_hangup<EventWSA>::value);
	
	for (uint i=0; i != max_events; ++i)
	{
		if (w_ev[i] == WSA_INVALID_EVENT)
		{
			w_ev[i] = WSACreateEvent();
			if (!(w_ev[i])) return false;
			
			const int r = WSAEventSelect(fd, w_ev[i], events);
			
			if (r == SOCKET_ERROR)
			{
				error = WSAGetLastError();
				
				assert(error != WSAENOTSOCK);
				assert(error != WSAEINVAL);
				assert(error != WSANOTINITIALISED);
				
				return false;
			}
			
			// update ev_to_fd and fd_to_ev maps
			fd_to_ev[fd] = i;
			ev_to_fd[i] = fd;
			
			if (i > last_event)
				last_event = i;
			
			return true;
		}
	}
	
	#ifndef NDEBUG
	cerr << "Event system overloaded!" << endl;
	#endif
	
	return false;
}

// Errors: WSAENETDOWN
int EventWSA::modify(fd_t fd, long events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsa.modify(fd: " << fd << ")" << endl;
	#endif
	
	#ifndef NDEBUG
	cout << ": Setting events: " << events << ", for FD: " << fd << endl;
	
	cout << " # ";
	bool next=false;
	if (fIsSet(events, event_read<EventWSA>::value))
	{
		cout << "read"; next = true;
	}
	if (fIsSet(events, event_write<EventWSA>::value))
	{
		if (next) cout << ", ";
		cout << "write"; next = true;
	}
	if (fIsSet(events, event_accept<EventWSA>::value))
	{
		if (next) cout << ", ";
		cout << "accept"; next = true;
	}
	if (fIsSet(events, event_hangup<EventWSA>::value))
	{
		if (next) cout << ", ";
		cout << "close";
	}
	cout << endl;
	#endif
	
	assert( fd >= 0 );
	
	fSet(events, event_hangup<EventWSA>::value);
	
	const ev_iter fi(fd_to_ev.find(fd));
	assert(fi != fd_to_ev.end());
	assert(fi->second < max_events);
	
	const int r = WSAEventSelect(fd, w_ev[fi->second], events);
	
	if (r == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		
		assert(error != WSAENOTSOCK);
		assert(error != WSAEINVAL);
		assert(error != WSANOTINITIALISED);
		
		return false;
	}
	
	return 0;
}

int EventWSA::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsa.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert( fd >= 0 );
	
	const ev_iter fi(fd_to_ev.find(fd));
	assert(fi != fd_to_ev.end());
	
	assert(fi->second < max_events);
	WSACloseEvent(w_ev[fi->second]);
	
	w_ev[fi->second] = WSA_INVALID_EVENT;
	
	ev_to_fd.erase(fi->second);
	fd_to_ev.erase(fd);
	
	// find last event
	for (; w_ev[last_event] == WSA_INVALID_EVENT; --last_event)
		if (last_event == 0) break;
	
	return true;
}

bool EventWSA::getEvent(fd_t &fd, long &events) throw()
{
	uint get_event = nfds - WSA_WAIT_EVENT_0;
	WSANETWORKEVENTS set;
	
	#ifndef NDEBUG
	cout << "~ Getting events, start offset: " << get_event << endl;
	#endif
	
	r_ev_iter fd_iter;
	
	for (; get_event <= last_event;)
	{
		if (w_ev[get_event] != WSA_INVALID_EVENT)
		{
			fd_iter = ev_to_fd.find(get_event);
			assert(fd_iter != ev_to_fd.end());
			fd = fd_iter->second;
			
			#ifndef NDEBUG
			cout << "~ Checking FD: " << fd << endl;
			#endif
			
			const int r = WSAEnumNetworkEvents(fd, w_ev[get_event], &set);
			
			if (r == SOCKET_ERROR)
			{
				error = WSAGetLastError();
				
				switch (error)
				{
				case WSAECONNRESET: // reset by remote
				case WSAECONNABORTED: // connection aborted
				case WSAETIMEDOUT: // connection timed-out
				case WSAENETUNREACH: // network unreachable
				case WSAECONNREFUSED: // connection refused/rejected
					events = event_hangup<EventWSA>::value;
					break;
				case WSAENOBUFS: // out of network buffers
				case WSAENETDOWN: // network sub-system failure
				case WSAEINPROGRESS: // something's in progress
					goto loopend;
				default:
					#ifndef NDEBUG
					cerr << "Event(wsa).getEvent() - unknown error: " << error << endl;
					#endif
					goto loopend;
				}
			}
			else
				events = set.lNetworkEvents;
			
			if (events != 0)
			{
				#ifndef NDEBUG
				cout << "+ Events triggered: " << events << ", for FD: " << fd << endl;
				
				cout << " # ";
				bool next=false;
				if (fIsSet(events, event_read<EventWSA>::value))
				{
					cout << "read"; next = true;
				}
				if (fIsSet(events, event_write<EventWSA>::value))
				{
					if (next) cout << ", ";
					cout << "write"; next = true;
				}
				if (fIsSet(events, event_accept<EventWSA>::value))
				{
					if (next) cout << ", ";
					cout << "accept"; next = true;
				}
				if (fIsSet(events, event_hangup<EventWSA>::value))
				{
					if (next) cout << ", ";
					cout << "close";
				}
				cout << endl;
				#endif
				
				++nfds;
				++get_event;
				
				return true;
			}
		}
		
		loopend:
		++get_event;
	}
	
	#ifndef NDEBUG
	cerr << "- No events triggered!" << endl;
	#endif
	
	return false;
}

void EventWSA::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "WSA.timeout(msecs: " << msecs << ")" << std::endl;
	#endif
	
	_timeout = msecs;
}
