/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef Container_H_INCLUDED
#define Container_H_INCLUDED

#include <cstddef> // size_t
#include <cassert>

//! Dynamic type array container
template <typename T>
class Array
{
public:
	//! Copy constructor
	/**
	 * @param[in] array Source array
	 */
	Array(const Array<T>& array) __attribute__((nothrow));
	
	//! Constructor
	/**
	 * @param[in] _data char* string to associate with this Array
	 * @param[in] _size Size of _data
	 */
	Array(T* _data=0, const size_t _size=0) __attribute__((nothrow));
	
	//! Destructor
	/**
	 * @note Deletes contained char* string to avoid memory leak
	 */
	virtual ~Array() __attribute__((nothrow));
	
	virtual void set(T* _data, const size_t _size) __attribute__((nothrow));
	
	//! Type of array
	typedef T type;
	
	//! Pointer to data
	T* ptr;
	//! Number of elements in data
	size_t size;
};

template <typename T>
Array<T>::Array(const Array<T>& array)
	: ptr(array.ptr), size(array.size)
{
}

template <typename T>
Array<T>::Array(T* _data, const size_t _size)
	: ptr(_data), size(_size)
{
	assert((_data == 0 and _size == 0) or (_data != 0 and _size > 0));
}

template <typename T>
Array<T>::~Array()
{
	delete [] ptr;
}

template <typename T>
void Array<T>::set(T* _data, const size_t _size)
{
	delete [] ptr;
	ptr = _data;
	size = _size;
}

#endif // Container_H_INCLUDED
