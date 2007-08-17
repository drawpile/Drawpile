#ifndef SystemErrors_INCLUDED
#define SystemErrors_INCLUDED

#include <cstddef>

static const int OutOfMemory = ENOMEM;
static const int Fault = EFAULT;
static const int BadDescriptor = EBADF;
static const int SystemDescriptorLimit = ENFILE;
static const int InsufficientPermissions = EPERM;

#ifdef WIN32
static const int Interrupted = EINTR;
static const int WouldBlock = WSAEWOULDBLOCK;
static const int DescriptorLimit = WSAEMFILE;
#else
static const int DescriptorLimit = EMFILE;
static const int Interrupted = WSAEINTR;
	#ifndef EWOULDBLOCK
static const int WouldBlock = EWOULDBLOCK;
	#else // some systems don't define EWOULDBLOCK
static const int WouldBlock = EAGAIN;
	#endif
#endif

#endif // SystemErrors_INCLUDED
