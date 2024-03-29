cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(ZLIB "1.2.13" CACHE STRING
	"The version of zlib to build")
set(LIBMICROHTTPD "0.9.75" CACHE STRING
	"The version of libmicrohttpd to build")
set(LIBSODIUM "1.0.18" CACHE STRING
	"The version of libsodium to build")
set(QTKEYCHAIN "c6f0b66318f8da6917fb4681103f7303b1836194" CACHE STRING
	"The Git refspec of QtKeychain to build")
set(LIBZIP "1.9.2" CACHE STRING
	"The version of libzip to build")
set(KARCHIVE "v5.105.0" CACHE STRING
	"The version of KArchive to build")
option(KEEP_ARCHIVES "Keep downloaded archives instead of deleting them" OFF)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)
set(TARGET_BITS "64" CACHE STRING
	"Platform bits (32 or 64, only relevant on Windows)")

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

# macdeployqt does not search rpaths correctly so give a full path of the
# library instead
if(APPLE AND NOT ANDROID)
	set(extra_cmake_flags "-DCMAKE_INSTALL_NAME_DIR=${CMAKE_INSTALL_PREFIX}/lib")
endif()

if(WIN32 AND ZLIB)
	build_dependency(zlib ${ZLIB} ${BUILD_TYPE}
		URL https://github.com/madler/zlib/releases/download/v@version@/zlib-@version@.tar.xz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			1.2.13
			SHA384=57f9fd368500c413cf5fafd5ffddf150651a43de580051d659fab0fcacbf1fb63f4954851895148e530afa3b75d48433
		ALL_PLATFORMS
			CMAKE
				ALL -DBUILD_SHARED_LIBS=on ${extra_cmake_flags}
	)
endif()

if(LIBMICROHTTPD)
	if(USE_ASAN)
		set(extra_debug_flags --enable-sanitizers=address)
	endif()

	build_dependency(libmicrohttpd ${LIBMICROHTTPD} ${BUILD_TYPE}
		URL https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-@version@.tar.gz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			0.9.75
			SHA384=5a853f06d5f82c1e708c4d19758ffb77f5d1efd8431133cc956118aa49ce9b1d5b57ca468d9098127a81ed42582a97ec
		WIN32
			MSBUILD
				SOLUTION w32/VS-Any-Version/libmicrohttpd.vcxproj
				SHARED Release-dll
				STATIC Release-static
				DEBUG_SHARED Debug-dll
				DEBUG_STATIC Debug-static
				INCLUDES src/include/microhttpd.h
				RM lib/microhttpd.h bin/microhttpd.h
		UNIX
			AUTOMAKE
				ALL --disable-doc --disable-examples --disable-curl
				DEBUG --enable-asserts ${extra_debug_flags}
				RELWITHDEBINFO --enable-asserts
	)
endif()

if(LIBSODIUM)
	build_dependency(libsodium ${LIBSODIUM} ${BUILD_TYPE}
		URL https://download.libsodium.org/libsodium/releases/libsodium-@version@.tar.gz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			1.0.18
			SHA384=1dd0171eb6aa3444f4c7aeb35dc57871f151a2e66da13a487a5cd97f2d9d5e280b995b90de53b12b174f7f649d9acd0d
		WIN32
			MSBUILD
				SOLUTION builds/msvc/vs2019/libsodium.sln
				SHARED DynRelease
				STATIC StaticRelease
				DEBUG_SHARED DynDebug
				DEBUG_STATIC StaticDebug
				TARGET_BITS "${TARGET_BITS}"
				INCLUDES src/libsodium/include/sodium.h src/libsodium/include/sodium
		UNIX AUTOMAKE
	)
endif()

if(QTKEYCHAIN)
	build_dependency(qtkeychain ${QTKEYCHAIN} ${BUILD_TYPE}
		URL https://github.com/frankosterfeld/qtkeychain/archive/@version@.tar.gz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			c6f0b66318f8da6917fb4681103f7303b1836194
			SHA384=4dd6c985f0b8e2ad0a4e01cade0c230d8924ea564965098e5fa5a246ac5166ae0d9524516e2f19981af9f975955c563a
		ALL_PLATFORMS
			CMAKE
				ALL
					-DBUILD_WITH_QT6=${BUILD_WITH_QT6}
					${extra_cmake_flags}
	)
endif()

if(LIBZIP AND BUILD_WITH_QT6)
	build_dependency(libzip ${LIBZIP} ${BUILD_TYPE}
		URL https://libzip.org/download/libzip-@version@.tar.xz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			1.9.2
			SHA384=3fae34c63ac4e40d696bf5b95ff25d38c572c9e01f71350f065902f371c93db14fdee727d0179421f03b67c129d0f567
		ALL_PLATFORMS
			CMAKE
				ALL
					-DBUILD_TOOLS=off -DBUILD_REGRESS=off
					-DBUILD_DOC=off -DBUILD_EXAMPLES=off
					-DENABLE_COMMONCRYPTO=off -DENABLE_GNUTLS=off
					-DENABLE_MBEDTLS=off -DENABLE_OPENSSL=off
					-DENABLE_WINDOWS_CRYPTO=off -DENABLE_BZIP2=off
					-DENABLE_LZMA=off -DENABLE_ZSTD=off
					${extra_cmake_flags}
	)
endif()

if(KARCHIVE AND NOT BUILD_WITH_QT6)
	# KDE will install into multiarch directories on some flavors of Linux, even
	# when not installing into the system prefix. That breaks linuxdeploy, so we
	# specify explicitly that we really just want the regular lib directory.
	if(UNIX AND NOT APPLE AND NOT ANDROID)
		set(kf_extra_cmake_flags -DKDE_INSTALL_LIBDIR=lib)
	else()
		unset(kf_extra_cmake_flags)
	endif()
	# All KF libraries get released under a shared, synchronized version number,
	# so we don't need to provide separate arguments for these.
	build_dependency(extra-cmake-modules ${KARCHIVE} ${BUILD_TYPE}
		URL https://invent.kde.org/frameworks/extra-cmake-modules/-/archive/@version@/extra-cmake-modules-@version@.tar.gz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			v5.105.0
			SHA384=e7f6cdee1f388d44024fc20ef663562a0ab2e0851745003c83b437aa620d0f808dea7ddb60fbf1e33161a83f08815a9c
		ALL_PLATFORMS
			CMAKE
				ALL
					${kf_extra_cmake_flags}
					${extra_cmake_flags}
	)
	build_dependency(karchive ${KARCHIVE} ${BUILD_TYPE}
		URL https://invent.kde.org/frameworks/karchive/-/archive/@version@/karchive-@version@.tar.gz
		TARGET_BITS "${TARGET_BITS}"
		VERSIONS
			v5.105.0
			SHA384=51b17b9f8d75927c6a9708c705eaef1611c3b76f157bf04f550af8ff82bf02d55e46f03b4add369751d7ef0b51086eb7
		ALL_PLATFORMS
			CMAKE
				ALL
					-DCMAKE_DISABLE_FIND_PACKAGE_BZip2=on
					-DCMAKE_DISABLE_FIND_PACKAGE_LibLZMA=on
					-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=on
					${kf_extra_cmake_flags}
					${extra_cmake_flags}
	)
endif()
