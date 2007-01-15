/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

#ifndef Protocol_Tools_INCLUDED
#define Protocol_Tools_INCLUDED

namespace protocol
{

//! Tool identifier codes for ToolInfo message.
namespace tool_type
{

const uint8_t
	//! No tool defined.
	None = 0,
	//! Default brush tool type.
	Brush = 1;

} // namespace tool_type

//! Tool composition modes
namespace tool_mode
{

const uint8_t
	//! No mode set
	None = 0,
	
	//! Normal
	Normal = 1;

} // namespace tool_mode

} // namespace protocol

#endif // Protocol_Tools_INCLUDED
