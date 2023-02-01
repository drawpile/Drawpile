find_package("Qt${QT_VERSION}" COMPONENTS Network Sql REQUIRED)

if(CLIENT)
	find_package(PNG MODULE REQUIRED)
	find_package(JPEG MODULE REQUIRED)
	find_package("Qt${QT_VERSION}" COMPONENTS Xml REQUIRED)
	find_package(Threads REQUIRED)

	if("${QT_VERSION}" EQUAL 5)
		find_package(KF5Archive REQUIRED)
	else()
		find_package(libzip REQUIRED)
	endif()

	include("${DRAWDANCE_EXPORT_PATH}")

	find_package("Qt${QT_VERSION}" COMPONENTS Gui Svg Multimedia REQUIRED)
	find_package("Qt${QT_VERSION}" COMPONENTS Keychain LinguistTools)
	find_package(QtColorWidgets)
endif()

if(CLIENT OR SERVERGUI)
	find_package("Qt${QT_VERSION}" COMPONENTS Widgets REQUIRED)
endif()

if(TESTS)
	find_package("Qt${QT_VERSION}" COMPONENTS Test REQUIRED)
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
