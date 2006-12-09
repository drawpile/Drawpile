/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   ---

   For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#ifndef Server_C_Included
#define Server_C_Included

#include "../shared/sockets.h"
#include "event.h"
#include "user.h"

//#include <sys/time.h>
#include <getopt.h> // for command-line opts

#include <bitset>
#include <map>
#include <list>
#include <vector>

//! foo
namespace defaults
{

//! Hard limit for users and sessions.
const int hard_limit = 255;

} // namespace defaults

//! Server
class Server
{
protected:
	/* data */
	
	Event ev;
	
	std::bitset<defaults::hard_limit> user_ids;
	std::bitset<defaults::hard_limit> session_ids;
	
	std::vector<uint32_t> sockets;
	std::map<uint32_t, User*> users;
	std::map<uint8_t, uint32_t> user_id_map;
	
	// listening socket
	Socket lsock;
	
	char* password;
	size_t pw_len,
		user_limit,
		cur_users;

	uint16_t
		hi_port,
		lo_port;

	/* functions */

	//! Frees user ID
	void freeUserID(uint8_t id);

	//! Frees session ID
	void freeSessionID(uint8_t id);

	//! Cleanup anything that's left.
	inline
	void cleanup(int rc);
	
	//! Get free user ID
	uint8_t getUserID();

	//! Get free session ID
	uint8_t getSessionID();

	//! Write to user socket
	void uWrite(uint32_t fd);

	//! Read from user socket
	void uRead(uint32_t fd);

public:
	//! ctor
	Server();
	
	//! dtor
	~Server();
	
	//! Initializes anything that need to be done so.
	int init();
	
	//! Parses command-line args
	void getArgs(int argc, char** argv);
	
	//! Enter main loop
	int run();
}; // class Server

#endif // Server_C_Included
