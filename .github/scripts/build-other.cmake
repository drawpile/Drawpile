cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(ZLIB "1.3.1" CACHE STRING
	"The version of zlib to build")
set(LIBMICROHTTPD "1.0.1" CACHE STRING
	"The version of libmicrohttpd to build")
set(LIBSODIUM "1.0.20" CACHE STRING
	"The version of libsodium to build")
set(QTKEYCHAIN "0.14.3" CACHE STRING
	"The Git refspec of QtKeychain to build")
set(LIBZIP "1.10.1" CACHE STRING
	"The version of libzip to build")
set(KARCHIVE5 "v5.116.0" CACHE STRING
	"The version of KArchive for Qt5 to build")
set(KARCHIVE6 "v6.3.0" CACHE STRING
	"The version of KArchive for Qt6 to build")
option(KEEP_ARCHIVES "Keep downloaded archives instead of deleting them" OFF)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)
set(TARGET_ARCH "x86_64" CACHE STRING
	"Target architecture (x86, x86_64, arm32, arm64)")

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
	set(KARCHIVE "${KARCHIVE6}")
else()
	set(BUILD_WITH_QT6 off)
	set(KARCHIVE "${KARCHIVE5}")
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
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.3.1
			SHA384=fc5ef6aa369bb70bbdef1f699cd7f182404ac5305e652f67470bd70320592e8b501d516267c74957cc02beee7e06ad14
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
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.0.1
			SHA384=ac9f27e91d7b05084dfd3d2a68b00297f374af3cecb41700b207c80269147339d3da8511809dbb06fec65e9d5fdfd4be
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
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.0.20
			SHA384=67473eaecf7085446feac68c36859cbdb2cc3ecc2748b0209ca364dd00f0c836f7000790c8dec5e890cd97d6646303f1
		WIN32
			MSBUILD
				SOLUTION builds/msvc/vs2019/libsodium.sln
				SHARED DynRelease
				STATIC StaticRelease
				DEBUG_SHARED DynDebug
				DEBUG_STATIC StaticDebug
				TARGET_ARCH "${TARGET_ARCH}"
				INCLUDES src/libsodium/include/sodium.h src/libsodium/include/sodium
		UNIX AUTOMAKE
	)
endif()

if(QTKEYCHAIN)
	build_dependency(qtkeychain ${QTKEYCHAIN} ${BUILD_TYPE}
		URL https://github.com/frankosterfeld/qtkeychain/archive/@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			0.14.3
			SHA384=52d84992ce397a123591191afb47490b3ad53e29684a35241af5e25545408f7bd631223797b2b8c7ecf00380997786bc
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
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.10.1
			SHA384=b614bd95cff0c915074f9113b517298e4c10c5e4e0d2dcf25fad82516ae852da54d9330a1d910b08e8d30a733da5f86c
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
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			v5.116.0
			SHA384=c791c8d6ed0ce8ae80ac6e2479bdfb29dfb03cb36ae40e76118fe8f52058ba07dbe6d8bfca42b48891c4f9ce6f08b5ed
			v6.3.0
			SHA384=215c1b649fba07a2a57534cb30cfff9d03fcbe9b4c97d5181d46b367b4014230a5eff35b965682e3c6a6d3b0c744cf0d
		ALL_PLATFORMS
			CMAKE
				ALL
					${kf_extra_cmake_flags}
					${extra_cmake_flags}
	)
	build_dependency(karchive ${KARCHIVE} ${BUILD_TYPE}
		URL https://invent.kde.org/frameworks/karchive/-/archive/@version@/karchive-@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			v5.116.0
			SHA384=4a782fed01e559371cb12efe94060b15e31022dca90e7853b5ede0b56d2fba58410f0992ad0363fc5e495954f6d8ae56
			v6.3.0
			SHA384=5fad9c2b4196f3b698209202770cd9a8f176c09c6bd6733cc667d7c44c07146f58ef9597e9ebe6506f4f0addf051dd7f
		ALL_PLATFORMS
			CMAKE
				ALL
					-DCMAKE_DISABLE_FIND_PACKAGE_BZip2=on
					-DCMAKE_DISABLE_FIND_PACKAGE_LibLZMA=on
					-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=on
					${kf_extra_cmake_flags}
					${extra_cmake_flags}
		PATCHES
			ALL
				patches/karchive_truncate.diff
	)
endif()
