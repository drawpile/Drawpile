# from: https://github.com/kostko/libcurvecpr-asio/blob/master/cmake/FindSodium.cmake
# aa358c98dec8fd0f31e7bab7a585571d5f8bfae3
#
# - Find Sodium
# Find the native libsodium includes and library.
#
#  HINT: SODIUM_ROOT_DIR
#
# Once done this will define
#
#  SODIUM_INCLUDE_DIR    - where to find libsodium header files, etc.
#  SODIUM_LIBRARY        - List of libraries when using libsodium.
#  SODIUM_FOUND          - True if libsodium found.
#

# (from FindBoost script, edited by Cameron)
# Collect environment variable inputs and use them if the local variable is not set.
foreach(v SODIUM_ROOT_DIR)
  if(NOT ${v})
	set(_env $ENV{${v}})
	if(_env)
		file(TO_CMAKE_PATH "${_env}" _ENV_${v})
    else()
		set(_ENV_${v} "")
    endif()
	set(${v} ${_ENV_${v}})
  endif()
endforeach()

if(CMAKE_CL_64)
	set(SODIUM_BUILDTYPE_DIR "x64")
else()
	set(SODIUM_BUILDTYPE_DIR "Win32")
endif()

if (sodium_USE_STATIC_LIBS)
	set(SODIUM_NAMES sodium.a libsodium.a )
else()
	set(SODIUM_NAMES sodium libsodium )
endif()

FIND_LIBRARY(SODIUM_LIBRARY NAMES ${SODIUM_NAMES}
	HINTS
	${SODIUM_ROOT_DIR}/lib
	${SODIUM_ROOT_DIR}/${SODIUM_BUILDTYPE_DIR}/Debug/v141/dynamic
	${SODIUM_ROOT_DIR}/${SODIUM_BUILDTYPE_DIR}/Win32/Debug/v141/dynamic
	${CMAKE_FIND_ROOT_PATH}/sys-root/mingw/lib
)

find_path(SODIUM_INCLUDE_DIR NAMES sodium.h
	HINTS
	${SODIUM_ROOT_DIR}/include
	${SODIUM_ROOT_DIR}/src/libsodium/include
	${CMAKE_FIND_ROOT_PATH}/sys-root/mingw/include
)

# handle the QUIETLY and REQUIRED arguments and set SODIUM_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Sodium REQUIRED_VARS SODIUM_LIBRARY SODIUM_INCLUDE_DIR)

MARK_AS_ADVANCED(SODIUM_LIBRARY SODIUM_INCLUDE_DIR)

