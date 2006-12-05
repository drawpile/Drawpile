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

#ifndef Protocol_Errors_INCLUDED
#define Protocol_Errors_INCLUDED

// #include <exception>

namespace protocol
{

//! Scrambled input buffer.
class scrambled_buffer
	: public std::exception
{
public:
	scrambled_buffer() { }
	~scrambled_buffer() throw() { }
};

//! Errors
/**
 * @see protocol::Error message
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Error
 */
namespace error
{

const uint16_t
	//! No error code defined.
	None = 0,
	
	/* Server status things that can be considered errors. */
	
	//! Server full.
	Overloaded = 7,
	
	/* Session errors. */
	
	//! No sessions (likely a response to ListSessions).
	NoSessions = 10,
	//! Does not exist.
	UnknownSession = 12,
	//! Subscribe failed, session full.
	SessionFull = 15,
	//! Not subscribed (likely a response to invalid Unsubscribe)
	NotSubscribed = 16,
	//! Session lost (possibly because of protocol::admin::destroy)
	SessionLost = 19,
	
	/* Admin errors. */
	
	//! Unrecognized instruction target.
	UnrecognizedTarget = 50,
	//! Unrecognized instruction command.
	UnrecognizedCommand = 52,
	//! Garbage in instruction, couldn't parse.
	ParseFailure = 55,
	
	/* Auth errors. */ 
	
	//! Wrong password
	PasswordFailure = 45,
	
	/* Name policy errors. */
	
	//! Name is too long.
	TooLong = 72,
	//! Name not unique, go frell it up some more.
	NotUnique = 74,
	
	/* Bad behaving clients */
	//! What? Who?
	UnrecognizedMessage = 80,
	
	/* Completely unexpected error messages */
	
	//! Something bad happened to server, yet it managed to throw this up.
	ServerFarked = 98,
	
	//! Something worse happened, yet the server managed to barf this up.
	/**
	 * Server is unlikely to recover automatically.
	 * Check free memory and some such.
	 */
	ServerFrelled = 99;

} // namespace error

} // namespace protocol

#endif // Protocol_Errors_INCLUDED
