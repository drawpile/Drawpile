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

#include "../../config.h"

#include "../shared/protocol.h"

#include "server.flags.h"
#include "server.h"

#include <cstdlib>
#include <iostream>
#include <getopt.h> // for command-line opts

namespace srv_info
{

const char
	applicationName[] = "DrawPile Server",
	versionString[] = "0.1",
	copyrightNotice[] = "Copyright (c) 2006,2007 M.K.A.",
	//contactAddress[] = "wyrmchild@users.sourceforge.net",
	websiteURL[] = "http://drawpile.sourceforge.net/";

}

void getArgs(int argc, char** argv, Server* srv) throw(std::bad_alloc)
{
	int32_t opt = 0;
	
	while ((opt = getopt( argc, argv, "a:p:hlbu:S:s:wed:T")) != -1)
	{
		switch (opt)
		{
			case 'h': // help
				std::cout << "Syntax: drawpile-srv [options]" << std::endl
					<< std::endl
					<< "Options:" << std::endl
					<< std::endl
					<< "   -a [address]  listen on this address" << std::endl
					<< "   -p [port]     listen on 'port' (1024 - 65535)" << std::endl
					<< "   -h            this output (a.k.a. Help)" << std::endl
					<< "   -l            localhost admin" << std::endl
					<< "   -b            daemon mode" << std::endl
					<< "   -u [number]   set server user limit" << std::endl
					<< "   -S [string]   admin password string" << std::endl
					<< "   -s [string]   password string" << std::endl
					<< "   -w            UTF-16 strings" << std::endl
					<< "   -e            unique name enforcing" << std::endl
					<< "   -d [size]     minimum dimension for canvas" << std::endl
					<< "   -T            enable transient mode" << std::endl
					;
				exit(1);
				break;
			case 'a': // address to listen on
				std::cerr << "Setting listening address not implemented." << std::endl;
				exit(1);
				break;
			case 'p': // port to listen on
				{
					uint16_t lo_port = atoi(optarg), hi_port=0;
					{
						char* off = strchr(optarg, '-');
						hi_port = (off != 0 ? atoi(off+1) : lo_port);
					}
					
					if (lo_port <= 1023 or hi_port <= 1023)
					{
						std::cerr << "Super-user ports not allowed!" << std::endl;
						exit(1);
					}
					srv->setPorts(lo_port, hi_port);
					
					std::cout << "Listening port range set to: " << lo_port;
					if (lo_port != hi_port)
						std::cout << " - " << hi_port;
					std::cout << std::endl;
				}
				break;
			case 'l': // localhost admin
				srv->setMode(server::mode::LocalhostAdmin);
				std::cout << "Localhost admin enabled." << std::endl;
				break;
			case 'u': // user limit
				{
					size_t user_limit = atoi(optarg);
					srv->setUserLimit(user_limit);
					std::cout << "User limit set to: " << user_limit << std::endl;
				}
				break;
			case 'S': // admin password
				{
					size_t pw_len = strlen(optarg);
					char* password = 0;
					if (pw_len > 0)
					{
						password = new char[pw_len];
						memcpy(password, optarg, pw_len);
					}
					srv->setAdminPassword(password, pw_len);
				}
				std::cout << "Admin password set." << std::endl;
				break;
			case 's': // password
				{
					size_t pw_len = strlen(optarg);
					char* password = 0;
					if (pw_len > 0)
					{
						password = new char[pw_len];
						memcpy(password, optarg, pw_len);
					}
					srv->setPassword(password, pw_len);
				}
				std::cout << "Server password set." << std::endl;
				break;
			case 'T':
				srv->setMode(server::mode::Transient);
				std::cout << "Server will exit after all users have left." << std::endl;
				break;
			case 'b':
				srv->setMode(server::mode::Daemon);
				std::cerr << "Daemon mode not implemented." << std::endl;
				exit(1);
				break;
			case 'd': // adjust minimum dimension.
				{
					size_t mindim = atoi(optarg);
					srv->setMinDimension(mindim);
					std::cout << "Minimum board dimension set to: " << mindim << std::endl;
				}
				break;
			case 'e': // name enforcing
				srv->setRequirement(protocol::requirements::EnforceUnique);
				std::cout << "Unique name enforcing enabled." << std::endl;
				break;
			case 'w': // utf-16 string
				srv->setRequirement(protocol::requirements::WideStrings);
				std::cout << "UTF-16 string mode enabled." << std::endl;
				break;
			case 'V': // version
				exit(0);
			default:
				std::cerr << "What?" << std::endl;
				exit(1);
				break;
		}
	}
}

int main(int argc, char** argv)
{
	#ifdef NDEBUG
	std::ios::sync_with_stdio(false);
	#endif
	
	// application name, version, etc. info
	std::cout << srv_info::applicationName << " v" << srv_info::versionString << std::endl
		<< srv_info::copyrightNotice << std::endl
		<< srv_info::websiteURL << std::endl
		<< std::endl;
	
	int rc = 0;
	
	// limited scope for server
	{
		Server srv;
		
		getArgs(argc, argv, &srv);
		
		netInit();
		
		if (srv.init() != 0)
			return 1;
		
		try {
			rc = srv.run();
		}
		catch (...) {
			std::cerr << "Unknown exception caught." << std::endl;
			rc = 9;
			// do nothing
		}
	} // server scope
	
	netStop();
	
	#ifndef NDEBUG
	std::cout << "quitting" << std::endl;
	#endif
	
	return rc;
}
