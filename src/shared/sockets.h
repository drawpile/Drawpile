/*******************************************************************************

Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#ifndef SOCKETS_H_INCLUDED
#define SOCKETS_H_INCLUDED

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	// TODO
#endif

#endif // SOCKETS_H_INCLUDED
