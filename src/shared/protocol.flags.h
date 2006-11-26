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

#ifndef Protocol_Flags_INCLUDED
#define Protocol_Flags_INCLUDED

namespace protocol
{

//! Protocol extension flags.
/**
 * @see protocol::Identifier message
 * @see protocol::HostInfo message
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Extensions
 */
namespace extensions
{

const uint8_t
	//! No supported extensions.
	None = 0x00,
	
	//! Deflate extension.
	Deflate = 0x01,
	
	//! Chat extension
	Chat = 0x04;

} // namespace extensions

//! Operation flags.
/**
 * @see protocol::HostInfo message
 */
namespace requirements
{

const uint8_t
	//! No special requirements.
	None = 0x00,
	
	//! Enforces unique user and board names.
	EnforceUnique = 0x01,
	
	//! Server does not allow global Chat messages.
	NoGlobalChat = 0x10;

} // namespace operation

//! User mode flags.
/**
 * @see protocol::UserInfo message
 */
namespace user
{

const uint8_t
	//! Null user mode.
	None = 0x00,
	
	//! User is identified as admin by the server (may use Instruction messages).
	Administrator = 0x01,
	
	//! User is not able to draw.
	Observer = 0x08,
	
	//! User may not send Chat messages.
	Mute = 0x10,
	//! User can't see Chat messages.
	Deaf = 0x20;

} // namespace user

//! Session specific flags
/**
 * @see protocol::BoardInfo message
 */
namespace session
{

const uint8_t
	//! No session flags
	None = 0x00,
	
	//! Users join the session in observer mode.
	Observer = 0x08,
	
	//! Users join the session muted.
	Mute = 0x10,
	
	//! Users join the session deafened.
	Deaf = 0x20;

} // namespace session

} // namespace protocol

#endif // Protocol_Flags_INCLUDED
