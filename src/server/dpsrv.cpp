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

// win32 macro workaround
#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include "config.h"

#include "../shared/protocol.h"

#include "server.h"
#include "sockets.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <getopt.h> // for command-line opts

// Server information
namespace srv_info
{

const std::string
	applicationName("DrawPile Server"),
	versionString("0.4"),
	copyrightNotice("Copyright (c) 2006,2007 M.K.A."),
	websiteURL("http://drawpile.sourceforge.net/");

}

using std::cout;
using std::endl;
using std::cerr;

void getArgs(int argc, char** argv, Server& srv) throw(std::bad_alloc)
{
	int32_t opt = 0, count = 0;
	
	while ((opt = getopt( argc, argv, "a:p:hlbu:S:s:wed:Tn:L:J:VM")) != -1)
	{
		++count;
		switch (opt)
		{
			case 'h': // help
				cout << "Options:" << endl
					<< endl
					<< "   -a [address]  Listen on address" << endl
					<< "   -p [port]     Listen on port (1024 - 65535)" << endl
					<< "   -h            This output (a.k.a. Help)" << endl
					<< "   -l            Localhost admin" << endl
					<< "   -b            Daemon mode" << endl
					<< "   -u [number]   Set user limit" << endl
					<< "   -S [string]   Admin password" << endl
					<< "   -s [string]   Server password" << endl
					<< "   -w            UTF-16 strings" << endl
					<< "   -e            Enable unique name enforcing" << endl
					<< "   -d [size]     Minimum dimension for canvas" << endl
					<< "   -n [size]     Name length limit" << endl
					<< "   -T            Enable transient mode " << endl
					<< "   -L [num]      Set session limit" << endl
					<< "   -J [num]      Set subscription limit" << endl
					<< "   -M            Allow duplicate connections" << endl
					;
				exit(EXIT_SUCCESS);
				break;
			case 'a': // address to listen on
				cerr << "- Setting listening address not implemented." << endl;
				exit(EXIT_FAILURE);
				break;
			case 'n': // name length limit
				{
					const int tmp = atoi(optarg);
					
					const uint8_t len = std::min(tmp, static_cast<int>(std::numeric_limits<uint8_t>::max()));
					
					srv.setNameLengthLimit(len);
					cout << "& Name length limit set to: " << static_cast<int>(len) << endl;
				}
				break;
			case 'p': // port to listen on
				{
					const uint16_t lo_port = atoi(optarg);
					
					char* off = strchr(optarg, '-');
					uint16_t hi_port = (off != 0 ? atoi(off+1) : lo_port);
					
					if (lo_port <= Network::SuperUser_Port or hi_port <= Network::SuperUser_Port)
					{
						cerr << "- Super-user ports not allowed!" << endl;
						exit(EXIT_FAILURE);
					}
					srv.setPorts(lo_port, hi_port);
					
					cout << "& Listening port range set to: " << lo_port;
					if (lo_port != hi_port)
						cout << " - " << hi_port;
					cout << endl;
				}
				break;
			case 'l': // localhost admin
				srv.setLocalhostAdmin(true);
				cout << "& Localhost admin enabled." << endl;
				break;
			case 'u': // user limit
				{
					const size_t user_limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<uint8_t>::max()));
					if (user_limit < 2)
					{
						cerr << "- Too low user limit." << endl;
						exit(EXIT_FAILURE);
					}
					
					srv.setUserLimit(user_limit);
					cout << "& User limit set to: " << user_limit << endl;
				}
				break;
			case 'S': // admin password
				{
					const size_t pw_len = strlen(optarg);
					if (pw_len == 0)
					{
						cerr << "- Zero length admin password?" << endl;
						exit(EXIT_FAILURE);
					}
					else if (pw_len > std::numeric_limits<uint8_t>::max())
					{
						cerr << "- Admin password too long, max length: " << static_cast<int>(std::numeric_limits<uint8_t>::max()) << endl;
						exit(EXIT_FAILURE);
					}
					
					char* password = new char[pw_len];
					memcpy(password, optarg, pw_len);
					srv.setAdminPassword(password, pw_len);
					cout << "& Admin password set." << endl;
				}
				break;
			case 's': // password
				{
					const size_t pw_len = strlen(optarg);
					if (pw_len == 0)
					{
						cerr << "- Zero length server password?" << endl;
						exit(EXIT_FAILURE);
					}
					else if (pw_len > std::numeric_limits<uint8_t>::max())
					{
						cerr << "- Server password too long, max length: " << static_cast<int>(std::numeric_limits<uint8_t>::max()) << endl;
						exit(EXIT_FAILURE);
					}
					
					char* password = new char[pw_len];
					memcpy(password, optarg, pw_len);
					srv.setPassword(password, pw_len);
					cout << "& Server password set." << endl;
				}
				
				break;
			case 'T': // transient/temporary
				srv.setTransient(true);
				cout << "& Server will exit after all users have left." << endl;
				break;
			case 'b': // background
				#if defined(HAVE_FORK) or defined(HAVE_FORK1)
				{
					#ifdef HAVE_FORK
					pid_t rc = fork();
					#else // HAVE_FORK1
					pid_t rc = fork1();
					#endif
					if (rc == -1)
					{
						cerr << "- fork() failed" << endl;
						exit(EXIT_FAILURE);
					}
					else if (rc != 0)
						exit(0); // kill parent process
					
					fclose(stdout);
					fclose(stderr);
				}
				#else
				cerr << "- Non-forking daemon mode not implemented." << endl;
				exit(EXIT_FAILURE);
				#endif
				break;
			case 'd': // adjust minimum dimension.
				{
					const size_t mindim = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<uint16_t>::max()));
					if (mindim < 400) // just a reasonably nice lower bound
					{
						cerr << "- Min. dimension must be at least 400" << endl;
						exit(EXIT_FAILURE);
					}
					
					srv.setMinDimension(mindim);
					cout << "& Minimum board dimension set to: " << mindim << endl;
				}
				break;
			case 'e': // name enforcing
				srv.setRequirement(protocol::requirements::EnforceUnique);
				cout << "& Unique name enforcing enabled." << endl;
				break;
			case 'w': // utf-16 string (wide chars)
				srv.setRequirement(protocol::requirements::WideStrings);
				cout << "& UTF-16 string mode enabled." << endl;
				break;
			case 'L': // session limit
				{
					const int limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<uint8_t>::max()));
					if (limit < 1)
					{
						cerr << "- Limit must be greater than 0" << endl;
						exit(EXIT_FAILURE);
					}
					
					srv.setSessionLimit(limit);
					cout << "& Session limit set to: " << limit << endl;
				}
				break;
			case 'J': // subscription limit
				{
					const int limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<uint8_t>::max()));
					if (limit < 1)
					{
						cerr << "- Limit must be greater than 0" << endl;
						exit(EXIT_FAILURE);
					}
					
					srv.setSubscriptionLimit(limit);
					cout << "& Subscription limit set to: " << limit << endl;
				}
				break;
			case 'M': // allow multiple connections from same address
				srv.blockDuplicateConnectsion(false);
				cout << "& Multiple connections allowed from same source address." << endl;
				break;
			case 'V': // version
				exit(0);
			default:
				cerr << "What?" << endl;
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (count != 0)
		cout << endl;
}

int main(int argc, char** argv)
{
	#ifndef NDEBUG
	std::ios::sync_with_stdio(false);
	#endif
	
	// We don't use user input for anything
	fclose(stdin);
	
	// application name, version, etc. info
	cout << srv_info::applicationName << " v" << srv_info::versionString
		#ifndef NDEBUG
		<< " DEBUG"
		#endif
		<< endl
		<< srv_info::copyrightNotice << endl
		<< srv_info::websiteURL << endl
		<< endl;
	
	int rc = EXIT_SUCCESS;
	
	// limited scope for server
	{
		Server srv;
		
		getArgs(argc, argv, srv);
		
		#ifdef NEED_NET
		const Net _net; // :)
		#endif // NEED_NET
		
		if (!srv.init())
		{
			#ifndef NDEBUG
			std::cerr << "- Initialization failed!" << std::endl;
			#endif
			return EXIT_FAILURE;
		}
		
		try {
			rc = srv.run();
		}
		catch (...) {
			#ifndef NDEBUG
			std::cerr << "- Exception caught!" << std::endl;
			#endif
			rc = 2;
		}
	} // end server scope
	
	#ifndef NDEBUG
	std::cout << "~ Quitting..." << std::endl;
	#endif
	
	return rc;
}
