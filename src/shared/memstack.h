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

*******************************************************************************/

#ifndef MemoryStack_INCLUDED
#define MemoryStack_INCLUDED

#ifdef DEBUG_MEMORY_STACK
	#ifndef NDEBUG
		#include <iostream>
	#endif
#endif

#include <stack> // std::stack

template <class T>
struct MemoryStack
{
	// Memory stack for operators new and delete
	static std::stack<T*> _memstack;
	
	// delete operator
	void operator delete(void* obj) throw()
	{
		if (obj != 0)
			_memstack.push(static_cast<T*>(obj));
		
		#ifdef DEBUG_MEMORY_STACK
		#ifndef NDEBUG
		std::cout << "MemoryStack<>::size()/del = " << _memstack.size() << std::endl;
		#endif
		#endif
	}
	
	// new
	void* operator new(size_t bytes) throw(std::bad_alloc)
	{
		if (_memstack.size() == 0)
		{
			#ifdef DEBUG_MEMORY_STACK
			#ifndef NDEBUG
			std::cout << "MemoryStack<>::size()/NEW" << std::endl;
			#endif
			#endif
			return ::new T;
		}
		else
		{
			#ifdef DEBUG_MEMORY_STACK
			#ifndef NDEBUG
			std::cout << "MemoryStack<>::size()/get = " << _memstack.size() << std::endl;
			#endif
			#endif
			T* obj = _memstack.top();
			_memstack.pop();
			return obj;
		}
	}
};

template <class T>
std::stack<T*> MemoryStack<T>::_memstack;

#endif // MemoryStack_INCLUDED
