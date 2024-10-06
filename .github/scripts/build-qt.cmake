cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(ANDROID_SDK_ROOT "" CACHE STRING "Path to Android SDK")
set(ANDROID_NDK_ROOT "" CACHE STRING "Path to Android NDK")
set(ANDROID_HOST_PATH "" CACHE STRING "Path to Qt host installation for Android builds")
set(ANDROID_ABI "" CACHE STRING "Android ABI to build")
set(ANDROID_PLATFORM "android-23" CACHE STRING "Android API to build")
if(ANDROID_NDK_ROOT AND ANDROID_SDK_ROOT)
	set(ANDROID true)
endif()

option(EMSCRIPTEN "Build for WebAssembly via Emscripten" OFF)
option(EMSCRIPTEN_THREADS "Enable threads in Emscripten" ON)
set(EMSCRIPTEN_HOST_PATH "" CACHE STRING "Path to Qt host installation for Emscripten builds")

set(QT_VERSION "" CACHE STRING "The version of Qt to build")
if((QT_VERSION VERSION_LESS 6 AND WIN32) OR ANDROID)
	set(OPENSSL "1.1.1t" CACHE STRING "The version of OpenSSL to build")
else()
	set(OPENSSL "" CACHE STRING "The version of OpenSSL to build")
endif()
option(BASE "Build qtbase" ON)
option(MULTIMEDIA "Build qtmultimedia and dependencies" ON)
option(SHADERTOOLS "Build qtshadertools (built automatically for Qt6 multimedia)" OFF)
option(SVG "Build qtsvg" ON)
option(IMAGEFORMATS "Build qtimageformats" ON)
option(TOOLS "Build qttools" ON)
option(TRANSLATIONS "Build qttranslations" ON)
option(WEBSOCKETS "Build qtwebsockets" ON)
option(KEEP_ARCHIVES "Keep downloaded archives instead of deleting them" OFF)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)
set(TARGET_ARCH "x86_64" CACHE STRING
	"Target architecture (x86, x86_64, arm32, arm64)")

if(NOT QT_VERSION)
	message(FATAL_ERROR "-DQT_VERSION is required")
endif()

if(QT_VERSION VERSION_LESS 6)
	option(ANDROID_EXTRAS "Build qtandroidextras (Qt5 only)" ${ANDROID})
endif()

include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(BuildDependency)

set(enable_lto ON)
# Qt6 has various issues with link-time optimization. On Linux, rcc fails to
# build with a weird error about not finding qt_version_tag.
# https://bugreports.qt.io/browse/QTBUG-72846 regressed in Qt6 due to
# https://gitlab.kitware.com/cmake/cmake/-/issues/23864
if(QT_VERSION VERSION_GREATER_EQUAL 6)
	set(enable_lto OFF)
endif()

if(ANDROID)
	list(APPEND BASE_FLAGS
		-android-ndk "${ANDROID_NDK_ROOT}"
		-android-sdk "${ANDROID_SDK_ROOT}"
		-android-abis "${ANDROID_ABI}"
	)

	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		list(APPEND BASE_FLAGS
			-platform android-clang
			-qt-host-path "${ANDROID_HOST_PATH}"
		)
	else()
		list(APPEND BASE_FLAGS
			-xplatform android-clang
			-no-warnings-are-errors
			-disable-rpath
			# Qt5 still uses minimum 21 but NDK 25 minimum is 23
			-android-ndk-platform "${ANDROID_PLATFORM}"
			-hostprefix "${CMAKE_INSTALL_PREFIX}"
		)
	endif()

	# On Android with the x86 and x86_64 ABIs, Qt5 fails to build with
	# link-time optimization enabled. Something about not being able to find
	# _GLOBAL_OFFSET_TABLE_, which has something to do with -fPIC, which Qt
	# always includes on Android with no way to turn it off.
	if(ANDROID_ABI STREQUAL "armeabi-v7a")
		set(openssl_abi arm)
	elseif(ANDROID_ABI STREQUAL "arm64-v8a")
		set(openssl_abi arm64)
	elseif(ANDROID_ABI STREQUAL "x86")
		set(openssl_abi x86)
		set(enable_lto OFF)
	elseif(ANDROID_ABI STREQUAL "x86_64")
		set(openssl_abi x86_64)
		set(enable_lto OFF)
	else()
		message(FATAL_ERROR "Unknown Android ABI '${ANDROID_ABI}'")
	endif()

	get_android_toolchain(android_toolchain_path "${ANDROID_NDK_ROOT}")
	list(APPEND OPENSSL_ENV
		"ANDROID_NDK_HOME=${ANDROID_NDK_ROOT}"
		"PATH=${android_toolchain_path}/bin:$ENV{PATH}"
	)
	list(APPEND OPENSSL_FLAGS android-${openssl_abi})
	if(ANDROID_PLATFORM MATCHES "^android-([0-9]+)$")
		list(APPEND OPENSSL_FLAGS "-D__ANDROID_API__=${CMAKE_MATCH_1}")
	else()
		message(FATAL_ERROR "Invalid Android platform '${ANDROID_PLATFORM}'")
	endif()
	list(APPEND OPENSSL_MAKE_FLAGS SHLIB_VERSION_NUMBER= SHLIB_EXT=_1_1.so)
elseif(EMSCRIPTEN)
	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		list(APPEND BASE_FLAGS
			-platform wasm-emscripten
			-qt-host-path "${EMSCRIPTEN_HOST_PATH}"
			-device-option QT_EMSCRIPTEN_ASYNCIFY=1
		)
		if(EMSCRIPTEN_THREADS)
			list(APPEND BASE_FLAGS -feature-thread)
		else()
			list(APPEND BASE_FLAGS -no-feature-thread)
		endif()
	else()
		message(FATAL_ERROR "Building for Emscripten is only implemented for Qt6")
	endif()
endif()

if(WIN32)
	set(SCRIPT_SUFFIX .bat)
	list(APPEND BASE_FLAGS -mp -schannel)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
	list(APPEND BASE_FLAGS -system-freetype -fontconfig -openssl-linked)
else()
	list(APPEND BASE_FLAGS -qt-freetype)
endif()

if(USE_ASAN)
	list(APPEND BASE_FLAGS -sanitize address)
endif()

if(EMSCRIPTEN)
	unset(BASE_DEBUG_INFO_FLAGS)
	set(BASE_RELEASE_FLAGS -feature-optimize_full)
else()
	set(BASE_DEBUG_INFO_FLAGS -separate-debug-info)
	if(enable_lto)
		list(APPEND BASE_FLAGS -ltcg)
	endif()
endif()

if(QT_VERSION VERSION_GREATER_EQUAL 6.4)
	list(APPEND MULTIMEDIA_FLAGS
		-no-feature-ffmpeg
		-no-feature-spatialaudio
	)
endif()

if(QT_VERSION VERSION_GREATER_EQUAL 6)
	set(BASE_GENERATOR
		CMAKE NO_DEFAULT_BUILD_TYPE
		CONFIGURATOR configure${SCRIPT_SUFFIX}
	)
	set(MODULE_GENERATOR
		CMAKE NO_DEFAULT_FLAGS
		CONFIGURATOR ${CMAKE_INSTALL_PREFIX}/bin/qt-configure-module${SCRIPT_SUFFIX} @source_dir@
	)

	list(APPEND BASE_FLAGS
		-no-feature-qmake
	)
	list(APPEND BASE_DEBUG_FLAGS -debug)

	# "--" sends the cmake flags of the configurator to cmake instead of
	# treating them as unknown extra defines
	list(APPEND BASE_FLAGS --)

	if(OPENSSL)
		list(APPEND BASE_FLAGS "-DOPENSSL_ROOT_DIR=${CMAKE_INSTALL_PREFIX}")
	endif()

	set(TOOLS_FLAGS
		-no-feature-clang
		-no-feature-clangcpp
	)
else()
	set(URL_LICENSE opensource-)

	if(OPENSSL)
		list(APPEND BASE_FLAGS "OPENSSL_PREFIX=${CMAKE_INSTALL_PREFIX}")
	endif()
	list(APPEND BASE_DEBUG_FLAGS -force-debug-info)

	# Module feature flags must be passed as inputs instead of flags
	set(FLAGS_SEPARATOR --)

	set(BASE_GENERATOR
		QMAKE
		CONFIGURATOR configure${SCRIPT_SUFFIX} -prefix "${CMAKE_INSTALL_PREFIX}"
	)
	set(MODULE_GENERATOR
		QMAKE
		CONFIGURATOR ${CMAKE_INSTALL_PREFIX}/bin/qmake @source_dir@
	)

	set(TOOLS_FLAGS
		-no-feature-makeqpf
		-no-feature-qtpaths
		-no-feature-winrtrunner
	)
endif()

set(URL https://download.qt.io/archive/qt/@version_major@/@version@/submodules/@name@-everywhere-${URL_LICENSE}src-@version@.tar.xz)

if(OPENSSL)
	string(REGEX REPLACE "[a-z]+$" "" openssl_prefix "${OPENSSL}")
	build_dependency(openssl ${OPENSSL} ${BUILD_TYPE}
		URL "https://www.openssl.org/source/old/${openssl_prefix}/openssl-@version@.tar.gz"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "openssl-@version@"
		VERSIONS
			1.1.1t
			SHA384=ead831a5ccdae26e2f47f9a40dbf9ba74a073637c0fe51e137c01e7d49fc5619b71a950c8aea30020cf96710288b5d43
		ALL_PLATFORMS
			AUTOMAKE
				CONFIGURATOR "Configure"
				ASSIGN_PREFIX BROKEN_INSTALL NEEDS_VC_WIN_TARGET
				MAKE_FLAGS ${OPENSSL_MAKE_FLAGS}
				WIN32_CONFIGURE_COMMAND perl
				WIN32_MAKE_COMMAND nmake
				INSTALL_TARGET install_sw
				ENV ${OPENSSL_ENV}
				ALL shared no-tests ${OPENSSL_FLAGS}
	)
endif()

if(BASE)
	build_dependency(qtbase ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=92fdb994e7e4c0e616c650dbb01957193151573d206ac4a8d4213bf62af36f9429279a7efbb58be5f50436ab277ac1a5
			6.7.2
			SHA384=47f0fb85e5a34220e91a5f9d4ecb3fa358442fbcd7fdeb4e7f7a86c4ecd6271268d6fbf07b03abb301e858253b8f76a2
		ALL_PLATFORMS
			${BASE_GENERATOR}
				ALL
					-opensource -confirm-license
					-nomake tests -nomake examples
					-no-sql-mysql -no-sql-odbc -no-sql-psql -sql-sqlite
					-qt-libjpeg -qt-libpng -qt-sqlite -qt-harfbuzz
					${BASE_FLAGS}
				DEBUG
					${BASE_DEBUG_FLAGS} ${BASE_DEBUG_INFO_FLAGS}
				RELWITHDEBINFO
					-release -force-debug-info
					${BASE_DEBUG_INFO_FLAGS} ${BASE_RELEASE_FLAGS}
				RELEASE
					-release ${BASE_RELEASE_FLAGS}
		PATCHES
			5.15.14
				patches/qtbug-111538.diff
				patches/androiddeployqt_keystore_env.diff
				patches/cast_types_for_egl_x11_test.diff
				patches/qmake_android_ltcg.diff
				patches/qandroidassetsfileenginehandler_dot_slash.diff
				patches/fusioncontrast-qt5.diff
				patches/macostabs-qt5.diff
				patches/windows_clipboard_sadness.diff
				patches/qt5androidsupport_params.diff
				patches/android_backtrace.diff
			6.7.2
				patches/qtbug-113394.diff
				patches/cancel_touch_on_pen.diff
				patches/qtbug-121416.diff
				patches/qtbug-116754.diff
				patches/touchstart_prevent_default.diff
				patches/webgl_is_hard.diff
				patches/fusioncontrast-qt6.diff
				patches/macostabs-qt6.diff
				patches/qtbug-127468.diff
				patches/qt6androidmacros_build_tools_revision.diff
	)
endif()

if(SVG)
	build_dependency(qtsvg ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=31d3776e5d4ed77b2633dc4618c1458b46b002f238e83cc09affeea0b3a01f101260889e7a909e907e6bd9ad127908c2
			6.7.2
			SHA384=384c7dac3434be7063fca4931c0da179657c275b2e3a343fb6b9f597c1b1390a4c17ca4a16b73f31c23841fd987685c8
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(IMAGEFORMATS)
	build_dependency(qtimageformats ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=2ae3f0acf830232c265d320aae9631afce7d1636fb4a5e385d81f983d6786d15c49a969a10faebb4cbad61e584c86d2a
			6.7.2
			SHA384=fd1a98a1ff4643a4eccbcbb6d383eebf306a1f421889e4ef2a9d6cfef80360fe10c506b673db45bda0d21647c49b75aa
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				ALL ${FLAGS_SEPARATOR}
					-qt-webp
					-no-feature-jasper
					-no-feature-mng
					-no-feature-tiff
	)
endif()

# Required multimedia dependency in Qt6
if(SHADERTOOLS OR (QT_VERSION VERSION_GREATER_EQUAL 6 AND MULTIMEDIA))
	build_dependency(qtshadertools ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			6.7.2
			SHA384=56a7ea77e78006f805283befa415b3d78c21e7509d790b95e3fbb836495c824a736abaaf4ff01cccd019e66198edc4fe
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(MULTIMEDIA)
	build_dependency(qtmultimedia ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=f28af7f2d40dcb926765683b54faaffa6a439ab5f8e472185e2ebb9ec5635b3ea5cb39a7d3cc11e045c23e64019edea3
			6.7.2
			SHA384=ef58f56cc00b9d32dcbdcf2713f2cc7145be09b12cf919d0c8e1e7a1ec88c312c4a477d1aaf1e5ed4d16550008172584
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				ALL ${FLAGS_SEPARATOR} ${MULTIMEDIA_FLAGS}
	)
endif()

if(TOOLS)
	# Required translations dependency
	build_dependency(qttools ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=c48a461ffaed6a2622cbe8641a6fb8cfe7051d0688f373da608a514bc11a66ca841cb30e9ebe8c4459ec0796343d02af
			6.7.2
			SHA384=ecd8e52476dfb7aad42acf6bb68a8ad03a307c066cb0f0be235d0a4724725fbb297ddfcd90c704667415eef73ba2a6cb
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				# linguist is required for translation compilation, and it transitively
				# requires designer for some reason. The rest are useless
				ALL ${FLAGS_SEPARATOR}
					-no-feature-assistant
					-no-feature-distancefieldgenerator
					-no-feature-kmap2qmap
					-no-feature-pixeltool
					-no-feature-qdbus
					-no-feature-qev
					-no-feature-qtattributionsscanner
					-no-feature-qtdiag
					-no-feature-qtplugininfo
					${TOOLS_FLAGS}
	)
endif()

if(ANDROID_EXTRAS)
	build_dependency(qtandroidextras ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=4a6f9b5236ff324cab1839785a9dc811030fb02e80e6ea30dda2ccdd0045ff60b9a371b3a7e91a447c41aee0c9fdf5fc
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(TRANSLATIONS)
	build_dependency(qttranslations ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=8ad2341bb4694f5d045c9217df864820cfd64cd526497452181165afb37b0d2cb731213e9e14b56d742cf093bc15f74b
			6.7.2
			SHA384=428908175b3f14da484fac4a682d85de16add57c734863ca55e4873d82826328cde2451dd2b353a8d77e96d4dbfafa82
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(WEBSOCKETS)
	build_dependency(qtwebsockets ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.14
			SHA384=3fa6de687ecf9c4e9ac72c2b4af1dc37e06e55986e1573cc22a5287dbac4717db11502c885c98f9e5c2488c005740028
			6.7.2
			SHA384=a668d863fb73d8e25e4e334cd9bc22d7b98333186496110ba4080d007fdd52b6134ccf50c5aa671ff04540b3e10fa6a9
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()
