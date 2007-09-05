/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef Sockets_INCLUDED
#define Sockets_INCLUDED

#include "config.h"

#include "address.h"
#include "socket.porting.h"
#include "socket.errors.h"
#include "socket.types.h" // fd_t

#include "descriptor.h"

//! Socket abstraction
class Socket
	: public Descriptor<fd_t>
{
public:
	/*
	static const int FullShutdown;
	static const int ShutdownWriting;
	static const int ShutdownReading;
	*/
protected:
	//! Address (local for listening, remote for outgoing)
	Address m_addr;
	
	//! Last error number (from errno or equivalent)
	int s_error;
public:
	//! Default constructor
	/**
	 * @param[in] nsock FD to associate with this Socket
	 */
	Socket(const fd_t nsock=socket_error::InvalidHandle) __attribute__ ((nothrow));
	
	//! More advanced constructor
	/**
	 * @param[in] nsock FD to associate with this Socket
	 * @param[in] saddr Address to associate with this Socket
	 */
	Socket(const fd_t nsock, const Address& saddr) __attribute__ ((nothrow));
	
	//! Copy ctor
	Socket(const Socket& socket) __attribute__ ((nothrow));
	
	~Socket() __attribute__ ((nothrow));
	
	//! Create new socket
	fd_t create() __attribute__ ((nothrow));
	
	//! Accept new connection.
	/**
	 * @return Socket if new connection was accepted
	 * @note (Socket.getFD() == socket_error::InvalidHandle) if no new connection was accepted
	 */
	Socket accept()  __attribute__ ((nothrow)) /*__attribute__ ((warn_unused_result))*/;
	
	//! Re-use socket port
	/**
	 * Sets SO_REUSEPORT for the socket. Allows multiple apps to bind to same port and address.
	 * Primarily used for multi-casting.
	 *
	 * @param[in] x enable/disable port re-use
	 *
	 * @note In Win32, this causes behaviour similar to reuse_addr() does in all other systems.
	 */
	bool reuse_port(bool x) __attribute__ ((nothrow));
	
	//! Re-use socket address
	/**
	 * Sets SO_REUSEADDR for the socket. Allows socket port to be re-used as soon as it's
	 * freed, avoiding the delay caused from waiting TIME_WAIT to expire.
	 *
	 * @param[in] x enable/disable address re-use
	 *
	 * @note In Win32, this does nothing as TIME_WAIT is ignored completely there.
	 */
	bool reuse_addr(bool x) __attribute__ ((nothrow));
	
	//! Receive OOB data like any other
	bool inline_oob(bool x) __attribute__ ((nothrow));
	
	//! Set/unset lingering
	/**
	 * Sets SO_LINGER for the socket.
	 *
	 * "Lingers on close if unsent data is present."
	 *
	 * @param[in] x enable/disable lingering
	 * @param[in] delay linger time if enabled
	 */
	bool linger(bool x, ushort delay) __attribute__ ((nothrow));
	
	//! Bind socket to port and address
	/**
	 * @param[in] address address string to bind to
	 * @param[in] port port number to bind to
	 *
	 * @note This constructs Address from the string and port and passes it to bindTo(const Address&)
	 *
	 * @retval 0 on success
	 * @retval Error on error
	 */
	int bindTo(const std::string& address, ushort port) __attribute__ ((nothrow,warn_unused_result));
	
	//! Bind socket to port and address
	/**
	 * @param[in] address Address to bind to
	 *
	 * @retval 0 on success
	 * @retval Error on error
	 */
	int bindTo(const Address& address) __attribute__ ((nothrow,warn_unused_result));
	
	//! Connect to address
	/**
	 * @param[in] rhost host to connect to
	 * @retval 0 on success
	 * @retval Error on error
	 *
	 * @note getError() (... ?)
	 */
	int connect(const Address& rhost) __attribute__ ((nothrow));
	
	//! Set listening
	/**
	 * @retval 0 on success
	 * @retval Error on error
	 */
	int listen() __attribute__ ((nothrow));
	
	//! Send data.
	/**
	 * @param[in] buffer Data to be send.
	 * @param[in] buflen Number of bytes to send from buffer
	 *
	 * @return number of bytes actually sent.
	 * @retval Error on error
	 */
	int write(char* buf, size_t len) __attribute__ ((nothrow,warn_unused_result,nonnull(1)));
	
	//! Receive data
	/**
	 * @param[out] buf Target buffer
	 * @param[in] len Number of bytes to read from network.
	 *
	 * @return number of bytes read.
	 * @retval 0 if connection was closed on the other end.
	 * @retval Error on error.
	 */
	int read(char* buf, size_t len) __attribute__ ((nothrow,warn_unused_result,nonnull(1)));
	
	//! Shutdown socket
	/**
	 * @param[in] how SHUT_RD, SHUT_WR, SHUT_RDWR
	 */
	int shutdown(int how) __attribute__ ((nothrow));
	
	//! Get address structure
	/**
	 * @return Associated address structure.
	 */
	Address& getAddr() __attribute__ ((nothrow,warn_unused_result));
	const Address& getConstAddr() const __attribute__ ((nothrow,warn_unused_result));
	
	//! Get IP address
	/**
	 * @return IP address string.
	 * (e.g. [::1]:30000 or 127.0.0.1:30000)
	 */
	std::string address() const __attribute__ ((nothrow));
	
	//! Get port
	/**
	 * @return Local port number
	 */
	ushort port() const __attribute__ ((nothrow,warn_unused_result));
	
private:
	//! Set blocking
	/**
	 * @param[in] x enable/disable blocking
	 *
	 * @note You can't re-enable blocking on non-Win32 systems (API limitation?).
	 */
	bool block(bool x) __attribute__ ((nothrow));
};

#endif // Sockets_INCLUDED
