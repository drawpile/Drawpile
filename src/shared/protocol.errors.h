/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

#ifndef Protocol_Errors_INCLUDED
#define Protocol_Errors_INCLUDED

#include <stdint.h>
#include <stdexcept>

namespace protocol
{

//! Scrambled input buffer.
struct scrambled_buffer
	: std::exception
{
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
	
	//! User limit reached
	UserLimit = 7,
	//! Session limit reached
	SessionLimit = 8,
	
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
	
	//! Invalid data value in the message
	InvalidData = 50,
	//! Unrecognized instruction command.
	UnrecognizedCommand = 52,
	//! Garbage in instruction, couldn't parse.
	ParseFailure = 55,
	
	/* Canvas related */
	
	//! Too small canvas
	TooSmall = 57,
	
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
