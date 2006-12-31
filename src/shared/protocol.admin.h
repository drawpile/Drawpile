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
const char separator[] = ":";

//! Arg separator
const char argsep[] = ",";


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
