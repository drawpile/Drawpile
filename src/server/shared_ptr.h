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

#ifndef SharedPointer_INCLUDED
#define SharedPointer_INCLUDED

#include "ref_counted.h"

//! Shared Pointer
template <typename T>
class SharedPtr
	: public ReferenceCounted
{
private:
	//! Internal pointer
	T *m_sptr;
protected:
	//! Default ctor, not usable
	SharedPtr() __attribute__ ((nothrow));
public:
	//! Assign ctor
	SharedPtr(T *ptr) __attribute__ ((nothrow));
	
	//! Copy ctor
	SharedPtr(const SharedPtr<T> &shr_ptr) __attribute__ ((nothrow));
	
	//! Dtor
	~SharedPtr() __attribute__ ((nothrow));
	
	//! Get reference of the shared pointer's object
	T& operator* () __attribute__ ((nothrow));
	//! Access functions and members of the class directly
	T* operator-> () __attribute__ ((nothrow));
};

template <typename T>
SharedPtr<T>::SharedPtr()
{
	/* should never be called */ 
}

template <typename T>
SharedPtr<T>::SharedPtr(T *ptr)
	: ReferenceCounted(),
	m_sptr(ptr)
{
	/* nothing */
}

template <typename T>
SharedPtr<T>::SharedPtr(const SharedPtr<T> &shr_ptr)
	: ReferenceCounted(shr_ptr),
	m_sptr(shr_ptr.m_sptr)
{
	/* nothing */
}

template <typename T>
SharedPtr<T>::~SharedPtr()
{
	if (unique())
		delete m_sptr;
}

template <typename T>
T& SharedPtr<T>::operator* ()
{
	return (*m_sptr);
}

template <typename T>
T* SharedPtr<T>::operator-> ()
{
	return m_sptr;
}

#endif // SharedPointer_INCLUDED
