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

#ifndef Protocol_Types_INCLUDED
#define Protocol_Types_INCLUDED

namespace protocol
{

//! Message type identifiers.
/**
 * @see protocol::Message, base for all real messages.
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Messages
 */
namespace type
{

const uint8_t
	//! No type
	None = 0,
	
	//! for Identifier.
	Identifier = 200,
	
	//! for StrokeInfo.
	StrokeInfo = 1,
	//! for StrokeEnd.
	StrokeEnd = 2,
	//! for ToolInfo.
	ToolInfo = 3,
	
	//! for Authentication.
	Authentication = 25,
	//! for Password.
	Password = 26,
	
	//! for Synchronize.
	Synchronize = 10,
	//! for Raster.
	Raster = 11,
	//! for SyncWait.
	SyncWait = 12,
	
	//! for Subscribe.
	Subscribe = 30,
	//! for Unsubscribe.
	Unsubscribe = 31,
	//! for Session select.
	SessionSelect = 37,
	
	//! for Instruction.
	Instruction = 77,
	//! for ListSessions.
	ListSessions = 72,
	//! for Cancel.
	Cancel = 73,
	
	//! for UserInfo.
	UserInfo = 80,
	//! for HostInfo.
	HostInfo = 81,
	//! for SessionInfo.
	SessionInfo = 82,
	
	//! for Acknowledgement.
	Acknowledgement = 20,
	//! for Error.
	Error = 21,
	
	//! for Deflate.
	Deflate = 191,
	//! for Chat.
	Chat = 192,
	
	//! for Palette.
	Palette = 91;

} // namespace type

//! User events
/**
 * @see protocol::UserInfo message.
 */
namespace user_event
{

const uint8_t
	//! No user event.
	None = 0,
	
	//! Logging in.
	Login = 1,
	
	/** Session specific */
	
	//! Joined session.
	Join = 5,
	//! Left session.
	Leave = 6,
	
	/** Verbose leave reasons */
	
	//! User disconnected. Indication of poorly behaving client.
	Disconnect = 10,
	//! Broken pipe / lost connection.
	BrokenPipe = 11,
	//! Timed out
	TimedOut = 12,
	//! Dropped by server.
	Dropped = 13,
	//! Kicked by admin.
	Kicked = 14,
	//! Client is behaving badly / protocol violation
	Violation = 17,
	
	/** User mode changes */
	
	//! Muted by admin.
	Mute = 20,
	//! Unmuted by admin.
	UnMute = 21,
	//! Deafened by admin.
	Deaf = 22,
	//! Undeafened by admin.
	UnDeafen = 23,
	
	//! Forced to observer mode by admin.
	Observer = 25,
	//! Observer mode lifted by admin.
	Participant = 26;

} // namespace user_event

} // namespace protocol

#endif // PROTOCOL_MESSAGE_TYPES_INCLUDED
