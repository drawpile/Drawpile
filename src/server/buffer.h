/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

//! Circular buffer.
struct Buffer
{
	//! ctor
	Buffer() throw()
		: data(0),
		wpos(0),
		rpos(0),
		left(0),
		size(0)
	{
	}
	
	//! ctor with buffer assignment
	Buffer(char* buf, size_t buflen) throw()
		: data(buf),
		wpos(buf),
		rpos(buf),
		left(0),
		size(buflen)
	{
		assert(buf != 0);
		assert(buflen > 1);
	}
	
	//! dtor
	~Buffer() throw()
	{
	}
	
	//! Assign allocated buffer 'buf' of size 'buflen'.
	/**
	 * Simplifies assignment of new *char 
	 *
	 * @param buf
	 * @param buflen
	 */
	void setBuffer(char* buf, size_t buflen) throw()
	{
		assert(buf != 0);
		assert(buflen > 1);
		
		data = rpos = wpos = buf;
		
		size = buflen;
		left = 0;
	}
	
	//! Did read of 'n' bytes.
	/**
	 * Tells the buffer how many bytes you did read.
	 * Decrements left and moves .*rpos by 'n' bytes, possibly resetting the
	 * position back to the beginning of the buffer.
	 *
	 * @param len
	 */
	void read(size_t len) throw()
	{
		assert(data != 0);
		assert(size > 1);
		
		assert(len <= canRead());
		
		rpos += len;
		if (rpos+len == data+size)
			rpos = data;
		
		left -= len;
		
		assert(left >= 0UL);
	}
	
	//! How many bytes can be read.
	/** 
	 * Any read before calling .read() should not go over this many bytes.
	 *
	 * @return number of bytes you can read in one go.
	 */
	size_t canRead() const throw()
	{
		assert(data != 0);
		assert(size > 1);
		
		return ((rpos>wpos) ? (data+size) : wpos) - rpos;
	}
	
	//! Wrote 'n' bytes to buffer.
	/**
	 * Increments .left and moves ´*wpos by 'n' bytes. Possibly resets .*wpos back
	 * to beginning of the circular buffer.
	 *
	 * @param len states the number of bytes you wrote to buffer.
	 */
	void write(size_t len) throw()
	{
		assert(data != 0);
		assert(size > 1);
		
		assert(len <= canWrite());
		
		wpos += len;
		if (wpos+len == data+size)
			wpos = data;
		
		left += len;
		
		assert(left <= size);
	}
	
	//! How many bytes can be written.
	/**
	 * @return number of bytes you can write in one go.
	 */
	size_t canWrite() const throw()
	{
		assert(data != 0);
		assert(size > 1);
		
		// this should never return more than .size - .left bytes
		return ((rpos<wpos) ? (data+size) : rpos) - wpos;
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
		//! Number of bytes left to read.
		left,
		//! Total size of the circular buffer (size of .data)
		size;
};

#endif // CircularBuffer_INCLUDED
