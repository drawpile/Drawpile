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
 * board:create 200,200,8,Faust's session
 * \endcode
 *
 * All of the admin instructions emit ACK/Instruction response, but some will
 * give some additional response. Error will be returned in case there was a
 * problem interpreting the instruction or you had insufficient rights for it.
 *
 * Session owner may alter the board she is the owner of.
 * Since observer, mute, deaf and such statuses are hostwide, they can't be
 * enforced on per board basis.
 */
namespace admin
{

//! Target/Command separator
const wchar_t separator = ":";

//! Arg separator
const wchar_t argsep = ",";

//! Set board as instruction target.
const wchar_t board[] = "board";

//! Set user as instruction target.
const wchar_t user[] = "user";

//! Set server as instruction target.
const wchar_t server[] = "server";

//! Bool.true
const wchar_t bool_true[] = "true";

//! Bool.false
const wchar_t bool_false[] = "false";

//! Board/Create
/**
 * \code
 * board:create [width],[height],[user_limit],[name]
 * \endcode
 *
 * Response: BoardInfo
 */
const wchar_t create[] = "create";

//! Board/Alter
/**
 * \code
 * board:alter [board_id] [width],[height],[user_limit],[name]
 * \endcode
 *
 * Board size can only be increased!
 */
const wchar_t alter[] = "alter";

//! Board/Destroy
/**
 * \code
 * board:destroy [board_id]
 * \endcode
 */
const wchar_t destroy[] = "destroy";

//! Board/Persist
/**
 * \code
 * board:persist [board_id]
 * \endcode
 *
 * Response: BoardInfo
 */
const wchar_t persist[] = "persist";

//! User/Kick
/**
 * \code
 * user:kick [user_id]
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const wchar_t kick[] = "kick";

//! User/Observe
/**
 * \code
 * user:observe [user_id] [bool]
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const wchar_t observe[] = "observe";

//! User/Mute
/**
 * \code
 * user:mute [user_id] [bool]
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const wchar_t mute[] = "mute";

//! User/Deafen
/**
 * \code
 * user:deaf [user_id] [bool]
 * \endcode
 *
 * Response: UserInfo with updated status
 */
const wchar_t deaf[] = "deaf";

} // namespace admin

} // namespace protocol

#endif // Protocol_Errors_INCLUDED
