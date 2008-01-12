#pragma once

#cmakedefine IS_BIG_ENDIAN 1

#cmakedefine HAVE_BOOST 1
#cmakedefine HAVE_SNPRINTF 1

#cmakedefine USE_ASM 1

#cmakedefine HAVE_QT43 1

#define NOTHROW __attribute__ (( __nothrow__ ))
#define NONNULL(x) __attribute__ (( __nonnull__(x) ))
#define ATTRPURE __attribute__ (( __pure__ ))

#if defined(WIN32)
	#define WIN32_LEAN_AND_MEAN
	#define NOGDI
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
#endif
