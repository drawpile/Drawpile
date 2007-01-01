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

#ifndef Protocol_Flags_INCLUDED
#define Protocol_Flags_INCLUDED

namespace protocol
{

//! Message modifiers
/**
 * http://drawpile.sourceforge.net/wiki/index.php/Protocol#Message_modifiers
 */
namespace message
{

const uint8_t
	//! No message modifiers.
	None = 0x00,
	//! Has user identifier
	isUser = 0x01,
	//! Has session identifier
	isSession = 0x02,
	//! Is bundling
	isBundling = 0x04,
	//! Is directed by SessionSelect message.
	isSelected = 0x08;
}

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
	
	//! Chat extension
	Chat = 0x01,
	
	//! Shared palette extension
	Palette = 0x02,
	
	//! Deflate extension
	Deflate = 0x08;

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
	
	//! Enforces unique user and session names.
	EnforceUnique = 0x01,
	
	//! Multi-session support
	MultiSession = 0x04,
	
	//! Strings use UTF-16 format instead of default UTF-8.
	WideStrings = 0x08,
	
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
 * @see protocol::SessionInfo message
 */
namespace session
{

const uint8_t
	//! No session flags
	None = user::None,
	
	//! Users join the session in observer mode.
	Observer = user::Observer,
	
	//! Users join the session muted.
	Mute = user::Mute,
	//! Users join the session deafened.
	Deaf = user::Deaf;

} // namespace session

} // namespace protocol

#endif // Protocol_Flags_INCLUDED
