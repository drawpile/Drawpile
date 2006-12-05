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

#ifndef Protocol_Defaults_INCLUDED
#define Protocol_Defaults_INCLUDED

namespace protocol
{

//! Session identifier for no particular session (global).
const uint8_t Global = 0;

/* RGB color size */
const size_t RGB_size = sizeof(uint32_t) - sizeof(uint8_t);

/* RGBA color size */
const size_t RGBA_size = sizeof(uint32_t);

//! The only invalid message count value.
const uint8_t null_count = 0;

//! No user identifier defined (not really null, as you can see).
const uint8_t null_user = 255;

//! No protocol revision defined.
const uint16_t null_revision = 0;

//! No feature implementation level defined.
const uint16_t null_implementation = 0;

/* only used by Identifier message */
const uint8_t identifier_size = 8;

//! Seed size for passwords (bytes).
const uint8_t password_seed_size = 4;
//! Size of the password hash (bytes).
const uint8_t password_hash_size = 20;

//! Protocol identifier string.
const char identifierString[identifier_size] = {'D','r','a','w','P','i','l','e'};

} // namespace protocol

#endif // Protocol_Defaults_INCLUDED
