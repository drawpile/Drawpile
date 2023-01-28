include(FindPackageHandleStandardArgs)

find_path(libsodium_INCLUDE_DIR NAMES sodium.h)
mark_as_advanced(libsodium_INCLUDE_DIR)

find_library(libsodium_LIBRARY NAMES sodium)
mark_as_advanced(libsodium_LIBRARY)

find_package_handle_standard_args(
	libsodium
	REQUIRED_VARS libsodium_LIBRARY libsodium_INCLUDE_DIR
)

if(libsodium_FOUND)
	add_library(libsodium::libsodium UNKNOWN IMPORTED)
	set_target_properties(libsodium::libsodium PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${libsodium_INCLUDE_DIR}"
		IMPORTED_LOCATION "${libsodium_LIBRARY}"
	)
endif()
