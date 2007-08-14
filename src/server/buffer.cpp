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

#include "buffer.h"

#include <memory>
#include <cassert>

Buffer::Buffer(char* buf, const size_t len)
	: Array<char>(buf, len),
	wpos(buf),
	rpos(buf),
	left(0)
{
}

Buffer::~Buffer()
{
}

Buffer& Buffer::operator<< (Buffer& buffer)
{
	delete [] ptr;
	
	ptr = buffer.ptr;
	wpos = buffer.wpos;
	rpos = buffer.rpos;
	left = buffer.left;
	size = buffer.size;
	
	buffer.ptr = 0;
	
	buffer.reset();
	
	return *this;
}

void Buffer::reset()
{
	delete [] ptr;
	ptr = wpos = rpos = 0;
	left = size = 0;
}

void Buffer::resize(size_t nsize, char* nbuf)
{
	assert(nsize > 0);
	assert(nsize >= left);
	
	// create new array if it doesn't exist.
	if (nbuf == 0)
		nbuf = new char[nsize];
	
	copy(nbuf, nsize);
	
	const size_t off = left;
	
	// set the buffer as our current one.
	set(nbuf, nsize);
	
	// move the offset to the real position.
	write(off);
}

bool Buffer::copy(char* buf, const size_t buflen) const
{
	assert(buflen >= left);
	assert(buf != 0);
	
	if (buflen < left)
		return false;
	
	const size_t chunk1 = canRead();
	if (chunk1 < left)
		memcpy(buf+chunk1, ptr, left - chunk1);
	memcpy(buf, rpos, chunk1);
	
	return true;
}

void Buffer::set(char* buf, const size_t buflen, const size_t fill)
{
	assert(buf != 0);
	assert(buflen > 1);
	assert(fill <= buflen);
	
	Array<char>::set(buf, buflen);
	
	ptr = rpos = wpos = buf;
	
	if (fill < buflen)
		wpos += fill;
	
	left = fill;
}

void Buffer::reposition()
{
	if (rpos == ptr) // already optimally positioned
	{
		// do nothing
	}
	else if (left == 0)
	{
		rewind();
	}
	else if (canRead() < left)
	{ // we can't read all in one go
		const size_t chunk1 = canRead(), chunk2 = left - canRead();
		
		if (left <= free())
		{ // there's more free space than used
			// move second chunk to free space between the chunks
			wpos = ptr+chunk1;
			memcpy(wpos, ptr, chunk2);
			
			// move first chunk to the beginning of the buffer
			memcpy(ptr, rpos, chunk1);
			
			// adjust wpos to the end of second chunk
			wpos += chunk2;
		}
		else
		{
			// create temporary
			char* tmp = new char[chunk2];
			
			// move second chunk to temporary
			memcpy(tmp, ptr, chunk2);
			// move first chunk to the front of buffer
			memcpy(ptr, rpos, chunk1);
			
			// move wpos to the end of first chunk
			wpos = ptr+chunk1;
			
			// move second chunk to wpos
			memcpy(wpos, tmp, chunk2);
			
			// cleanup
			delete [] tmp;
			
			// move wpos to the end of allocated space
			wpos += chunk2;
		}
		
		// set rpos to the beginning of the buffer
		rpos = ptr;
	}
	else
	{ // we can read all the data in one go.
		const size_t chunk1 = canRead();
		// just move the data to the beginning.
		memmove(ptr, rpos, chunk1);
		
		// set rpos to the beginning of the buffer
		rpos = ptr;
		
		// adjust rpos and wpos
		wpos = ptr+chunk1;
	}
}

const bool Buffer::isEmpty() const
{
	return (left == 0);
}

void Buffer::read(const size_t len)
{
	assert(ptr != 0);
	assert(size > 1);
	
	assert(len > 0);
	assert(len <= canRead());
	
	rpos += len;
	assert(rpos <= ptr+size);
	
	left -= len;
	assert(left >= 0);
	
	if (rpos == ptr+size)
		rpos = ptr;
}

size_t Buffer::canRead() const
{
	assert(ptr != 0);
	assert(size > 1);
	
	if (left == 0)
		return 0;
	else if (wpos > rpos)
		return wpos - rpos;
	else
		return (ptr+size) - rpos;
	
	return 0;
}

void Buffer::write(const size_t len)
{
	assert(ptr != 0);
	assert(size > 1);
	
	assert(len > 0);
	assert(len+left <= size);
	
	wpos += len; // increment wpos pointer
	left += len; // increase number of bytes left to read
	
	// Set wpos to beginning of buffer if it reaches its end.
	if (wpos == ptr+size)
		wpos = ptr;
}

size_t Buffer::canWrite() const
{
	assert(ptr != 0);
	assert(size > 1);
	
	if (rpos > wpos)
		return rpos - wpos;
	else
		return (ptr+size) - wpos;
}

size_t Buffer::free() const
{
	return size - left;
}

void Buffer::rewind()
{
	assert(ptr != 0);
	assert(size > 0);
	
	wpos = rpos = ptr;
	left = 0;
}
