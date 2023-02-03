cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../config
)

set(BUILD_TYPE "release" CACHE STRING
	"The type of build ('debug' or 'release')")
set(LIBMICROHTTPD "0.9.75" CACHE STRING
	"The version of libmicrohttpd to build")
set(LIBSODIUM "1.0.18" CACHE STRING
	"The version of libsodium to build")
set(QTKEYCHAIN "c6f0b66318f8da6917fb4681103f7303b1836194" CACHE STRING
	"The Git refspec of QtKeychain to build")

string(TOLOWER "${BUILD_TYPE}" BUILD_TYPE)
if(BUILD_TYPE STREQUAL "debug")
	message(WARNING "This build type enables ASan for some dependencies!\n"
		"You may need to use `-DCMAKE_EXE_LINKER_FLAGS_INIT=-fsanitize=address`"
		" when linking to these libraries."
	)
elseif(NOT BUILD_TYPE STREQUAL "release")
	message(FATAL_ERROR "Unknown build type '${BUILD_TYPE}'")
endif()

if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "`-DCMAKE_INSTALL_PREFIX` is required")
endif()

# Hack to get Qt version, since its `find_package` code does not support script
# mode and this information is needed to set up QtKeychain and the macOS version
# minimum
set(CMAKE_FIND_LIBRARY_PREFIXES_OLD ${CMAKE_FIND_LIBRARY_PREFIXES})
set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_FIND_LIBRARY_SUFFIXES_OLD ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES "")
find_library(QT_CONFIG
	REQUIRED
	PATH_SUFFIXES
		CMake/Qt6
		cmake/Qt6
		CMake/Qt5
		cmake/Qt5
	NAMES
		Qt6ConfigVersion.cmake
		qt6-config-version.cmake
		Qt5ConfigVersion.cmake
		qt5-config-version.cmake
)
set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_FIND_LIBRARY_PREFIXES_OLD})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES_OLD})
include(${QT_CONFIG})
set(QT_VERSION ${PACKAGE_VERSION})

if(QT_VERSION VERSION_GREATER_EQUAL 6)
	set(BUILD_WITH_QT6 on)
else()
	set(BUILD_WITH_QT6 off)
endif()

include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(BuildDependency)

if(LIBMICROHTTPD)
	build_dependency(libmicrohttpd ${LIBMICROHTTPD} ${BUILD_TYPE}
		URL https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-@version@.tar.gz
		VERSIONS
			0.9.75
			SHA384=5a853f06d5f82c1e708c4d19758ffb77f5d1efd8431133cc956118aa49ce9b1d5b57ca468d9098127a81ed42582a97ec
		WIN32
			MSBUILD
				SOLUTION w32/VS-Any-Version/libmicrohttpd.vcxproj
				SHARED Release-dll
				STATIC Release-static
				INCLUDES src/include/microhttpd.h
				RM lib/microhttpd.h bin/microhttpd.h
		UNIX
			AUTOMAKE
				ALL --disable-doc --disable-examples --disable-curl
				DEBUG --enable-asserts --enable-sanitizers=address
	)
endif()

if(LIBSODIUM)
	build_dependency(libsodium ${LIBSODIUM} ${BUILD_TYPE}
		URL https://download.libsodium.org/libsodium/releases/libsodium-@version@.tar.gz
		VERSIONS
			1.0.18
			SHA384=1dd0171eb6aa3444f4c7aeb35dc57871f151a2e66da13a487a5cd97f2d9d5e280b995b90de53b12b174f7f649d9acd0d
		WIN32
			MSBUILD
				SOLUTION builds/msvc/vs2019/libsodium.sln
				SHARED DynRelease
				STATIC StaticRelease
				INCLUDES src/libsodium/include/sodium.h src/libsodium/include/sodium
		UNIX AUTOMAKE
	)
endif()

if(QTKEYCHAIN)
	# macdeployqt does not search rpaths correctly so give a
	# full path of the library instead
	if(APPLE)
		set(QTKEYCHAIN_FLAGS "-DCMAKE_INSTALL_NAME_DIR=${CMAKE_INSTALL_PREFIX}/lib")
	endif()

	build_dependency(qtkeychain ${QTKEYCHAIN} ${BUILD_TYPE}
		URL https://github.com/frankosterfeld/qtkeychain/archive/@version@.tar.gz
		VERSIONS
			c6f0b66318f8da6917fb4681103f7303b1836194
			SHA384=4dd6c985f0b8e2ad0a4e01cade0c230d8924ea564965098e5fa5a246ac5166ae0d9524516e2f19981af9f975955c563a
		ALL_PLATFORMS
			CMAKE
				ALL
					-DBUILD_WITH_QT6=${BUILD_WITH_QT6}
					${QTKEYCHAIN_FLAGS}
	)
endif()
