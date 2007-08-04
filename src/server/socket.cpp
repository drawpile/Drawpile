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

#include "socket.h"

#include "socket.internals.h"
#include "socket.porting.h"
#include "../shared/templates.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <string> // std::string
#include <cassert>

#ifndef WIN32 // POSIX
	#include <fcntl.h>
	#ifdef HAVE_SNPRINTF
		#include <cstdio>
	#endif
	#include <cerrno>
#endif

Socket::Socket(const fd_t& nsock)
	: sock(nsock)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::Socket()" << std::endl;
	#endif
}

Socket::Socket(const fd_t& nsock, const Address& saddr)
	: sock(nsock),
	addr(saddr)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket(FD: " << nsock << ", address: " << saddr.toString() << ") constructed" << std::endl;
	#endif
}

Socket::~Socket()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "~Socket(FD: " << sock << ") destructed" << std::endl;
	#endif
	
	close();
}

fd_t Socket::create()
{
	if (sock != INVALID_SOCKET)
		close();
	
	#ifdef WIN32
	sock = WSASocket(addr.family, SOCK_STREAM, 0, 0, 0, 0 /* WSA_FLAG_OVERLAPPED */);
	#else // POSIX
	sock = socket(addr.family, SOCK_STREAM, IPPROTO_TCP);
	#endif
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EPROTONOSUPPORT);
		assert(s_error != EPROTOTYPE);
		assert(s_error != ESOCKTNOSUPPORT);
		assert(s_error != EINVAL);
		
		#ifndef NDEBUG
		switch (s_error)
		{
		#ifdef WIN32
		case EINPROGRESS:
			break;
		case ENETDOWN:
			cerr << "[Socket] Network sub-system failure" << endl;
			break;
		#endif
		case EMFILE:
			cerr << "[Socket] Socket limit reached" << endl;
			break;
		case ENOBUFS:
			cerr << "[Socket] out of buffers" << endl;
			break;
		default:
			cerr << "[Socket] Unknown error in create() - " << s_error << endl;
			assert(s_error);
			break;
		}
		#endif
	}
	
	return sock;
}

fd_t Socket::fd() const
{
	return sock;
}

fd_t Socket::fd(fd_t nsock)
{
	if (sock != INVALID_SOCKET) close();
	return sock = nsock;
}

fd_t Socket::release()
{
	fd_t t_sock = sock;
	sock = INVALID_SOCKET;
	return t_sock;
}

void Socket::close()
{
	#if defined(HAVE_XPWSA)
	::DisconnectEx(sock, 0, TF_REUSE_SOCKET, 0);
	#elif defined(WIN32)
	::closesocket(sock);
	#else // POSIX
	::close(sock);
	#endif
	
	sock = INVALID_SOCKET;
}

Socket Socket::accept()
{
	assert(sock != INVALID_SOCKET);
	
	// temporary address struct
	Address sa;
	
	socklen_t addrlen = sa.size();
	
	#ifdef WIN32
	fd_t n_fd = ::WSAAccept(sock, &sa.addr, &addrlen, 0, 0);
	#else // POSIX
	fd_t n_fd = ::accept(sock, &sa.addr, &addrlen);
	#endif
	
	if (n_fd == INVALID_SOCKET)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifdef EPROTO
		assert(s_error != EPROTO);
		#endif
		
		#ifdef LINUX
		// no idea what these are
		assert(s_error != ENOSR); // ?
		assert(s_error != ESOCKTNOSUPPORT); // ?
		assert(s_error != EPROTONOSUPPORT); // Protocol not supported
		assert(s_error != ETIMEDOUT); // Timed out
		assert(s_error != ERESTARTSYS); // ?
		#endif
		
		#ifndef NDEBUG
		switch (s_error)
		{
		case EINTR: // interrupted
		case EWOULDBLOCK: // would block
			break;
		case EMFILE:
			cerr << "[Socket] Process FD limit reached" << endl;
			break;
		case ENOBUFS:
			cerr << "[Socket] Out of network buffers" << endl;
			break;
		case ECONNABORTED:
			cerr << "[Socket] Incoming connection aborted" << endl;
			break;
		case ENOMEM:
			cerr << "[Socket] Out of memory" << endl;
			break;
		case EPERM:
			cerr << "[Socket] Firewall blocked incoming connection" << endl;
			break;
		#ifndef WIN32 // POSIX
		case ENFILE:
			cerr << "[Socket] System FD limit reached" << endl;
			break;
		#endif
		default:
			cerr << "[Socket] Unknown error in accept() - " << s_error << endl;
			assert(s_error);
			break;
		}
		#endif
	}
	
	return Socket(n_fd, sa);
}

bool Socket::block(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Blocking for socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif // NDEBUG
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	uint32_t arg = (x ? 1 : 0);
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else // POSIX
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif
}

bool Socket::reuse_port(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Reuse port of socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifndef SO_REUSEPORT
	// Windows (for example) does not have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char*)&val, sizeof(int));
	
	if (r == SOCKET_ERROR)
	{
		s_error = errno;
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in reuse_port() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
	#endif
}

bool Socket::reuse_addr(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Reuse address of socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifndef SO_REUSEADDR
	// If the system doesn't have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(int));
	
	if (r == SOCKET_ERROR)
	{
		s_error = errno;
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in reuse_addr() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
	#endif
}

bool Socket::linger(const bool x, const ushort delay)
{
	#ifndef NDEBUG
	cout << "[Socket] Linger for socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	::linger lval;
	lval.l_onoff = (x ? 1 : 0);
	lval.l_linger = delay;
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lval), sizeof(lval));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in linger() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
}

int Socket::bindTo(const std::string& address, const ushort _port)
{
	#if !defined(NDEBUG)
	cout << "[Socket] Binding to address " << address << ":" << _port << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	Address naddr = Address::fromString(address);
	
	naddr.port(_port);
	
	return bindTo(naddr);
}

int Socket::bindTo(const Address& naddr)
{
	addr = naddr;
	
	#ifndef NDEBUG
	assert(addr.family != Network::Family::None);
	#endif
	
	const int r = bind(sock, &addr.addr, addr.size());
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EISCONN);
		
		#ifndef NDEBUG
		cerr << "[Socket] Failed to bind address and port." << endl;
		
		switch (s_error)
		{
		case EADDRINUSE:
			cerr << "[Socket] Address already in use" << endl;
			break;
		case EADDRNOTAVAIL:
			cerr << "[Socket] Address not available" << endl;
			break;
		case ENOBUFS:
			cerr << "[Socket] Out of network buffers" << endl;
			break;
		case EACCES:
			cerr << "[Socket] Can't bind to super-user ports" << endl;
			break;
		default:
			cerr << "[Socket] Unknown error in bindTo() [error: " << s_error << "]" << endl;
			break;
		}
		#endif
	}
	
	return r;
}

int Socket::connect(const Address& rhost)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Connecting to " << rhost.toString() << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	addr = rhost;
	
	#ifdef WIN32
	const int r = WSAConnect(sock, &addr.addr, addr.size(), 0, 0, 0, 0);
	#else // POSIX
	const int r = ::connect(sock, &addr.addr, addr.size());
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EISCONN);
		assert(s_error != EADDRINUSE);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EALREADY);
		
		switch (s_error)
		{
		case EINPROGRESS:
			break;
		case EACCES:
		#ifdef EPERM
		case EPERM:
			break;
		#endif
		case ECONNREFUSED:
		case ETIMEDOUT:
		case ENETUNREACH:
			break;
		default:
			break;
		}
	}
	
	return r;
}

int Socket::listen()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Listening" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	const int r = ::listen(sock, 4);
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifndef NDEBUG
		cerr << "[Socket] Failed to open listening port. [error: " << s_error << "]" << endl;
		#endif // NDEBUG
	}
	
	return r;
}

int Socket::send(char* buffer, const size_t len)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Sending " << len << " bytes" << endl;
	#endif
	
	assert(buffer != 0);
	assert(len > 0);
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	u_long sb;
	const int r = ::WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	#else // POSIX
	const int r = ::send(sock, buffer, len, MSG_NOSIGNAL);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != EBADF);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#else
		assert(s_error != EOPNOTSUPP);
		#endif
		
		switch (s_error)
		{
		case EWOULDBLOCK:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			break;
		case ENOMEM:
		case ENOBUFS:
			break;
		#ifdef WIN32
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "[Socket] Unknown error in send() - " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return sb;
	#else
	return r;
	#endif
}

/*
#ifdef HAVE_SENDMSG
int Socket::sc_send(std::list<Array<char*,size_t>* > buffers)
{
	//iterator iter = blah
	assert(buffers.size() != 0);
	
	msghdr *msg = new msghdr;
	msg->msg_iovlen = buffers.size();
	msg->msg_iov = new iovec[msg->msg_iovlen];
	for (int i=0; iter != buffers.end(); ++iter, ++i)
	{
		msg->msg_iov[i].iov_base = (*iter)->ptr;
		msg->msg_iov[i].iov_len = (*iter)->length;
	}
}
#endif

#ifdef HAVE_RECVMSG
int Socket::sc_recv(std::list<Array<char*,size_t> > buffers)
{
	
}
#endif
*/

int Socket::recv(char* buffer, const size_t len)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Receiving at most " << len << " bytes" << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	assert(buffer != 0);
	assert(len > 0);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	u_long flags=0;
	u_long rb;
	const int r = ::WSARecv(sock, &wbuf, 1, &rb, &flags, 0, 0);
	#else // POSIX
	const int r = ::recv(sock, buffer, len, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		
		switch (s_error)
		{
		case EWOULDBLOCK:
		case EINTR:
			break;
		case ENOBUFS: // Out of buffers
		case ECONNRESET:
		case ECONNREFUSED:
			break;
		case ENOMEM:
			break;
		#ifdef WIN32
		case WSAEDISCON:
		case WSAESHUTDOWN:
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSA_IO_PENDING: // Operation will be completed later
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "[Socket] Unknown error in recv() - " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return rb;
	#else
	return r;
	#endif
}

#if defined(WITH_SENDFILE) or defined(HAVE_XPWSA)
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Sending file" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	assert(offset >= 0);
	
	// call the real sendfile()
	#ifdef HAVE_XPWSA
	const int r = TransmitFile(sock, fd, nbytes, 0, 0, 0, TF_WRITE_BEHIND);
	#else // non-windows
	const int r = ::sendfile(fd, sock, offset, nbytes, 0, sbytes, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != ENOTSOCK);
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTCONN);
		
		switch (s_error)
		{
		case EWOULDBLOCK:
			// retry
			break;
		#ifndef WIN32 // POSIX
		case EPIPE:
		case EIO: // should be handled by the caller
			break;
		#endif
		default:
			cerr << "[Socket] Unknown error in sendfile() - " << s_error << endl;
			assert(s_error);
		}
	}
	
	return 0;
}
#endif // WITH_SENDFILE

std::string Socket::address() const
{
	return addr.toString();
}

ushort Socket::port() const
{
	return addr.port();
}

int Socket::shutdown(int how)
{
	return ::shutdown(sock, how);
}

int Socket::getError() const
{
	return s_error;
}

Address& Socket::getAddr()
{
	return addr;
}

#ifdef SOCKET_OPS
bool Socket::operator== (const Socket& tsock) const
{
	return (sock == tsock.sock);
}

Socket& Socket::operator= (Socket& tsock)
{
	if (sock != INVALID_SOCKET)
		close(sock);
	sock = tsock.sock;
	return *this;
}
#endif
