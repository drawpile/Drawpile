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

#ifndef PROTOCOL_HELPER_H_INCLUDED
#define PROTOCOL_HELPER_H_INCLUDED

#include <stdexcept> // std::bad_alloc

namespace protocol
{

struct Message;

//! Outputs the name of the message type in terminal
/**
 * @note Does not do anything in release builds
 *
 * @param[in] type Message type to be generated (see protocol::type).
 */
void msgName(const unsigned char type) NOTHROW ATTRPURE;

//! Get new message struct.
/**
 * Allocates memory for specific message type and returns pointer to it.
 * Supplied as a helper function to simplify some tasks.
 *
 * @param[in] type Message type to be generated (see protocol::type).
 *
 * @return Appropriate message struct allocated with operator new and casted to base Message class.
 * @retval 0 if \b type was not recognized
 *
 * @throw std::bad_alloc
 */
Message* getMessage(const unsigned char type) ATTRPURE;

} // namespace protocol

#endif // PROTOCOL_HELPER_H_INCLUDED
