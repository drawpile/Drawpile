include(FindPackageHandleStandardArgs)

find_path(libmicrohttpd_INCLUDE_DIR NAMES microhttpd.h)
mark_as_advanced(libmicrohttpd_INCLUDE_DIR)

find_library(libmicrohttpd_LIBRARY NAMES microhttpd)
mark_as_advanced(libmicrohttpd_LIBRARY)

find_package_handle_standard_args(
	libmicrohttpd
	REQUIRED_VARS libmicrohttpd_LIBRARY libmicrohttpd_INCLUDE_DIR
)

if(libmicrohttpd_FOUND)
	add_library(libmicrohttpd::libmicrohttpd UNKNOWN IMPORTED GLOBAL)
	set_target_properties(libmicrohttpd::libmicrohttpd PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${libmicrohttpd_INCLUDE_DIR}"
		IMPORTED_LOCATION "${libmicrohttpd_LIBRARY}"
	)
endif()
