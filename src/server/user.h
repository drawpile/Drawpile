/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

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

*******************************************************************************/

#ifndef USER_H_INCLUDED
#define USER_H_INCLUDED

#include <stdint.h>
#include <map>
#include <queue>

#include "buffer.h"

#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"
#include "../shared/sockets.h"

//! User session data
/**  */
struct UserData
{
	UserData()
		: session(protocol::Global),
		mode(protocol::user::None),
		owner(false)
	{
	}
	
	UserData(const UserData& ud)
		: session(ud.session),
		mode(ud.mode),
		owner(ud.owner)
	{
	}
	
	~UserData()
	{
	}
	
	uint8_t session; // session id
	
	uint8_t mode; // user mode within session
	
	bool owner; // is the owner of the session
};

//! User
/**  */
struct User
{
	User()
		: id(0), nlen(0), name(0), state(0)
	{
		
	}
	
	/*
	User(const User& nu)
		: s(nu.s),
		nlen(nu.nlen),
		name(nu.name),
		session(nu.session),
		u(nu.u),
		state(nu.state),
		sessions(nu.sessions)
	{
		
	}
	*/
	
	~User()
	{
		delete [] name;
		
		// TODO: clean sessions
	}
	
	Socket *s;
	
	uint8_t id;
	
	// name length
	uint8_t nlen;
	// username
	char* name;
	
	// currently active session
	uint8_t session;
	
	// global user data
	UserData u;
	
	uint8_t state;
	
	std::map<uint8_t, UserData> sessions;
	
	// output buffers
	std::queue<Buffer> buffers;
	
	Buffer input;
};

#endif // USER_H_INCLUDED
