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

#pragma once

#ifndef CircularBuffer_INCLUDED
#define CircularBuffer_INCLUDED

#include "config.h"
#include "array.h"

#include <cstddef> // size_t?

//! Circular buffer.
class Buffer
	: public Array<char>
{
public:
	//! Default constructor
	/**
	 * @param[in] buf char* string to associate with Buffer
	 * @param[in] len size of buf
	 */
	Buffer(char* buf=0, size_t len=0) NOTHROW;
	
	~Buffer() NOTHROW;
	
	//! Moves buffer contents to another buffer struct
	/**
	 * @param[in,out] buffer Buffer to move contents from, source Buffer is emptied.
	 */
	Buffer& operator<< (Buffer& buffer) NOTHROW;
	
	//! Empty the buffer
	void reset() NOTHROW;
	
	//! Resizes the buffer to new size.
	/**
	 * This is expensive since it calls reposition(), which may cause another alloc.
	 *
	 * @param[in] nsize New size in bytes
	 * @param[in] nbuf (Optional) buffer to use, otherwise the new buffer is allocated internally.
	 *
	 * @throw std::bad_alloc
	 */
	void resize(size_t nsize, char* nbuf=0);
	
	//! Copies the still readable portion of the data to the provided buffer
	/**
	 * @param[out] buf char* string to fill with current buffer contents
	 * @param[in] buflen Size of buf
	 */
	bool copy(char* buf, size_t buflen) const NOTHROW NONNULL(1);
	
	//! Assign allocated buffer 'buf' of size 'buflen'.
	/**
	 * Simplifies assignment of new *char 
	 *
	 * @param[in] buf char* string to associate with buffer
	 * @param[in] buflen Size of buf
	 * @param[in] fill (Optional) Number of bytes filled
	 *
	 * @note Old buffer is automatically deleted to avoid memory leak
	 */
	void set(char* buf, size_t buflen, size_t fill=0) NOTHROW NONNULL(1);
	
	//! Repositions data for maximum contiguous _read_ length.
	/**
	 * @throw std::bad_alloc
	 */
	void reposition();
	
	//! Returns true if buffer has no data
	const bool isEmpty() const NOTHROW;
	
	//! Did read of 'n' bytes.
	/**
	 * Tells the buffer how many bytes you did read.
	 * Decrements .left and moves .*rpos by 'n' bytes, possibly resetting the
	 * position back to the beginning of the buffer.
	 *
	 * @param[in] len Number of bytes to move the read pointer by
	 */
	void read(size_t len) NOTHROW;
	
	//! How many bytes can be read.
	/** 
	 * Any read before calling .read() should not go over this many bytes.
	 *
	 * @return number of bytes you can read in one go.
	 */
	size_t canRead() const NOTHROW;
	
	//! Wrote 'n' bytes to buffer.
	/**
	 * Increments .left and moves .*wpos by 'n' bytes. Possibly resets .*wpos back
	 * to beginning of the circular buffer.
	 *
	 * @param[in] len Number of bytes you wrote to buffer, moves write position by this much.
	 */
	void write(size_t len) NOTHROW;
	
	//! How many bytes can be written.
	/**
	 * @return number of bytes you can write in one go.
	 */
	size_t canWrite() const NOTHROW;
	
	//! Returns the number of free bytes in buffer.
	/**
	 * @return Number of bytes still unused
	 */
	size_t free() const NOTHROW;
	
	//! Rewinds wpos and rpos to beginning of the buffer.
	/**
	 * You probably shouldn't use this before you have handled all data in the buffer.
	 * Just check that canRead() returns 0.
	 * 
	 * Effectively allows the next write to occupy largest possible space.
	 */
	void rewind() NOTHROW;
	
	char
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
	
	//! Number of elements left to read (how many elements filled).
	size_t left;
};

#endif // CircularBuffer_INCLUDED
