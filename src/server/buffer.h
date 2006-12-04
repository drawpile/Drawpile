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
	
	/** */
	
	void setBuffer(char* buf, size_t buflen)
	{
		assert(buf != 0);
		
		data = buf;
		rpos = buf;
		wpos = buf;
		
		size = buflen;
		left = 0;
	}
	
	/** */
	void read(size_t len)
	{
		assert(len <= canRead());
		
		rpos += len;
		if (rpos+len == data+size)
			rpos = data;
		
		left -= len;
		
		assert(left >= 0);
	}
	
	/** 
	 * @return number of bytes you can read in one go.
	 */
	size_t canRead()
	{
		return ((rpos>wpos) ? (data+size) : wpos) - rpos;
	}
	
	/**
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
	
	/** */
	size_t canWrite()
	{
		return ((rpos<wpos) ? (data+size) : rpos) - wpos;
	}
	
	char *data, *wpos, *rpos;
	size_t left, size;
};

#endif // BUFFER_H_INCLUDED
