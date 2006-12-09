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

#include "../shared/protocol.h"
#include "../shared/sockets.h"

#include "user.h"
#include "event.h"
#include "server.h"

//#include <sys/time.h>

#include <bitset>
#include <map>
#include <list>
#include <vector>
#include <cstdlib>
#include <iostream>

/**
 *
 */
int main(int argc, char** argv)
{
	std::ios::sync_with_stdio(false);
	
	std::cout << "dpsrv v0.0a\n(C) 2006 M.K.A.\n" << std::endl;
	
	Server srv;
	
	srv.getArgs(argc, argv);
	
	#ifdef WIN32
	std::cout << "initializing WSA" << std::endl;
	WSADATA info;
    if (WSAStartup(MAKEWORD(2,0), &info)) { exit(1); }
    #endif

	if (srv.init() != 0)
		return 1;
	
	std::cout << "running main" << std::endl;
	
	srv.run();
	
	std::cout << "done" << std::endl;
	
	return 0; // never reached
}
