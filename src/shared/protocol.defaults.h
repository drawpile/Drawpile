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

//! Default listening port.
const uint16_t default_port = 27750;

} // namespace protocol

#endif // Protocol_Defaults_INCLUDED
