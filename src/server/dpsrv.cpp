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

#include "../shared/protocol.h"

//#include "sockets.h"
//#include "user.h"
//#include "event.h"
#include "server.h"

//#include <sys/time.h>

//#include <bitset>
//#include <map>
//#include <list>
//#include <vector>
#include <cstdlib>
#include <iostream>
#include <getopt.h> // for command-line opts

namespace srv_info
{

#if 0
const uint16_t
	major_version = 0,
	minor_version = 1;
#endif // 0

const char versionString[] = "0.1a";

const char copyrightNotice[] = "Copyright (c) 2006-2007 M.K.A.";
//const char contactAddress[] = "wyrmchild@users.sourceforge.net";
const char applicationName[] = "drawpile-srv";
const char websiteURL[] = "http://drawpile.sourceforge.net/";

}

void getArgs(int argc, char** argv, Server* srv) throw(std::bad_alloc)
{
	int32_t opt = 0;
	
	while ((opt = getopt( argc, argv, "p:Vhs:")) != -1)
	{
		switch (opt)
		{
			/*
			case 'a': // address to listen on
				
				break;
			*/
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
				}
				break;
			/*
			case 'l': // localhost admin
				localhost_admin = true;
				break;
			*/
			case 'u': // user limit
				srv->setUserLimit(atoi(optarg));
				break;
			case 'c': // password
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
				break;
			case 'h': // help
				std::cout << "Syntax: dbsrv [options]" << std::endl
					<< std::endl
					<< "Options:" << std::endl
					<< std::endl
					<< "   -p [port]    listen on 'port' (1024 - 65535)" << std::endl
					<< "   -h           this output (a.k.a. Help)" << std::endl
					//<< "   -d           daemon mode" << std::endl
					<< "   -s [string]  password string" << std::endl
					//<< "   -w           UTF-16 string" << std::endl
					;
				exit(1);
				break;
			default:
				std::cerr << "What?" << std::endl;
			case 'V': // version
				exit(0);
		}
	}
}

int main(int argc, char** argv)
{
	std::ios::sync_with_stdio(false);
	
	std::cout << srv_info::applicationName << " v" << srv_info::versionString << std::endl
		<< srv_info::copyrightNotice << std::endl
		<< srv_info::websiteURL << std::endl
		<< std::endl;
	
	Server srv;
	
	getArgs(argc, argv, &srv);
	
	netInit();
	
	if (srv.init() != 0)
		return 1;
	
	std::cout << "running main" << std::endl;
	
	srv.run();
	
	std::cout << "done" << std::endl;
	
	netStop();
	
	return 0; // never reached
}
