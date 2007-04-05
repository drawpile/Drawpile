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

//#include "../shared/templates.h"
#include "event.h"

#ifndef NDEBUG
	#include <iostream>
	#include <ios>
#endif

#include <time.h>
#include <cerrno> // errno
#include <memory> // memcpy()
#include <cassert> // assert()

namespace hack
{
namespace events
{

// "Hack" for WSA
inline
uint32_t& prepare_events(uint32_t& evs) throw()
{
	if (fIsSet(evs, static_cast<uint32_t>(FD_ACCEPT))
		or fIsSet(evs, static_cast<uint32_t>(FD_CONNECT))
		or fIsSet(evs, static_cast<uint32_t>(FD_CLOSE)))
		fSet(evs, Event::read);
	
	if (fIsSet(evs, Event::read));
		fSet(evs, static_cast<uint32_t>(FD_ACCEPT|FD_CLOSE));
	
	if (fIsSet(evs, Event::read)
		or fIsSet(evs, Event::write));
		fSet(evs, static_cast<uint32_t>(FD_CLOSE|FD_CONNECT));
	
	return evs;
}

} // namespace events
} // namespace hack

const uint32_t
	//! identifier for 'read' event
	Event::read = FD_READ,
	//! identifier for 'write' event
	Event::write = FD_WRITE,
	//! error event
	Event::error = 0, // WSA doesn't have it
	//! hangup event
	Event::hangup = FD_CLOSE;

Event::Event() throw()
	: last_event(0),
	_error(0),
	nfds(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa)()" << std::endl
		<< "Max events: " << max_events << std::endl
		<< "FD_MAX_EVENTS: " << FD_MAX_EVENTS << std::endl;
	#endif
	
	for (unsigned int i=0; i != max_events; i++)
		w_ev[i] = WSA_INVALID_EVENT;
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "~Event(wsa)()" << std::endl;
	#endif
}

bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).init()" << std::endl;
	#endif
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event::finish()" << std::endl;
	#endif
}

int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).wait()" << std::endl;
	#endif
	
	assert(fd_to_ev.size() != 0);
	
	nfds = WSAWaitForMultipleEvents(fd_to_ev.size(), w_ev, false, _timeout, true);
	if (nfds == WSA_WAIT_FAILED)
	{
		_error = WSAGetLastError();
		switch (_error)
		{
		#ifndef NDEBUG
		case WSA_INVALID_HANDLE:
			assert(!(_error == WSA_INVALID_HANDLE));
			break;
		case WSANOTINITIALISED:
			assert(!(_error == WSANOTINITIALISED));
			break;
		case WSA_INVALID_PARAMETER:
			assert(!(_error == WSA_INVALID_PARAMETER));
			break;
		#endif
		case WSAENETDOWN:
		case WSAEINPROGRESS:
		case WSA_NOT_ENOUGH_MEMORY:
			break;
		default:
			std::cerr << "Event(wsa).wait() - unknown error: " << nfds << std::endl;
			break;
		}
		
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
			// events ready
			break;
		}
	}
	
	return fd_to_ev.size();
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	hack::events::prepare_events(ev);
	
	for (uint32_t i=0; i != max_events; ++i)
	{
		if (w_ev[i] == WSA_INVALID_EVENT)
		{
			w_ev[i] = WSACreateEvent();
			if (!(w_ev[i])) return false;
			
			WSAEventSelect(fd, w_ev[i], ev);
			
			// update ev_to_fd and fd_to_ev maps
			fd_to_ev[fd] = i;
			ev_to_fd[i] = fd;
			
			if (i > last_event)
				last_event = i;
			
			return true;
		}
	}
	
	#ifndef NDEBUG
	std::cerr << "Event system overloaded!" << std::endl;
	#endif
	
	return false;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	#ifndef NDEBUG
	std::cout << ": Setting events: " << ev << ", for FD: " << fd << std::endl;
	
	if (fIsSet(ev, static_cast<uint32_t>(FD_READ)))
		std::cout << "   #read:   " << FD_READ << std::endl;
	if (fIsSet(ev, static_cast<uint32_t>(FD_WRITE)))
		std::cout << "   #write:  " << FD_WRITE << std::endl;
	if (fIsSet(ev, static_cast<uint32_t>(FD_ACCEPT)))
		std::cout << "   #accept: " << FD_ACCEPT << std::endl;
	if (fIsSet(ev, static_cast<uint32_t>(FD_CLOSE)))
		std::cout << "   #close:  " << FD_CLOSE << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	hack::events::prepare_events(ev);
	
	const std::map<fd_t, uint32_t>::iterator fi(fd_to_ev.find(fd));
	assert(fi != fd_to_ev.end());
	//if (fi == fd_to_ev.end())
	//	return false;
	
	WSAEventSelect(fd, w_ev[fi->second], ev);
	
	return 0;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	hack::events::prepare_events(ev);
	
	const std::map<fd_t, uint32_t>::iterator fi(fd_to_ev.find(fd));
	assert(fi != fd_to_ev.end());
	
	//WSAEventSelect(fd, w_ev[fi->second], 0);
	WSACloseEvent(w_ev[fi->second]);
	
	w_ev[fi->second] = WSA_INVALID_EVENT;
	
	ev_to_fd.erase(fi->second);
	fd_to_ev.erase(fd);
	
	// find last event
	for (; w_ev[last_event] != WSA_INVALID_EVENT; --last_event);
	
	return true;
}

bool Event::getEvent(fd_t &fd, uint32_t &events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).getEvent()" << std::endl;
	#endif
	
	uint32_t get_event = nfds - WSA_WAIT_EVENT_0;
	WSANETWORKEVENTS set;
	
	#ifndef NDEBUG
	std::cout << "Getting events, offset: " << get_event << std::endl;
	#endif
	
	std::map<uint32_t, fd_t>::iterator fd_iter;
	for (; get_event <= last_event; ++get_event)
	{
		if (w_ev[get_event] != WSA_INVALID_EVENT)
		{
			fd_iter = ev_to_fd.find(get_event);
			assert(fd_iter != ev_to_fd.end());
			fd = fd_iter->second;
			
			#ifndef NDEBUG
			std::cout << "Checking FD: " << fd << std::endl;
			#endif
			
			const int r = WSAEnumNetworkEvents(fd, w_ev[get_event], &set);
			
			if (r == SOCKET_ERROR)
			{
				int error = WSAGetLastError();
				
				switch (error)
				{
				case WSAECONNRESET: // reset by remote
				case WSAECONNABORTED: // connection aborted
				case WSAETIMEDOUT: // connection timed-out
				case WSAENETUNREACH: // network unreachable
				case WSAECONNREFUSED: // connection refused/rejected
					events = FD_WRITE|FD_CLOSE;
					break;
				case WSAENOBUFS: // out of network buffers
				case WSAENETDOWN: // network sub-system failure
				case WSAEINPROGRESS: // something's in progress
					events = 0;
					break;
				default:
					std::cerr << "Event(wsa).getEvents() - unknown error: " << error << std::endl;
					events = 0;
					break;
				}
			}
			else
				events = static_cast<uint32_t>(set.lNetworkEvents);
			
			if (events != 0)
			{
				#ifndef NDEBUG
				std::cout << ": Events triggered: " << events << ", for FD: " << fd << std::endl;
				
				if (fIsSet(events, static_cast<uint32_t>(FD_READ)))
					std::cout << "   #read:   " << FD_READ << std::endl;
				if (fIsSet(events, static_cast<uint32_t>(FD_WRITE)))
					std::cout << "   #write:  " << FD_WRITE << std::endl;
				if (fIsSet(events, static_cast<uint32_t>(FD_ACCEPT)))
					std::cout << "   #accept: " << FD_ACCEPT << std::endl;
				if (fIsSet(events, static_cast<uint32_t>(FD_CLOSE)))
					std::cout << "   #close:  " << FD_CLOSE << std::endl;
				#endif
				
				++nfds;
				hack::events::prepare_events(events);
				
				std::cout << "+ Triggered!" << std::endl;
				
				return true;
			}
		}
	}
	
	#ifndef NDEBUG
	std::cerr << "No events triggered! " << std::endl;
	#endif
	
	return false;
}

uint32_t Event::getEvents(fd_t fd) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(wsa).getEvents(fd: " << fd << ")" << std::endl;
	#endif
	
	assert(fd != 0);
	
	const std::map<fd_t, uint32_t>::const_iterator fev(fd_to_ev.find(fd));
	assert(fev != fd_to_ev.end());
	
	WSANETWORKEVENTS set;
	
	const int r = WSAEnumNetworkEvents(fd, w_ev[fev->second], &set);
	
	if (r == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		
		switch (error)
		{
		#ifndef NDEBUG
		case WSAEFAULT:
			assert(!(error == WSAEFAULT));
			break;
		case WSANOTINITIALISED:
			assert(!(error == WSANOTINITIALISED));
			break;
		case WSAEINVAL:
			assert(!(error == WSAEINVAL));
			break;
		case WSAENOTSOCK:
			assert(!(error == WSAEFAULT));
			break;
		case WSAEAFNOSUPPORT:
			assert(!(error == WSAEAFNOSUPPORT));
			break;
		#endif
		case WSAECONNRESET: // reset by remote
		case WSAECONNABORTED: // connection aborted
		case WSAETIMEDOUT: // connection timed-out
		case WSAENETUNREACH: // network unreachable
		case WSAECONNREFUSED: // connection refused/rejected
			return FD_WRITE|FD_CLOSE;
		case WSAENOBUFS: // out of network buffers
		case WSAENETDOWN: // network sub-system failure
		case WSAEINPROGRESS: // something's in progress
			return 0;
		default:
			std::cerr << "Event(wsa).getEvents() - unknown error: " << error << std::endl;
			break;
		}
		
		return 0;
	}
	
	uint32_t evs = static_cast<uint32_t>(set.lNetworkEvents);
	hack::events::prepare_events(evs);
	
	return evs;
}
