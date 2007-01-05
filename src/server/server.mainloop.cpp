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

#include "server.h"

int Server::run() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::run()" << std::endl;
	#endif
	#endif
	
	// user map iterator
	std::map<fd_t, user_ref>::iterator usr;
	
	// event count
	int ec /*, evs */;
	
	#ifdef EVENT_HAS_ALL
	std::pair<fd_t, uint32_t> event;
	#endif
	
	// main loop
	while (1)
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
			//evl = ev.getEvents( ec );
			#ifdef DEBUG_SERVER
			#ifndef NDEBUG
			std::cout << "Events waiting: " << ec << std::endl;
			#endif
			#endif
			
			#if defined(EVENT_BY_ORDER)
			/* BY ORDER */
			event = ev.getEvent();
			
			if (event->first == lsock.fd())
			{
				ec--;
				
				uAdd( lsock.accept() );
			}
			
			#ifndef NDEBUG
			int last_ec = ec + 1;
			#endif // NDEBUG
			while (ec != 0)
			{
				#ifndef NDEBUG
				if (ec == last_ec)
				{
					std::cout << "Couldn't find one of the events." << std::endl;
					break;
				}
				#endif // NDEBUG
				
				usr = users.find(event->first);
				if (usr != users.end())
				{
					if (fIsSet(event->second, ev.read))
					{
						uRead(usr->second);
						if (--ec == 0) break;
					}
					if (fIsSet(event->second, ev.write))
					{
						uWrite(usr->second);
						if (--ec == 0) break;
					}
					if (fIsSet(event->second, ev.error))
					{
						uRemove(usr->second);
						if (--ec == 0) break;
					}
					#ifdef EV_HAS_HANGUP
					if (fIsSet(event->second, ev.hangup))
					{
						uRemove(usr->second);
						if (--ec == 0) break;
					}
					#endif // EV_HAS_HANGUP
				}
				else
				{
					std:cerr << "FD not in users." << std::endl;
					break;
				}
				
				#ifndef NDEBUG
				last_ec = ec;
				#endif // NDEBUG
			}
			#elif defined(EVENT_BY_FD)
			/* BY FD */
			if (ev.isset(lsock.fd(), ev.read))
			{
				uAdd( lsock.accept() );
				if (--ec == 0) continue;
			}
			
			if (ec > 0)
			{
				for (usr = users.begin(); usr != users.end(); usr++)
				{
					if (ev.isset(usr->first, ev.read))
					{
						uRead(usr->second);
						if (--ec == 0) break;
					}
					if (ev.isset(usr->first, ev.write))
					{
						uWrite(usr->second);
						if (--ec == 0) break;
					}
					if (ev.isset(usr->first, ev.error))
					{
						uRemove(usr->second);
						if (--ec == 0) break;
					}
					#ifdef EV_HAS_HANGUP
					if (ev.isset(usr->first, ev.hangup))
					{
						uRemove(usr->second);
						if (--ec == 0) break;
					}
					#endif // EV_HAS_HANGUP
				}
				
				#ifndef NDEBUG
				if (ec != 0)
					std::cout << "Not in clients..." << std::endl;
				#endif //NDEBUG
			}
			#else
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
