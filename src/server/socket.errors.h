#ifndef SocketErrors_INCLUDED
#define SocketErrors_INCLUDED

#include "socket.types.h"
#include "socket.porting.h"

//! Socket errors
namespace socket_error
{

#ifdef WIN32
#ifndef NDEBUG
const int FamilyNotSupported = WSAEAFNOSUPPORT;
const int NotSocket = WSAENOTSOCK;
const int ProtocolOption = WSAENOPROTOOPT;
const int ProtocolType = WSAEPROTOTYPE;
//const int _NotSupported = WSAE??;
const int OperationNotSupported = WSAEOPNOTSUPP;
const int ProtocolNotSupported = WSAEPROTONOSUPPORT;
#endif
const fd_t InvalidHandle = INVALID_SOCKET; // 0 ?
const int NoSignal = 0;
const int InProgress = WSAEINPROGRESS;
const int SubsystemDown = WSAENETDOWN;
const int OutOfBuffers = WSAENOBUFS;
const int ConnectionRefused = WSAECONNREFUSED;
const int ConnectionAborted = WSAECONNABORTED;
const int ConnectionTimedOut = WSAETIMEDOUT;
const int ConnectionReset = WSAECONNRESET;
const int Unreachable = WSAENETUNREACH;
const int NotConnected = WSAENOTCONN;
const int Connected = WSAEISCONN;
const int Already = WSAEALREADY; // ?
const int Shutdown = WSAESHUTDOWN;
const int Disconnected = WSAEDISCON;
const int NetworkReset = WSAENETRESET;
//! Generic error return value
const int Error = SOCKET_ERROR;
const int AddressInUse = WSAEADDRINUSE;
const int AddressNotAvailable = WSAEADDRNOTAVAIL;
#else // POSIX
#ifndef NDEBUG
const int FamilyNotSupported = EAFNOSUPPORT;
const int NotSocket = ENOTSOCK;
//const int _NotSupported = E??;
const int OperationNotSupported = EOPNOTSUPP;
const int ProtocolNotSupported = EPROTONOSUPPORT;
const int ProtocolOption = ENOPROTOOPT;
const int ProtocolType = EPROTOTYPE;
#endif
const fd_t InvalidHandle = -1;
const int NoSignal = MSG_NOSIGNAL;
const int NotConnected = ENOTCONN;
const int InProgress = EINPROGRESS;
const int SubsystemDown = ENETDOWN;
const int OutOfBuffers = ENOBUFS;
const int ConnectionRefused = ECONNREFUSED;
const int ConnectionAborted = ECONNABORTED;
const int ConnectionTimedOut = ETIMEDOUT;
const int ConnectionReset = ECONNRESET;
const int Unreachable = ENETUNREACH;
const int Connected = EISCONN;
const int Already = EALREADY; // ?
const int Shutdown = ESHUTDOWN;
//const int Disconnected = EDISCON; // not present on non-windows systems?
const int NetworkReset = ENETRESET;
//! Generic error return value
const int Error = -1;
const int AddressInUse = EADDRINUSE;
const int AddressNotAvailable = EADDRNOTAVAIL;
#endif

const int ConnectionBroken = EPIPE;

} // namespace:socket_error

#endif // SocketErrors_INCLUDED
