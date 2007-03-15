/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef CircularBuffer_INCLUDED
#define CircularBuffer_INCLUDED

#include "../../config.h"

#ifndef NDEBUG
	#include <iostream>
#endif
#include <cstddef> // size_t?
#include <cassert>

#include "../shared/memstack.h"

//! Circular buffer.
struct Buffer
	//: MemoryStack<Buffer>
{
	//! ctor with buffer assignment
	Buffer(char* buf=0, const size_t len=0) throw()
		: data(buf),
		wpos(buf),
		rpos(buf),
		left(0),
		size(len)
	{
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::Buffer(*buf, " << len << ")" << std::endl;
		#endif
		#endif
		
		assert((!buf and len == 0) or (buf and len > 0));
	}
	
	//! dtor
	~Buffer() throw()
	{
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::~Buffer()" << std::endl;
		#endif
		#endif
		
		#ifndef CBUFFER_UNMANAGED
		delete [] data;
		#endif
	}
	
	//! Moves buffer contents to another buffer struct
	Buffer& operator<< (Buffer& buffer) throw()
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
	
	void reset() throw()
	{
		delete [] data;
		data = wpos = rpos = 0;
		left = size = 0;
	}
	
	//! Resizes the buffer to new size.
	/**
	 * This is expensive since it calls reposition(), which may cause another alloc.
	 */
	void resize(size_t nsize, char* nbuf=0)
	{
		assert(nsize > 0);
		assert(nsize >= left);
		
		// create new array if it doesn't exist.
		if (nbuf == 0)
			nbuf = new char[nsize];
		
		reposition();
		
		size_t off = canRead();
		memmove(nbuf, data, off);
		
		#ifndef CBUFFER_UNMANAGED
		// causes memory leak otherwise
		delete [] data;
		data = 0;
		#endif
		
		// set the buffer as our current one.
		setBuffer(nbuf, nsize);
		
		// move the offset to the real position.
		write(off);
	}
	
	//! Assign allocated buffer 'buf' of size 'buflen'.
	/**
	 * Simplifies assignment of new *char 
	 *
	 * @param buf
	 * @param buflen
	 */
	void setBuffer(char* buf, const size_t buflen) throw()
	{
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::setBuffer(*buf, " << buflen << ")" << std::endl;
		#endif
		#endif
		
		assert(buf != 0);
		assert(buflen > 1);
		
		#ifndef CBUFFER_UNMANAGED
		delete [] data;
		#endif
		
		data = rpos = wpos = buf;
		
		size = buflen;
		left = 0;
	}
	
	//! Repositions data for maximum contiguous read length.
	void reposition()
	{
		if (rpos == data)
			return;
		
		if (left == 0)
		{
			rewind();
			return;
		}
		
		#ifndef NDEBUG
		size_t oleft = left;
		#endif
		
		if (canRead() < left)
		{ // we can't read all in one go
			// create temporary
			size_t oversize = left-canRead();
			size_t tmp_len = canRead();
			char* tmp = new char[tmp_len];
			// move data at the end to the temporary
			memmove(tmp, rpos, tmp_len);
			// move write pointer to target address
			wpos = data + tmp_len;
			// move data at the beginning to after
			memmove(wpos, data, oversize);
			// move write pointer to end of actual data.
			wpos = wpos + oversize;
			// move data in temporary to the beginning
			rpos = data;
			memmove(rpos, tmp, tmp_len);
			// delete temporary
			delete [] tmp;
		}
		else
		{ // we can read all the data in one go.
			// just move the data to the beginning.
			size_t off = canRead();
			memmove(data, rpos, off);
			// adjust rpos and wpos
			rewind();
			write(off);
		}
		
		assert(oleft == left);
	}
	
	//! Did read of 'n' bytes.
	/**
	 * Tells the buffer how many bytes you did read.
	 * Decrements .left and moves .*rpos by 'n' bytes, possibly resetting the
	 * position back to the beginning of the buffer.
	 *
	 * @param len
	 */
	void read(const size_t len) throw()
	{
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::read(" << len << ")" << std::endl;
		#endif
		#endif
		
		assert(data != 0);
		assert(size > 1);
		
		assert(len <= canRead());
		
		rpos += len;
		assert(rpos <= data+size);
		
		left -= len;
		assert(left >= 0);
		
		if (rpos == data+size)
			rpos = data;
	}
	
	//! How many bytes can be read.
	/** 
	 * Any read before calling .read() should not go over this many bytes.
	 *
	 * @return number of bytes you can read in one go.
	 */
	size_t canRead() const throw()
	{
		if (0) {
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::canRead()" << std::endl;
		#endif
		#endif
		}
		
		assert(data != 0);
		assert(size > 1);
		
		if (left == 0)
			return 0;
		
		if (wpos > rpos)
			return wpos - rpos;
		else
			return (data+size) - rpos;
		
		return 0;
	}
	
	//! Wrote 'n' bytes to buffer.
	/**
	 * Increments .left and moves .*wpos by 'n' bytes. Possibly resets .*wpos back
	 * to beginning of the circular buffer.
	 *
	 * @param len states the number of bytes you wrote to buffer.
	 */
	void write(const size_t len) throw()
	{
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::write(" << len << ")" << std::endl;
		#endif
		#endif
		
		assert(data != 0);
		assert(size > 1);
		
		assert(len <= canWrite());
		
		wpos += len; // increment wpos pointer
		left += len; // increase number of bytes left to read
		
		// Set wpos to beginning of buffer if it reaches its end.
		if (wpos == data+size)
			wpos = data;
		
		assert(left <= size);
	}
	
	//! How many bytes can be written.
	/**
	 * @return number of bytes you can write in one go.
	 */
	size_t canWrite() const throw()
	{
		if (0) {
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Buffer::canWrite()" << std::endl;
		#endif
		#endif
		}
		
		assert(data != 0);
		assert(size > 1);
		
		if (left == size) return 0;
		
		// this should never return more than .size - .left bytes
		if (wpos < rpos)
			return rpos - wpos;
		else
			return (data+size) - wpos;
		
		return 0;
	}
	
	//! Returns the number of free bytes in buffer.
	size_t free()
	{
		return size - left;
	}
	
	//! Rewinds wpos and rpos to beginning of the buffer.
	/**
	 * You probably shouldn't use this before you have handled all data in the buffer.
	 * Just check that canRead() returns 0.
	 * 
	 * Effectively allows the next write to occupy largest possible space.
	 */
	void rewind() throw()
	{
		assert(data != 0);
		assert(size > 0);
		wpos = rpos = data;
		left = 0;
	}
	
	char
		//! Circular buffer data. DON'T TOUCH (unless you're scrapping the buffer)
		*data,
		//! Writing position.
		/**
		 * Write directly to this.
		 * 
		 * Tell write() how many bytes you did after each write, unless you want to
		 * overwrite those bytes, but that makes no sense since you can't use the
		 * rpos for reading them. So for the sake of consistency, don't do it!
		 */
		*wpos,
		//! Reading position.
		/**
		 * Read directly from this.
		 *
		 * Tell read() how many bytes you did after each read (unless you want to
		 * re-read the same part and keep it in the buffer, but do note that this
		 * limits the write capability of the buffer until you continue reading).
		 */
		*rpos;
	
	size_t
		//! Number of bytes left to read (how many bytes filled).
		left,
		//! Total size of the circular buffer (size of .data)
		size;
};

#endif // CircularBuffer_INCLUDED
