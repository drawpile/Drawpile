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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::run()" << std::endl;
	#endif
	#endif
	
	assert(state == server::state::Init);
	state = server::state::Active;
	
	// user map iterator
	user_iterator usr;
	
	// event count
	int ec;
	
	#if defined(EVENT_HAS_ALL)
	std::pair<fd_t, uint32_t> event;
	#endif
	
	#ifdef EVENT_BY_FD
	uint32_t t_events;
	#endif
	
	// main loop
	while (state == server::state::Active)
	{
		ec = ev.wait(5000);
		
		if (ec == 0)
		{
			// continue, no fds or time exceeded.
		}
		else if (ec == -1)
		{
			std::cout << "Error in event system." << std::endl;
			// TODO (error)
			return -1;
		}
		else
		{
			#ifdef DEBUG_SERVER
			#ifndef NDEBUG
			std::cout << "Events waiting: " << ec << std::endl;
			#endif
			#endif
			
			#if defined(EVENT_BY_ORDER)
			/* BY ORDER */
			do
			{
				event = ev.getEvent(--ec);
				
				if (event->first == lsock.fd())
				{
					uAdd( lsock.accept() );
					continue;
				}
				
				usr = users.find(event->first);
				if (usr != users.end())
				{
					if (fIsSet(event->second, ev.error))
					{
						uRemove(usr->second, protocol::user_event::BrokenPipe);
						continue;
					}
					#ifdef EV_HAS_HANGUP
					if (fIsSet(event->second, ev.hangup))
					{
						uRemove(usr->second, protocol::user_event::Disconnect);
						continue;
					}
					#endif // EV_HAS_HANGUP
					if (fIsSet(event->second, ev.read))
					{
						uRead(usr->second);
						if (usr->second == 0) continue;
					}
					if (fIsSet(event->second, ev.write))
					{
						uWrite(usr->second);
						if (usr->second == 0) continue;
					}
				}
				else
				{
					std:cerr << "FD not in users." << std::endl;
					break;
				}
			}
			while (ec != 0);
			#elif defined(EVENT_BY_FD)
			/* BY FD */
			if (ev.isset(lsock.fd(), ev.read))
			{
				uAdd( lsock.accept() );
				if (--ec == 0) continue;
			}
			
			for (usr = users.begin(); usr != users.end(); usr++)
			{
				t_events = ev.getEvents(usr->first);
				if (t_events != 0)
				{
					if (fIsSet(t_events, ev.error))
					{
						uRemove(usr->second, protocol::user_event::BrokenPipe);
						if (--ec == 0) break;
					}
					#ifdef EV_HAS_HANGUP
					if (fIsSet(t_events, ev.hangup))
					{
						uRemove(usr->second, protocol::user_event::Disconnect);
						if (--ec == 0) break;
					}
					#endif // EV_HAS_HANGUP
					if (fIsSet(t_events, ev.read))
					{
						uRead(usr->second);
						if (usr->second == 0) break;
					}
					if (fIsSet(t_events, ev.write))
					{
						uWrite(usr->second);
						if (usr->second == 0) break;
					}
					
					if (--ec == 0) break;
				}
			}
			
			if (0) {
			#ifndef NDEBUG
			if (ec != 0) { std::cout << "Events left: " << ec << std::endl; }
			#endif //NDEBUG
			}
			
			usr = users.end();
			
			#else // EVENT_BY_*
			#error No event fetching method defined
			#endif // EVENT_BY_*
			
			// do something
		}
		
		// do something generic?
	}
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Done?" << std::endl;
	#endif
	#endif
	
	return 0;
}
