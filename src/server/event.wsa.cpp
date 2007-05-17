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

#include "common.h"

#include "event.h"

#ifndef EV_WSA
	#error EV_WSA not defined
#endif

#include <iostream>
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

const Event::ev_t
	Event::connect = FD_CONNECT,
	Event::accept = FD_ACCEPT,
	Event::read = FD_READ,
	Event::write = FD_WRITE,
	Event::hangup = FD_CLOSE;

Event::Event() throw()
	: last_event(0),
	_error(0),
	nfds(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa)()" << endl
		<< "Max events: " << max_events << endl
		<< "FD_MAX_EVENTS: " << FD_MAX_EVENTS << endl;
	#endif
	
	for (uint i=0; i != max_events; i++)
		w_ev[i] = WSA_INVALID_EVENT;
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~Event(wsa)()" << endl;
	#endif
}

bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa).init()" << endl;
	#endif
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event::finish()" << endl;
	#endif
}

// Errors: ENOMEM, WSAENETDOWN, WSAEINPROGRESS
int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa).wait()" << endl;
	#endif
	
	assert(fd_to_ev.size() != 0);
	
	/* params: event_count, event_array, wait_for_all, timeout, alertable */
	nfds = WSAWaitForMultipleEvents(fd_to_ev.size(), w_ev, false, _timeout, true);
	if (nfds == WSA_WAIT_FAILED)
	{
		_error = WSAGetLastError();
		
		assert(_error != WSANOTINITIALISED);
		assert(_error != WSA_INVALID_HANDLE);
		assert(_error != WSA_INVALID_PARAMETER);
		
		if (_error == WSA_NOT_ENOUGH_MEMORY)
			_error = ENOMEM;
		
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
int Event::add(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa).add(fd: " << fd << ")" << endl;
	#endif
	
	assert( fd >= 0 );
	
	fSet(events, Event::hangup);
	
	for (uint i=0; i != max_events; ++i)
	{
		if (w_ev[i] == WSA_INVALID_EVENT)
		{
			w_ev[i] = WSACreateEvent();
			if (!(w_ev[i])) return false;
			
			const int r = WSAEventSelect(fd, w_ev[i], events);
			
			if (r == SOCKET_ERROR)
			{
				_error = WSAGetLastError();
				
				assert(_error != WSAENOTSOCK);
				assert(_error != WSAEINVAL);
				assert(_error != WSANOTINITIALISED);
				
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
	
	cerr << "Event system overloaded!" << endl;
	
	return false;
}

// Errors: WSAENETDOWN
int Event::modify(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa).modify(fd: " << fd << ")" << endl;
	#endif
	
	#ifndef NDEBUG
	cout << ": Setting events: " << events << ", for FD: " << fd << endl;
	
	cout << " # ";
	bool next=false;
	if (fIsSet(events, Event::read))
	{
		cout << "read"; next = true;
	}
	if (fIsSet(events, Event::write))
	{
		if (next) cout << ", ";
		cout << "write"; next = true;
	}
	if (fIsSet(events, Event::accept))
	{
		if (next) cout << ", ";
		cout << "accept"; next = true;
	}
	if (fIsSet(events, Event::hangup))
	{
		if (next) cout << ", ";
		cout << "close";
	}
	cout << endl;
	#endif
	
	assert( fd >= 0 );
	
	fSet(events, Event::hangup);
	
	const ev_iter fi(fd_to_ev.find(fd));
	assert(fi != fd_to_ev.end());
	assert(fi->second < max_events);
	
	const int r = WSAEventSelect(fd, w_ev[fi->second], events);
	
	if (r == SOCKET_ERROR)
	{
		_error = WSAGetLastError();
		
		assert(_error != WSAENOTSOCK);
		assert(_error != WSAEINVAL);
		assert(_error != WSANOTINITIALISED);
		
		return false;
	}
	
	return 0;
}

int Event::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(wsa).remove(fd: " << fd << ")" << endl;
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

bool Event::getEvent(fd_t &fd, ev_t &events) throw()
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
				_error = WSAGetLastError();
				
				switch (_error)
				{
				case WSAECONNRESET: // reset by remote
				case WSAECONNABORTED: // connection aborted
				case WSAETIMEDOUT: // connection timed-out
				case WSAENETUNREACH: // network unreachable
				case WSAECONNREFUSED: // connection refused/rejected
					events = Event::hangup;
					break;
				case WSAENOBUFS: // out of network buffers
				case WSAENETDOWN: // network sub-system failure
				case WSAEINPROGRESS: // something's in progress
					goto loopend;
				default:
					cerr << "Event(wsa).getEvent() - unknown error: " << _error << endl;
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
				if (fIsSet(events, Event::read))
				{
					cout << "read"; next = true;
				}
				if (fIsSet(events, Event::write))
				{
					if (next) cout << ", ";
					cout << "write"; next = true;
				}
				if (fIsSet(events, Event::accept))
				{
					if (next) cout << ", ";
					cout << "accept"; next = true;
				}
				if (fIsSet(events, Event::hangup))
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
