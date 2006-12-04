/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

/** circular buffer of sorts */
struct Buffer
{
	Buffer()
		: data(0),
		wpos(0),
		rpos(0),
		left(0),
		size(0)
	{
	}
	
	Buffer(char* buf, size_t buflen)
		: data(buf),
		wpos(buf),
		rpos(buf),
		left(0),
		size(buflen)
	{
	}
	
	~Buffer()
	{
	}
	
	//! Assign allocated buffer 'buf' of size 'buflen'.
	/**
	 * Simplifies assignment of new *char 
	 *
	 * @param buf
	 * @param buflen
	 */
	void setBuffer(char* buf, size_t buflen)
	{
		assert(buf != 0);
		assert(buflen > 1);
		
		data = buf;
		rpos = buf;
		wpos = buf;
		
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
	void read(size_t len)
	{
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
	size_t canRead() const
	{
		return ((rpos>wpos) ? (data+size) : wpos) - rpos;
	}
	
	//! Wrote 'n' bytes to buffer.
	/**
	 * Increments .left and moves ´*wpos by 'n' bytes. Possibly resets .*wpos back
	 * to beginning of the circular buffer.
	 *
	 * @param len states the number of bytes you wrote to buffer.
	 */
	void write(size_t len)
	{
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
	size_t canWrite()
	{
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

#endif // BUFFER_H_INCLUDED
