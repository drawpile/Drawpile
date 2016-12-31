#
#  Copyright (c) 2010 Carlos Junior <address@hidden>
#
# - Try to find libmicrohttpd
# Once done this will define
#
#  MHD_FOUND - system has libmicrohttpd
#  MHD_INCLUDE_DIRS - the libmicrohttpd include directory
#  MHD_LIBRARIES - Link these to use libmicrohttpd
#  MHD_DEFINITIONS - Compiler switches required for using libmicrohttpd
#
#  Redistribution and use is allowed according to the terms of the GPLv3 license
#  For details see the accompanying LICENSE file.
#

if (MHD_LIBRARIES AND MHD_INCLUDE_DIRS)
  # in cache already
  set(MHD_FOUND TRUE)
else (MHD_LIBRARIES AND MHD_INCLUDE_DIRS)
  set(MHD_DEFINITIONS "")

  find_path(MHD_INCLUDE_DIR
    NAMES
      microhttpd.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(MHD_LIBRARY
    NAMES
      microhttpd
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (MHD_LIBRARY)
    set(MHD_FOUND TRUE)
  endif (MHD_LIBRARY)

  set(MHD_INCLUDE_DIRS
    ${MHD_INCLUDE_DIR}
  )

  if (MHD_FOUND)
    set(MHD_LIBRARIES
      ${MHD_LIBRARIES}
      ${MHD_LIBRARY}
    )
  endif (MHD_FOUND)

  if (MHD_INCLUDE_DIRS AND MHD_LIBRARIES)
    set(MHD_FOUND TRUE)
  endif (MHD_INCLUDE_DIRS AND MHD_LIBRARIES)

  if (MHD_FIND_REQUIRED AND NOT MHD_FOUND)
    message(FATAL_ERROR "Could not find LibMicroHTTPD")
  endif (MHD_FIND_REQUIRED AND NOT MHD_FOUND)

  # show the MHD_INCLUDE_DIRS and MHD_LIBRARIES variables only in the advanced view
  mark_as_advanced(MHD_INCLUDE_DIRS MHD_LIBRARIES)

endif (MHD_LIBRARIES AND MHD_INCLUDE_DIRS)
