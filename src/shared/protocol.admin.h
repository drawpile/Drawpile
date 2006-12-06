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

#ifndef Protocol_Admin_INCLUDED
#define Protocol_Admin_INCLUDED

namespace protocol
{

//! Admin Instruction message command words.
/**
 * For now, the format of admin instructions is as follows.
 * \code
 * [instruction target][separator][command] [instruction specific data]
 * \endcode
 *
 * For example:
 * \code
 * session:create 200,200,8,Faust's session
 * \endcode
 *
 * All of the admin instructions emit ACK/Instruction response, but some will
 * give some additional response. Error will be returned in case there was a
 * problem interpreting the instruction or you had insufficient rights for it.
 *
 * Session owner may alter the session she is the owner of.
 * Since observer, mute, deaf and such statuses are hostwide, they can't be
 * enforced on per-session basis.
 */
namespace admin
{

//! Target/Command separator
const char separator = ":";

//! Arg separator
const char argsep = ",";

//! Set user as instruction target.
const char user[] = "user";

//! Set server as instruction target.
const char server[] = "server";

//! Set session user as instruction target.
const char session[] = "session";

//! Bool.true
const char bool_true[] = "true";

//! Bool.false
const char bool_false[] = "false";

//! Create session
/**
 * \code
 * session:create width,height,user_limit,name
 * \endcode
 *
 * Response: SessionInfo
 */
const char create[] = "create";

//! Alter session
/**
 * \code
 * session:alter session_id width,height,user_limit,name
 * \endcode
 *
 * Session's board size can only be increased!
 */
const char alter[] = "alter";

//! Destroy session
/**
 * \code
 * session:destroy session_id
 * \endcode
 */
const char destroy[] = "destroy";

//! Make session persistent
/**
 * \code
 * session:persist session_id
 * \endcode
 *
 * Response: SessionInfo
 */
const char persist[] = "persist";




//! Kick user
/**
 * \code
 * user:kick session_id user_id
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const char kick[] = "kick";

//! User/Observe
/**
 * \code
 * user:observe session_id user_id bool
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const char observe[] = "observe";

//! Mute user
/**
 * \code
 * user:mute session_id user_id bool
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const char mute[] = "mute";

//! User/Deafen
/**
 * \code
 * user:deaf session_id user_id bool
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const char deaf[] = "deaf";

} // namespace admin

} // namespace protocol

#endif // Protocol_Errors_INCLUDED
