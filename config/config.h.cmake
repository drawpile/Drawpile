#pragma once

// unicode
//#define UNICODE 1
//#define _UNICODE 1
//#undef _MBCS

#ifdef WIN32
	//#define NTDDI_VERSION NTDDI_WIN2KSP4
	//#define _WIN32_WINNT 0x0501
	#define _WIN32_IE 0x0700 // MSIE 7.0
#else
	#define _XOPEN_SOURCE 600
#endif

#cmakedefine USE_ASM 1

#cmakedefine HAVE_QT43 1

#cmakedefine DRAWPILE_VERSION "${DRAWPILE_VERSION}"

#ifdef _MSC_VER
	#define NOTHROW
	#define NONNULL(x)
	#define ATTRPURE
#else
	#define NOTHROW __attribute__ (( __nothrow__ ))
	#define NONNULL(x) __attribute__ (( __nonnull__(x) ))
	#define ATTRPURE __attribute__ (( __pure__ ))
#endif

#if defined(WIN32)
	#define WIN32_LEAN_AND_MEAN
	#define NOGDI
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
#endif

// visibility
#ifdef _MSC_VER
	#ifdef BUILDING_DLL
		#define EXPORT __declspec(dllexport)
	#else
		#define EXPORT __declspec(dllimport)
	#endif
	#define LOCAL
#else
	#ifdef HAVE_GCCVISIBILITYPATCH
		#define EXPORT __attribute__ ((visibility("default")))
		#define LOCAL __attribute__ ((visibility("hidden")))
	#else
		#define EXPORT
		#define LOCAL
	#endif
#endif

