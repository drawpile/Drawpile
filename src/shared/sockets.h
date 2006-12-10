/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

#include <stdint.h>

#ifdef WIN32
	#include <winsock2.h>
	#define MSG_NOSIGNAL 0 // the flag isn't used in win32
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h> // close()
	#include <errno.h> // errno
	
	// not defined in non-win32 systems
	#define INVALID_SOCKET 0
	#define SOCKET_ERROR -1
#endif

//! Socket abstraction
class Socket
{
protected:
	//! Assigned file descriptor
	uint32_t sock;
	
	//! Remote address
	sockaddr_in addr;
	
	//! Last error number
	int error;
public:
	//! ctor
	Socket() throw()
		: sock(INVALID_SOCKET),
		error(0)
	{
	}
	
	//! dtor
	virtual ~Socket() throw()
	{
		close();
	}
	
	//! Close socket
	/**
	 * Closes the socket, and therefore, closes the connection associated with it.
	 */
	void close() throw();
	
	//! Set file descriptor
	/**
	 * @param nsock is the new file descriptor.
	 *
	 * @return file descriptor associated with the class.
	 */
	uint32_t fd(uint32_t nsock) throw()
	{
		if (sock != INVALID_SOCKET) close();
		return sock = nsock;
	}
	
	//! Get file descriptor
	/**
	 * @return file descriptor associated with the class.
	 */
	uint32_t fd() const throw() { return sock; }
	
	//! Accept new connection.
	/**
	 * @return NULL if no new connection was made.
	 * @return Socket* if new connection was accepted.
	 */
	Socket* accept();
	
	//! Set blocking
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise.
	 */
	int block(bool x) throw();
	
	//! Bind socket to port and address
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise
	 */
	int bindTo(uint32_t address, uint16_t port) throw();
	
	//! Set listening
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise.
	 */
	int listen() throw();
	
	//! Send data.
	/**
	 * @param buffer contains data to be send.
	 * @param buflen declares the number of bytes to be sent from buffer.
	 *
	 * @return number of bytes actually sent.
	 * @return SOCKET_ERROR otherwise.
	 */
	int send(char* buffer, size_t buflen) throw();
	
	//! Receive data
	/**
	 * @param buffer will be written to with max buflen bytes from network.
	 * @param buflen declares the number of bytes to read from network.
	 *
	 * @return number of bytes read.
	 * @return 0 if connection was closed on the other end.
	 * @return SOCKET_ERROR otherwise.
	 */
	int recv(char* buffer, size_t buflen) throw();
	
	//! Get last error number
	/**
	 * @return last errno.
	 */
	int getError() const throw() { return error; }
	
	//! Get address structure
	/**
	 * @return associated address structure.
	 */
	sockaddr_in* getAddr() throw() { return &addr; }
	
	//! Get remote IP address
	/**
	 * @return IP address.
	 */
	int address() const throw() { return ntohl(addr.sin_addr.s_addr); }
	
	//! Get remote port
	/**
	 * @return port number.
	 */
	int port() const throw() { return ntohs(addr.sin_port); }
};

#endif // Sockets_INCLUDED
