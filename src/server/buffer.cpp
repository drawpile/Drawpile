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

Buffer::Buffer(char* buf, const size_t len) throw()
	: data(buf),
	wpos(buf),
	rpos(buf),
	left(0),
	size(len)
{
}

Buffer::~Buffer() throw() { }

Buffer& Buffer::operator<< (Buffer& buffer) throw()
{
	delete [] data;
	
	data = buffer.data;
	wpos = buffer.wpos;
	rpos = buffer.rpos;
	left = buffer.left;
	size = buffer.size;
	
	buffer.data = 0;
	
	buffer.reset();
	
	return *this;
}

void Buffer::reset() throw()
{
	delete [] data;
	data = wpos = rpos = 0;
	left = size = 0;
}

void Buffer::resize(size_t nsize, char* nbuf) throw(std::bad_alloc)
{
	assert(nsize > 0);
	assert(nsize >= left);
	
	// create new array if it doesn't exist.
	if (nbuf == 0)
		nbuf = new char[nsize];
	
	getBuffer(nbuf, nsize);
	
	const size_t off = left;
	
	// set the buffer as our current one.
	setBuffer(nbuf, nsize);
	
	// move the offset to the real position.
	write(off);
}

bool Buffer::getBuffer(char*& buf, const size_t buflen) const throw()
{
	assert(buflen >= left);
	assert(buf != 0);
	
	if (buflen < left)
		return false;
	
	const size_t chunk1 = canRead();
	if (chunk1 < left)
		memcpy(buf+chunk1, data, left - chunk1);
	memcpy(buf, rpos, chunk1);
	
	return true;
}

void Buffer::setBuffer(char* buf, const size_t buflen, const size_t fill) throw()
{
	assert(buf != 0);
	assert(buflen > 1);
	assert(fill <= buflen);
	
	delete [] data;
	
	data = rpos = wpos = buf;
	
	size = buflen;
	
	if (fill < buflen)
		wpos += fill;
	
	left = fill;
}

void Buffer::reposition() throw(std::bad_alloc)
{
	if (rpos == data) // already optimally positioned
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
			wpos = data+chunk1;
			memcpy(wpos, data, chunk2);
			
			// move first chunk to the beginning of the buffer
			memcpy(data, rpos, chunk1);
			
			// adjust wpos to the end of second chunk
			wpos += chunk2;
		}
		else
		{
			// create temporary
			char* tmp = new char[chunk2];
			
			// move second chunk to temporary
			memcpy(tmp, data, chunk2);
			// move first chunk to the front of buffer
			memcpy(data, rpos, chunk1);
			
			// move wpos to the end of first chunk
			wpos = data+chunk1;
			
			// move second chunk to wpos
			memcpy(wpos, tmp, chunk2);
			
			// cleanup
			delete [] tmp;
			
			// move wpos to the end of allocated space
			wpos += chunk2;
		}
		
		// set rpos to the beginning of the buffer
		rpos = data;
	}
	else
	{ // we can read all the data in one go.
		const size_t chunk1 = canRead();
		// just move the data to the beginning.
		memmove(data, rpos, chunk1);
		
		// set rpos to the beginning of the buffer
		rpos = data;
		
		// adjust rpos and wpos
		wpos = data+chunk1;
	}
}

const bool Buffer::isEmpty() const throw()
{
	return (left == 0);
}

void Buffer::read(const size_t len) throw()
{
	assert(data != 0);
	assert(size > 1);
	
	assert(len > 0);
	assert(len <= canRead());
	
	rpos += len;
	assert(rpos <= data+size);
	
	left -= len;
	assert(left >= 0);
	
	if (rpos == data+size)
		rpos = data;
}

size_t Buffer::canRead() const throw()
{
	assert(data != 0);
	assert(size > 1);
	
	if (left == 0)
		return 0;
	else if (wpos > rpos)
		return wpos - rpos;
	else
		return (data+size) - rpos;
	
	return 0;
}

void Buffer::write(const size_t len) throw()
{
	assert(data != 0);
	assert(size > 1);
	
	assert(len > 0);
	assert(len <= canWrite());
	
	wpos += len; // increment wpos pointer
	left += len; // increase number of bytes left to read
	
	assert(left <= size);
	
	// Set wpos to beginning of buffer if it reaches its end.
	if (wpos == data+size)
		wpos = data;
}

size_t Buffer::canWrite() const throw()
{
	assert(data != 0);
	assert(size > 1);
	
	if (rpos > wpos)
		return rpos - wpos;
	else
		return (data+size) - wpos;
}

size_t Buffer::free() const throw()
{
	return size - left;
}

void Buffer::rewind() throw()
{
	assert(data != 0);
	assert(size > 0);
	wpos = rpos = data;
	left = 0;
}
