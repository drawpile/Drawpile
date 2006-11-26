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

namespace protocol
{

//! Errors
/**
 * @see protocol::Error message
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Error
 */
namespace error
{

//! Error categories
/**
 * @see protocol::error namespace
 * @see protocol::Error message
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Error
 */
namespace category
{
	//! No category defined.
	const uint8_t None = 0;
} // namespace category

//! Error codes
/**
 * @see protocol::error namespace
 * @see protocol::Error message
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Error
 */
namespace code
{
	//! No error code defined.
	const uint8_t None = 0;
} // namespace code

} // namespace error

} // namespace protocol

#endif // Protocol_Errors_INCLUDED
