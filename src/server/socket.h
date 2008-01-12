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

#pragma once

#ifndef Sockets_INCLUDED
#define Sockets_INCLUDED

#include "config.h"

#include "ref_counted.h"

#include "address.h"
#include "socket.types.h" // fd_t

#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif

#ifndef NDEBUG
namespace debug {
#ifdef WIN32
const int FamilyNotSupported = WSAEAFNOSUPPORT;
const int NotSocket = WSAENOTSOCK;
const int ProtocolOption = WSAENOPROTOOPT;
const int ProtocolType = WSAEPROTOTYPE;
const int OperationNotSupported = WSAEOPNOTSUPP;
const int ProtocolNotSupported = WSAEPROTONOSUPPORT;
const int NotConnected = WSAENOTCONN;
const int Connected = WSAEISCONN;
#else
const int FamilyNotSupported = EAFNOSUPPORT;
const int NotSocket = ENOTSOCK;
const int OperationNotSupported = EOPNOTSUPP;
const int ProtocolNotSupported = EPROTONOSUPPORT;
const int ProtocolOption = ENOPROTOOPT;
const int ProtocolType = EPROTOTYPE;
const int NotConnected = ENOTCONN;
const int Connected = EISCONN;
#endif
}
#endif

#ifdef WIN32
const int NetSubsystemDown = WSAENETDOWN;
const int OutOfBuffers = WSAENOBUFS;
const int ConnectionTimedOut = WSAETIMEDOUT;
#else // POSIX
const int NetSubsystemDown = ENETDOWN;
const int OutOfBuffers = ENOBUFS;
const int ConnectionTimedOut = ETIMEDOUT;
#endif

//! Socket abstraction
class Socket
	: public ReferenceCounted
{
protected:
	fd_t m_handle;
	int m_error;
public:
	#ifdef WIN32
	static const int Error = SOCKET_ERROR;
	static const fd_t InvalidHandle = INVALID_SOCKET; // 0 ?
	#else
	static const int Error = -1;
	static const fd_t InvalidHandle = -1;
	#endif
	
	//! Possible values to shutdown()
	enum ShutdownStyle {
		#ifdef WIN32
		FullShutdown=SD_BOTH,
		ShutdownWriting=SD_SEND,
		ShutdownReading=SD_RECEIVE
		#else
		FullShutdown=SHUT_RDWR,
		ShutdownWriting=SHUT_WR,
		ShutdownReading=SHUT_RD
		#endif
	};
protected:
	//! Address (local for listening, remote for outgoing)
	Address m_addr;
	
	//! For cheating
	bool m_connected;
public:
	//! Default constructor
	/**
	 * @param[in] nsock FD to associate with this Socket
	 * @param[in] saddr Address to associate with this Socket
	 */
	Socket(fd_t nsock=InvalidHandle, const Address& saddr=Address(Network::UnspecifiedAddress,0)) NOTHROW;
	
	//! Copy ctor
	Socket(const Socket& socket) NOTHROW;
	
	~Socket() NOTHROW;
	
	//! Accept new connection.
	/**
	 * @return Socket if new connection was accepted
	 * @note (Socket.getFD() == InvalidHandle) if no new connection was accepted
	 */
	Socket accept() NOTHROW;
	
	//! Get handle
	fd_t handle() const NOTHROW;
	//! Set handle
	void setHandle(fd_t handle) NOTHROW;
	//! Close handle
	void close() NOTHROW;
	
	//! Bind socket to port and address
	/**
	 * @param[in] address Address to bind to
	 *
	 * @retval true on success
	 * @retval false on error
	 */
	bool bindTo(const Address& address) NOTHROW;
	
	//! Set listening
	/**
	 * @retval true on success
	 * @retval false on error
	 */
	bool listen() NOTHROW;
	
	bool isValid() const NOTHROW;
	
	//! Return last error number
	int getError() NOTHROW;
	
	//! Send data.
	/**
	 * @param[in] buf Data to be send.
	 * @param[in] len Number of bytes to send from buffer
	 *
	 * @return number of bytes actually sent.
	 * @retval Error on error
	 */
	int write(char* buf, size_t len) NOTHROW NONNULL(1);
	
	//! Receive data
	/**
	 * @param[out] buf Target buffer
	 * @param[in] len Number of bytes to read from network.
	 *
	 * @return number of bytes read.
	 * @retval 0 if connection was closed on the other end.
	 * @retval Error on error.
	 */
	int read(char* buf, size_t len) NOTHROW NONNULL(1);
	
	//! Shutdown socket
	/**
	 * @param[in] how FullShutdown
	 */
	bool shutdown(ShutdownStyle how) NOTHROW;
	
	//! Get address structure
	Address& addr() NOTHROW;
	
	//! Get const address structure
	const Address& addr() const NOTHROW;
	
	//! Create new socket
	fd_t create() NOTHROW;
	
	//! Return true if socket is connected
	bool isConnected() const NOTHROW;
	
protected:
	//! Re-use socket address
	/**
	 * Sets SO_REUSEADDR for the socket. Allows socket port to be re-used as soon as it's
	 * freed, avoiding the delay caused from waiting TIME_WAIT to expire.
	 *
	 * @param[in] x enable/disable address re-use
	 *
	 * @note Called automatically in constructor on non-Windows systems.
	 */
	bool reuse_addr(bool x) NOTHROW;
	
	//! Set blocking
	/**
	 * @param[in] x enable/disable blocking
	 *
	 * @note You can't re-enable blocking on non-Win32 systems (API limitation?).
	 */
	bool block(bool x) NOTHROW;
	
	//! Set up socket options
	void setup() NOTHROW;
};

#endif // Sockets_INCLUDED
