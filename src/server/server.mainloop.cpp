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
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   ---

   For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#include "server.flags.h"
#include "server.h"

int Server::run() throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::run()" << std::endl;
	#endif
	
	assert(state == server::state::Init);
	state = server::state::Active;
	
	User *usr;
	
	// event count
	int ec;
	
	fd_t fd;
	uint32_t events;
	
	// set event timeout
	ev.timeout(30000);
	
	// main loop
	while (state == server::state::Active)
	{
		ec = ev.wait();
		current_time = time(0);
		if (ec == 0)
		{
			// continue, no fds or time exceeded.
			continue;
		}
		else if (ec == -1)
		{
			std::cerr << "Error in event system." << std::endl;
			// TODO (error)
			return -1;
		}
		else
		{
			do
			{
				#ifndef EV_WSA // non-WSA
				--ec;
				#endif
				
				if (!ev.getEvent(fd, events))
					break;
				
				if (fd == lsock.fd())
				{
					uAdd( lsock.accept() );
					continue;
				}
				
				assert(users.find(fd) != users.end());
				
				usr = users.find(fd)->second;
				
				/*
				#ifdef EV_WSA
				ev.modify(usr->sock->fd(), usr->events);
				#endif
				*/
				
				#ifdef EV_HAS_ERROR
				if (fIsSet(events, ev.error))
				{
					uRemove(usr, protocol::user_event::BrokenPipe);
					continue;
				}
				#endif // EV_HAS_ERROR
				#ifdef EV_HAS_HANGUP
				if (fIsSet(events, ev.hangup))
				{
					uRemove(usr, protocol::user_event::Disconnect);
					continue;
				}
				#endif // EV_HAS_HANGUP
				if (fIsSet(events, ev.read))
				{
					uRead(usr);
					if (usr == 0) continue;
				}
				if (fIsSet(events, ev.write))
				{
					uWrite(usr);
					if (usr == 0) continue;
				}
			}
			#if !defined(EV_WSA) // non-WSA
			while (ec != 0);
			#else // WSA
			while (1);
			#endif
			
			// do something
		}
		
		// do something generic?
		
		// check timer
		if (next_timer < current_time)
		{
			// schedule next
			next_timer = current_time + 600;
			
			// check list
			if (!utimer.empty())
				cullIdlers();
		}
	}
	
	return 0;
}
