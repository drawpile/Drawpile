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
	
	//! Joined session.
	Join = 5,
	//! Left session.
	Leave = 6,
	
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
	
	//! Muted by admin.
	Mute = 20,
	//! Unmuted by admin.
	UnMute = 21,
	//! Deafened by admin.
	Deaf = 22,
	//! Undeafened by admin.
	UnDeafe = 23,
	
	//! Forced to observer mode by admin.
	Observer = 25,
	//! Observer mode lifted by admin.
	Participant = 26;

} // namespace user_event

} // namespace protocol

#endif // PROTOCOL_MESSAGE_TYPES_INCLUDED
