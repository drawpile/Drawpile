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

#include "array.h"

template <typename T>
Array<T>::Array()
	: ptr(0), size(0)
{
}

template <typename T>
Array<T>::Array(const Array<T>& array)
	: size(array.size)
{
	//delete [] ptr;
	ptr = array.ptr;
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
