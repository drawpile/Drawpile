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

#include "dpsrv.h"

#include "config.h"

#include <algorithm> // std::min, std::max
#include <iostream>
#include <getopt.h>

#include "server.h" // Server class
#include "network.h" // Network namespace
#include "types.h"

using std::cout;
using std::endl;
using std::cerr;

static
void getArgs(int argc, char** argv, Server& srv)
{
	getarg:
	switch (getopt(argc, argv, "a:p:hlbu:S:s:wed:Tn:L:J:VM"))
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
			<< "   -z            Enable deflate compression" << endl
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
			
			const octet len = std::min(tmp, static_cast<int>(std::numeric_limits<octet>::max()));
			
			srv.setNameLengthLimit(len);
			#ifndef NDEBUG
			cout << "& Name length limit set to: " << static_cast<int>(len) << endl;
			#endif
		}
		break;
	case 'z':
		#ifndef NDEBUG
		#ifdef HAVE_ZLIB
		cout << "& Enabled Deflate compression" << endl;
		#else
		cout << "- Deflate was not configured in" << endl;
		#endif
		#endif
		srv.setDeflate(true);
		break;
	case 'p': // port to listen on
		{
			const uint16_t port = atoi(optarg);
			
			if (port <= Network::SuperUser_Port)
			{
				cerr << "- Super-user ports not allowed!" << endl;
				exit(EXIT_FAILURE);
			}
			srv.setPort(port);
			
			#ifndef NDEBUG
			cout << "& Listening port set to: " << port << endl;
			#endif
		}
		break;
	case 'l': // localhost admin
		srv.setLocalhostAdmin(true);
		#ifndef NDEBUG
		cout << "& Localhost admin enabled." << endl;
		#endif
		break;
	case 'u': // user limit
		{
			const size_t user_limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<octet>::max()));
			if (user_limit < 2)
			{
				cerr << "- Too low user limit." << endl;
				exit(EXIT_FAILURE);
			}
			
			srv.setUserLimit(user_limit);
			#ifndef NDEBUG
			cout << "& User limit set to: " << user_limit << endl;
			#endif
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
			else if (pw_len > std::numeric_limits<octet>::max())
			{
				cerr << "- Admin password too long, max length: " << static_cast<int>(std::numeric_limits<octet>::max()) << endl;
				exit(EXIT_FAILURE);
			}
			
			char* password = new char[pw_len];
			memcpy(password, optarg, pw_len);
			srv.setAdminPassword(password, pw_len);
			#ifndef NDEBUG
			cout << "& Admin password set." << endl;
			#endif
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
			else if (pw_len > std::numeric_limits<octet>::max())
			{
				cerr << "- Server password too long, max length: " << static_cast<int>(std::numeric_limits<octet>::max()) << endl;
				exit(EXIT_FAILURE);
			}
			
			char* password = new char[pw_len];
			memcpy(password, optarg, pw_len);
			srv.setPassword(password, pw_len);
			#ifndef NDEBUG
			cout << "& Server password set." << endl;
			#endif
		}
		
		break;
	case 'T': // transient/temporary
		srv.setTransient(true);
		#ifndef NDEBUG
		cout << "& Server will exit after all users have left." << endl;
		#endif
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
			#ifndef NDEBUG
			cout << "& Minimum board dimension set to: " << mindim << endl;
			#endif
		}
		break;
	case 'e': // name enforcing
		srv.setUniqueNameEnforcing();
		#ifndef NDEBUG
		cout << "& Unique name enforcing enabled." << endl;
		#endif
		break;
	case 'w': // utf-16 string (wide chars)
		srv.setUTF16Requirement();
		#ifndef NDEBUG
		cout << "& UTF-16 string mode enabled." << endl;
		#endif
		break;
	case 'L': // session limit
		{
			const int limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<octet>::max()));
			if (limit < 1)
			{
				cerr << "- Limit must be greater than 0" << endl;
				exit(EXIT_FAILURE);
			}
			
			srv.setSessionLimit(limit);
			#ifndef NDEBUG
			cout << "& Session limit set to: " << limit << endl;
			#endif
		}
		break;
	case 'J': // subscription limit
		{
			const int limit = std::min(atoi(optarg), static_cast<int>(std::numeric_limits<octet>::max()));
			if (limit < 1)
			{
				cerr << "- Limit must be greater than 0" << endl;
				exit(EXIT_FAILURE);
			}
			
			srv.setSubscriptionLimit(limit);
			#ifndef NDEBUG
			cout << "& Subscription limit set to: " << limit << endl;
			#endif
		}
		break;
	case 'M': // allow multiple connections from same address
		srv.setDuplicateConnectionBlocking(false);
		#ifndef NDEBUG
		cout << "& Multiple connections allowed from same source address." << endl;
		#endif
		break;
	case 'V': // version
		exit(0);
	case -1: // last arg
		return; 
	default:
		cerr << "What?" << endl;
		exit(EXIT_FAILURE);
		break;
	}
	goto getarg;
}

int main(int argc, char** argv)
{
	#ifndef NDEBUG
	std::ios::sync_with_stdio(false);
	#endif
	
	// We don't use user input for anything
	fclose(stdin);
	
	// application name, version, etc. info
	cout << "DrawPile Server v0.4" << endl
		<< "Copyright (c) 2006,2007 M.K.A." << endl
		<< "http://drawpile.sourceforge.net/" << endl
		<< endl;
	
	int rc = EXIT_FAILURE;
	
	#ifndef NDEBUG
	cout << "+ Initializing WSA" << endl;
	#endif
	
	if (!Network::start())
		goto end;
	
	// limited scope for server
	{
		#ifndef NDEBUG
		cout << "+ Initializing server" << endl;
		#endif
		
		Server srv;
		
		getArgs(argc, argv, srv);
		
		if ((rc = srv.init()) != 0)
		{
			#ifndef NDEBUG
			cerr << "- Initialization failed!" << endl;
			#endif
			goto end;
		}
		
		cout << "+ Listening on port " << srv.getPort() << endl;
		
		rc = srv.run();
	} // end server scope
	
	end:
	Network::stop();
	
	switch (rc)
	{
	case Server::xNoError:
		break;
	case Server::xUnsupportedIPv:
		#ifdef IPV6
		cerr << "- IPv6 not supported on this machine." << endl;
		#else
		cerr << "- IPv4 not supported on this machine." << endl;
		#endif
		break;
	case Server::xLastUserLeft:
		cout << "! Last user left." << endl;
		break;
	case Server::xBindError:
		cout << "- Port binding failed." << endl;
		break;
	case Server::xSocketError:
		cerr << "- Socket creation failed." << endl;
		break;
	case Server::xEventError:
		cerr << "- Event system error." << endl;
		break;
	default:
		cerr << "- Unknown error occured." << endl;
		break;
	}
	
	#ifndef NDEBUG
	cout << "~ Quitting..." << endl;
	#endif
	
	return rc;
}
