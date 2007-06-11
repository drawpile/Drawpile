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

#include "sockets.h"

#include <iostream>
#include <string>
#include <cassert>

#ifndef WIN32
	#include <fcntl.h>
	#ifdef HAVE_SNPRINTF
		#include <cstdio>
	#endif
#endif

using std::cout;
using std::endl;
using std::cerr;

#ifdef NEED_NET
Net::Net() throw(std::exception)
{
	#ifndef NDEBUG
	cout << "[Network] Starting" << endl;
	#endif
	
	#if defined(WIN32)
	const int maj=2, min=2;
	
	WSADATA info;
	if (WSAStartup(MAKEWORD(maj,min), &info))
		throw std::exception();
	
	if (LOBYTE(info.wVersion) != maj
		or HIBYTE(info.wVersion) != min)
	{
		cerr << "ERROR: Invalid WSA version: "
			<< static_cast<int>(LOBYTE(info.wVersion))
			<< "." << static_cast<int>(HIBYTE(info.wVersion)) << endl;
		WSACleanup( );
		exit(1); 
	}
	#endif
}

Net::~Net() throw()
{
	#ifndef NDEBUG
	cout << "[Network] Stopping" << endl;
	#endif
	
	#if defined(WIN32)
	WSACleanup();
	#endif
}
#endif // NEED_NET

/* *** Address Templates *** */

/*
template <> in_addr* getAddress<sockaddr_in,in_addr>(sockaddr_in &addr) throw() { return &addr.sin_addr; }
template <> in6_addr* getAddress<sockaddr_in6,in6_addr>(sockaddr_in6 &addr) throw() { return &addr.sin6_addr; }

template <> const in_addr* getAddress<sockaddr_in,in_addr>(const sockaddr_in &addr) throw() { return &addr.sin_addr; }
template <> const in6_addr* getAddress<sockaddr_in6,in6_addr>(const sockaddr_in6 &addr) throw() { return &addr.sin6_addr; }
*/

template <> ushort& getPort<sockaddr_in>(sockaddr_in &addr) throw() { return addr.sin_port; }
template <> ushort& getPort<sockaddr_in6>(sockaddr_in6 &addr) throw() { return addr.sin6_port; }

template <> ushort getPort<sockaddr_in>(const sockaddr_in &addr) throw() { return addr.sin_port; }
template <> ushort getPort<sockaddr_in6>(const sockaddr_in6 &addr) throw() { return addr.sin6_port; }

template <> void setFamily<sockaddr_in>(sockaddr_in &addr) throw() { addr.sin_family = AF_INET; }
template <> void setFamily<sockaddr_in6>(sockaddr_in6 &addr) throw() { addr.sin6_family = AF_INET6; }

/* *** Socket class *** */

fd_t Socket::create() throw()
{
	setFamily(addr);
	#ifdef WIN32
	sock = WSASocket(addr.sin_family, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
	#else // POSIX
	sock = socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
	#endif
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EPROTONOSUPPORT);
		assert(s_error != EPROTOTYPE);
		assert(s_error != ESOCKTNOSUPPORT);
		assert(s_error != EINVAL);
		
		switch (s_error)
		{
		#ifdef WIN32
		case EINPROGRESS:
			break;
		case ENETDOWN:
			cerr << "[Socket] Network sub-system failure" << endl;
			break;
		#else // POSIX
		// TODO
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
	}
	
	return sock;
}

void Socket::close() throw()
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

Socket Socket::accept() throw()
{
	assert(sock != INVALID_SOCKET);
	
	// temporary address struct
	r_sockaddr sa;
	
	socklen_t addrlen = sizeof(sa);
	
	#ifdef WIN32
	fd_t n_fd = ::WSAAccept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen, 0, 0);
	#else // POSIX
	fd_t n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen);
	#endif
	
	if (n_fd != INVALID_SOCKET)
	{
		return Socket(n_fd, sa);
	}
	else
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
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
		
		switch (s_error)
		{
		case EINTR: // interrupted
		case EAGAIN: // would block
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
		
		return Socket();
	}
}

bool Socket::block(const bool x) throw()
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

bool Socket::reuse_port(const bool x) throw()
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
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		cerr << "[Socket] Unknown error in reuse_port() - " << s_error << endl;
		exit(1);
	}
	
	return (r == 0);
	#endif
}

bool Socket::reuse_addr(const bool x) throw()
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
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		cerr << "[Socket] Unknown error in reuse_addr() - " << s_error << endl;
		exit(1);
	}
	
	return (r == 0);
	#endif
}

bool Socket::linger(const bool x, const ushort delay) throw()
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
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		cerr << "[Socket] Unknown error in linger() - " << s_error << endl;
		exit(1);
	}
	else
		return (r == 0);
}

int Socket::bindTo(const std::string& address, const ushort _port) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Binding to address " << address << ":" << _port << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	addr = Socket::StringToAddr(address);
	
	setFamily(addr);
	
	ushort &port = getPort(addr);
	bswap(port = _port);
	
	const int r = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EISCONN);
		
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
			cerr << "[Socket] Can't bind to super-user sockets" << endl;
			break;
		}
	}
	
	return r;
}

#ifdef IPV6_SUPPORT
int Socket::connect(const sockaddr_in6& rhost) throw()
{
	// TODO
	assert(1);
	return 0;
}
#endif

int Socket::connect(const sockaddr_in& rhost) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Connecting to " << AddrToString(rhost) << endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	memcpy(&r_addr, &rhost, sizeof(r_addr));
	
	#ifdef WIN32
	const int r = WSAConnect(sock, reinterpret_cast<sockaddr*>(&r_addr), sizeof(r_addr), 0, 0, 0, 0);
	#else // POSIX
	const int r = ::connect(sock, reinterpret_cast<sockaddr*>(&r_addr), sizeof(r_addr));
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
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
			cerr << "[Socket] Firewall denied connection" << endl;
			break;
		#endif
		case ECONNREFUSED:
		case ETIMEDOUT:
		case ENETUNREACH:
			break;
		case EAGAIN:
			// retry
			return 2;
		}
	}
	
	return r;
}

int Socket::listen() throw()
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
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in listen() - " << s_error << endl;
		#endif // NDEBUG
		exit(1);
	}
	else
		return r;
}

int Socket::send(char* buffer, const size_t len) throw()
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
	DWORD sb;
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
		case EAGAIN:
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

int Socket::recv(char* buffer, const size_t len) throw()
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
	DWORD flags=0;
	DWORD rb;
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
		case EAGAIN:
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
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes) throw()
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
		
		// programming errors
		assert(s_error != ENOTSOCK);
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTCONN);
		
		switch (s_error)
		{
		case EAGAIN:
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

std::string Socket::address() const throw()
{
	return AddrToString(addr);
}

ushort Socket::port() const throw()
{
	ushort port = getPort(addr);
	return bswap(port);
}

bool Socket::matchAddress(const Socket& tsock) const throw()
{
	#ifdef IPV6_SUPPORT
	return (addr.sin6_addr == tsock.addr.sin6_addr); // FIXME
	#else // IPv4
	return (addr.sin_addr.s_addr == tsock.addr.sin_addr.s_addr);
	#endif
}

bool Socket::matchPort(const Socket& tsock) const throw()
{
	return (port() == tsock.port());
}

/* string functions */

std::string Socket::AddrToString(const r_sockaddr& l_addr) throw()
{
	#ifdef IPV6_SUPPORT
	const uint length = Network::IPv6::AddrLength + Network::PortLength + 4;
	const char format_string[] = "[%s]:%d";
	#else
	const uint length = Network::IPv4::AddrLength + Network::PortLength + 2;
	const char format_string[] = "%s:%d";
	#endif
	
	char buf[length];
	
	#ifdef WIN32
	DWORD len = length;
	sockaddr sa;
	WSAAddressToString(&sa, sizeof(l_addr), 0, buf, &len);
	#else // POSIX
	char straddr[length];
	//inet_ntop(raddr.sin_family, getAddress(l_addr), straddr, length);
	#ifdef IPV6_SUPPRT
	inet_ntop(l_addr.sin_family, &l_addr.sin6_addr, straddr, length);
	#else
	inet_ntop(l_addr.sin_family, &l_addr.sin_addr, straddr, length);
	#endif
	ushort port = getPort(l_addr);
	bswap(port);
	
	#ifdef HAVE_SNPRINTF
	snprintf(buf, length, format_string, straddr, port);
	#else
	sprintf(buf, format_string, straddr, port);
	#endif // HAVE_SNPRINTF
	#endif
	return std::string(buf);
}

r_sockaddr Socket::StringToAddr(const std::string& address) throw()
{
	r_sockaddr naddr;
	setFamily(naddr);
	
	#ifdef WIN32
	// Win32 doesn't have inet_pton
	char buf[Network::IPv6::AddrLength + Network::PortLength + 4];
	memcpy(buf, address.c_str(), address.length());
	int size = sizeof(naddr);
	WSAStringToAddress(buf, naddr.sin_family, 0, reinterpret_cast<sockaddr*>(&naddr), &size);
	#else // POSIX
	//inet_pton(naddr.sin_family, address.c_str(), getAddress(naddr));
	#ifdef IPV6_SUPPORT
	inet_pton(naddr.sin_family, address.c_str(), &naddr.sin6_addr);
	#else
	inet_pton(naddr.sin_family, address.c_str(), &naddr.sin_addr);
	#endif
	#endif
	
	return naddr;
}
