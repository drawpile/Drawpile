/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior writeitten authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef RefCounted_INCLUDED
#define RefCounted_INCLUDED

#include "types.h"

//! Reference Counted base class
/**
 * @bug Classes implementations ReferenceCounted need to check for unique() in dtor to
 * perform final cleaning as this class does NOT call any inheritable destruction function.
 */
class ReferenceCounted
{
protected:
	//! Reference count
	ulong *rc_ref_count;
	
	//! Default ctor
	ReferenceCounted();
	
	//! Copy ctor
	ReferenceCounted(const ReferenceCounted &refcounted) NOTHROW;
	
	//! Dtor
	virtual ~ReferenceCounted() NOTHROW;
public:
	//! Return reference count
	ulong count() const NOTHROW;
	//! Return true if unique (reference count is 1)
	bool unique() const NOTHROW;
};

#endif // RefCounted_INCLUDED
