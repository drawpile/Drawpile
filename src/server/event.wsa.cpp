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

#include <cerrno> // errno
#include <memory> // memcpy()
#include <cassert> // assert()

const uint32_t
	//! identifier for 'read' event
	Event::read = FD_READ,
	//! identifier for 'write' event
	Event::write = FD_WRITE,
	//! error event
	Event::error = 0xFFFF,
	//! hangup event
	Event::hangup = FD_CLOSE;

Event::Event() throw()
	: w_ev_count(0)
{
	#ifndef NDEBUG
	std::cout << "Event(wsa)()" << std::endl;
	std::cout << "Max events: " << WSA_MAXIMUM_WAIT_EVENTS << std::endl;
	#endif
	
	memset(w_ev, 0, WSA_MAXIMUM_WAIT_EVENTS);
}

Event::~Event() throw()
{
	#ifndef NDEBUG
	std::cout << "~Event(wsa)()" << std::endl;
	#endif
}

bool Event::init() throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).init()" << std::endl;
	#endif
	
	return true;
}

void Event::finish() throw()
{
	#ifndef NDEBUG
	std::cout << "Event::finish()" << std::endl;
	#endif
}

int Event::wait(uint32_t msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).wait(msecs: " << msecs << ")" << std::endl;
	#endif
	
	assert(fd_to_ev.size() != 0);
	
	uint32_t r = WSAWaitForMultipleEvents(fd_to_ev.size(), w_ev, 0, msecs, true);
	
	if (r == WSA_WAIT_FAILED)
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
			std::cerr << "Event(wsa).wait() - unknown error: " << r << std::endl;
			break;
		}
	}
	else
	{
		switch (r)
		{
		case WSA_WAIT_IO_COMPLETION:
		case WSA_WAIT_TIMEOUT:
			return 0;
		default:
			nfds = r - WSA_WAIT_EVENT_0;
			break;
		}
	}
	
	return nfds;
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( ev == read or ev == write or ev == read|write );
	assert( fd >= 0 );
	
	WSAEVENT ev_s = WSACreateEvent();
	if (!ev_s) return false;
	
	for (uint32_t i=0; i != WSA_MAXIMUM_WAIT_EVENTS; i++)
	{
		if (w_ev[i] == 0)
		{
			WSAEventSelect(fd, ev_s, ev);
			w_ev[i] = ev_s;
			ev_to_fd.insert(std::make_pair(i, fd));
			fd_to_ev.insert(std::make_pair(fd, i));
			event_index.insert(std::make_pair(ev_s, i));
			return true;
		}
	}
	
	WSACloseEvent(ev_s);
	
	return false;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( ev == read or ev == write or ev == read|write );
	assert( fd >= 0 );
	
	std::map<fd_t, uint32_t>::iterator fi(fd_to_ev.find(fd));
	if (fi == fd_to_ev.end())
		return false;
	
	WSAEventSelect(fd, w_ev[fi->second], ev);
	
	return 0;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( ev == read or ev == write or ev == read|write );
	assert( fd >= 0 );
	
	std::map<fd_t, uint32_t>::iterator fi(fd_to_ev.find(fd));
	if (fi == fd_to_ev.end())
		return false;
	
	WSACloseEvent(w_ev[fi->second]);
	
	w_ev[fi->second] = 0;
	
	fd_to_ev.erase(fd);
	
	return true;
}

uint32_t Event::getEvents(fd_t fd) const throw()
{
	std::map<fd_t, uint32_t>::const_iterator fev(fd_to_ev.find(fd));
	if (fev == fd_to_ev.end())
		return 0;
	
	LPWSANETWORKEVENTS set(0);
	
	int r = WSAEnumNetworkEvents(fd, w_ev[fev->second], set);
	
	if (r == SOCKET_ERROR)
	{
		//_error = WSAGetLastError();
		
		switch (WSAGetLastError())
		{
		#ifndef NDEBUG
		case WSAEFAULT:
			assert(!(_error == WSAEFAULT));
			break;
		case WSANOTINITIALISED:
			assert(!(_error == WSANOTINITIALISED));
			break;
		case WSAEINVAL:
			assert(!(_error == WSAEINVAL));
			break;
		case WSAENOTSOCK:
			assert(!(_error == WSAEFAULT));
			break;
		case WSAEAFNOSUPPORT:
			assert(!(_error == WSAEAFNOSUPPORT));
			break;
		#endif
		case WSAECONNRESET: // reset by remote
		case WSAECONNABORTED: // connection aborted
		case WSAETIMEDOUT: // connection timed-out
		case WSAENETUNREACH: // network unreachable
		case WSAECONNREFUSED: // connection refused/rejected
			return FD_CLOSE;
		case WSAENOBUFS: // out of network buffers
		case WSAENETDOWN: // network sub-system failure
		case WSAEINPROGRESS: // something's in progress
			return 0;
		default:
			std::cerr << "Event(wsa).getEvents() - unknown error: " << _error << std::endl;
			break;
		}
		return 0;
	}
	
	return set->lNetworkEvents;
}

bool Event::isset(fd_t fd, uint32_t ev) const throw()
{
	#ifndef NDEBUG
	std::cout << "Event(wsa).isset(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(ev == read or ev == write or ev == hangup or ev == error);
	assert(fd >= 0);
	
	return fIsSet(getEvents(fd), ev);
}
