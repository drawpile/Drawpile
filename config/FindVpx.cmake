# Try to find libvpx
# 
# This will define
# LIBVPX_FOUND
# LIBVPX_INCLUDE_DIRS
# LIBVPX_LIBRARIES

find_path(LIBVPX_INCLUDE_DIR vpx_encoder.h
	PATH_SUFFIXES vpx)
find_library(LIBVPX_LIBRARY NAMES vpx)

set(LIBVPX_INCLUDE_DIRS ${LIBVPX_INCLUDE_DIR})
set(LIBVPX_LIBRARIES ${LIBVPX_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libvpx DEFAULT_MSG LIBVPX_LIBRARY LIBVPX_INCLUDE_DIR)

mark_as_advanced(LIBVPX_INCLUDE_DIR LIBVPX_LIBRARY)

