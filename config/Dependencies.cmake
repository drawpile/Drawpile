find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Sql REQUIRED)

if(CLIENT)
	find_package(PNG MODULE REQUIRED)
	find_package(JPEG MODULE REQUIRED)
	find_package(KF5Archive REQUIRED)
	find_package(Qt5Xml REQUIRED)
	find_package(Threads REQUIRED)
	include("${DRAWDANCE_EXPORT_PATH}")

	find_package(Qt5Gui REQUIRED)
	find_package(Qt5Multimedia REQUIRED)
	find_package(Qt5Svg REQUIRED)
	find_package(Qt5Keychain)
	find_package(Qt5LinguistTools)
	find_package(QtColorWidgets)
endif()

if(CLIENT OR SERVERGUI)
	find_package(Qt5Widgets REQUIRED)
endif()

if(TESTS)
	find_package(Qt5Test REQUIRED)
endif()

find_package(Libmicrohttpd)
find_package(Sodium)

find_package(PkgConfig)
if(PKGCONFIG_FOUND)
	if(INITSYS STREQUAL "systemd")
		pkg_check_modules(SYSTEMD "libsystemd")
	endif()
endif()

# disabled pending built-in server rewrite
# find_package(KF5DNSSD NO_MODULE)
if(KF5DNSSD_FOUND)
	add_definitions(-DHAVE_DNSSD)
	# KF5DNSSD 8.84.0 moves a bunch of headers to a different place.
	if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
		add_definitions(-DHAVE_DNSSD_BEFORE_5_84_0)
	endif()
endif()
