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

#include "config.h"
#include "container.h"

#include <stdexcept>
#include <cstddef> // size_t?
#include <cassert>

//! Circular buffer.
struct Buffer
{
	//! ctor with buffer assignment
	Buffer(char* buf=0, const size_t len=0) throw();
	
	//! dtor
	~Buffer() throw();
	
	//! Moves buffer contents to another buffer struct
	Buffer& operator<< (Buffer& buffer) throw();
	
	void reset() throw();
	
	//! Resizes the buffer to new size.
	/**
	 * This is expensive since it calls reposition(), which may cause another alloc.
	 */
	void resize(size_t nsize, char* nbuf=0) throw(std::bad_alloc);
	
	//! Copies the still readable portion of the data to the provided buffer
	bool getBuffer(char*& buf, const size_t buflen) const throw();
	
	//! Assign allocated buffer 'buf' of size 'buflen'.
	/**
	 * Simplifies assignment of new *char 
	 *
	 * @param buf
	 * @param buflen
	 * @param fill
	 */
	void setBuffer(char* buf, const size_t buflen, const size_t fill=0) throw();
	
	//! Repositions data for maximum contiguous _read_ length.
	void reposition() throw(std::bad_alloc);
	
	const bool isEmpty() const throw();
	
	//! Did read of 'n' bytes.
	/**
	 * Tells the buffer how many bytes you did read.
	 * Decrements .left and moves .*rpos by 'n' bytes, possibly resetting the
	 * position back to the beginning of the buffer.
	 *
	 * @param len
	 */
	void read(const size_t len) throw();
	
	//! How many bytes can be read.
	/** 
	 * Any read before calling .read() should not go over this many bytes.
	 *
	 * @return number of bytes you can read in one go.
	 */
	size_t canRead() const throw();
	
	//! Wrote 'n' bytes to buffer.
	/**
	 * Increments .left and moves .*wpos by 'n' bytes. Possibly resets .*wpos back
	 * to beginning of the circular buffer.
	 *
	 * @param len states the number of bytes you wrote to buffer.
	 */
	void write(const size_t len) throw();
	
	//! How many bytes can be written.
	/**
	 * @return number of bytes you can write in one go.
	 */
	size_t canWrite() const throw();
	
	//! Returns the number of free bytes in buffer.
	size_t free() const throw();
	
	//! Rewinds wpos and rpos to beginning of the buffer.
	/**
	 * You probably shouldn't use this before you have handled all data in the buffer.
	 * Just check that canRead() returns 0.
	 * 
	 * Effectively allows the next write to occupy largest possible space.
	 */
	void rewind() throw();
	
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
		//! Number of elements left to read (how many elements filled).
		left,
		//! Total size of the circular buffer (size of .data)
		size;
};

#endif // CircularBuffer_INCLUDED
