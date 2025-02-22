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
option(EMSCRIPTEN_ASYNCIFY "Enable Asyncify in Emscripten" OFF)
option(EMSCRIPTEN_THREADS "Enable threads in Emscripten" ON)
set(EMSCRIPTEN_HOST_PATH "" CACHE STRING "Path to Qt host installation for Emscripten builds")

set(QT_VERSION "" CACHE STRING "The version of Qt to build")
if((QT_VERSION VERSION_LESS 6 AND WIN32) OR ANDROID)
	set(OPENSSL "1.1.1t" CACHE STRING "The version of OpenSSL to build")
else()
	set(OPENSSL "" CACHE STRING "The version of OpenSSL to build")
endif()
option(BASE "Build qtbase" ON)
if(WIN32)
	set(MULTIMEDIA_DEFAULT OFF)
else()
	set(MULTIMEDIA_DEFAULT ON)
endif()
option(MULTIMEDIA "Build qtmultimedia and dependencies" "${MULTIMEDIA_DEFAULT}")
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
	"Target architecture (x86, x86_64, arm32, arm64, wasm)")

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
		)

		if(EMSCRIPTEN_ASYNCIFY)
			message(STATUS "Building with Emscripten  Asyncify")
			list(APPEND BASE_FLAGS -device-option QT_EMSCRIPTEN_ASYNCIFY=1)
		else()
			message(STATUS "Building without Emscripten Asyncify")
		endif()

		if(EMSCRIPTEN_THREADS)
			message(STATUS "Building with Emscripten threads")
			list(APPEND BASE_FLAGS -feature-thread)
		else()
			message(STATUS "Building without Emscripten threads")
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
	if("${OPENSSL_SOURCE_DATE_EPOCH}" MATCHES "^[0-9]+$")
		message(STATUS "Using given OPENSSL_SOURCE_DATE_EPOCH '${OPENSSL_SOURCE_DATE_EPOCH}'")
		list(APPEND OPENSSL_ENV "SOURCE_DATE_EPOCH=${OPENSSL_SOURCE_DATE_EPOCH}")
	else()
		message(STATUS "No valid OPENSSL_SOURCE_DATE_EPOCH, using current time")
	endif()
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
			5.15.16
			SHA384=249460e42289f758fc932d667339745b7f5b72769558a40df33cda295032866a9bdb0b7f71a6e63df1021f75e0829bde
			6.7.2
			SHA384=47f0fb85e5a34220e91a5f9d4ecb3fa358442fbcd7fdeb4e7f7a86c4ecd6271268d6fbf07b03abb301e858253b8f76a2
			6.8.2
			SHA384=e24776bbbcda6572423879f83782e6a160cd43f76f6e2998fd913e8bb875fcfe280ebd80334530bdf80cca7a126d5a21
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
			5.15.16
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
				patches/androidtilt-qt5.diff
				patches/qtbug-104895.diff
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
				patches/findeglemscripten.diff
				patches/embool_qtbase.diff
				patches/noasyncify.diff
			6.8.2
				patches/qtbug-113394.diff
				patches/cancel_touch_on_pen.diff
				patches/qtbug-121416.diff
				patches/qtbug-116754.diff
				patches/touchstart_prevent_default.diff
				patches/fusioncontrast-qt6.diff
				patches/macostabs-qt6.diff
				patches/qt6androidmacros_build_tools_revision.diff
				patches/findeglemscripten.diff
				# TODO: make this patch work.
				# patches/noasyncify.diff
	)
endif()

if(SVG)
	build_dependency(qtsvg ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.16
			SHA384=5e18ae955ec4e9a220f27274024804c60cafdfcf4aa8b01b018526ecd6d10a83012eb4e605e22cf9b22eefc2aa37d5f6
			6.7.2
			SHA384=384c7dac3434be7063fca4931c0da179657c275b2e3a343fb6b9f597c1b1390a4c17ca4a16b73f31c23841fd987685c8
			6.8.2
			SHA384=12e666205de53b83341f76f07baf8b5e3969819973e892434f3f2fab285af64a32f11e0287b274bb055bcfc24a58cce9
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
			5.15.16
			SHA384=5b68e0e1eacf67e31957d0ac29bf2fce1a745788e693737c328983e7e8546ed131bc290d76ede47f254276d36acdaa47
			6.7.2
			SHA384=fd1a98a1ff4643a4eccbcbb6d383eebf306a1f421889e4ef2a9d6cfef80360fe10c506b673db45bda0d21647c49b75aa
			6.8.2
			SHA384=ea3a0ced17731a4d499f1dd2db2c07f6190a31477b867a8a51bb2d90d113079a9b9926b07db74aee58e9cffcae1ad430
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
			6.8.2
			SHA384=d933f27c972dc379501644a1b4484d56b36534c6a070828819d8a27ed52d8dfaddac40a358e2f4a16bf2be02ba1b94ce
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
			5.15.16
			SHA384=87cdb677c2566e4b8148bbfaccf40c28162818918d733f4852259f591a236cb5b051650a6116f35737e84598fe250b81
			6.7.2
			SHA384=ef58f56cc00b9d32dcbdcf2713f2cc7145be09b12cf919d0c8e1e7a1ec88c312c4a477d1aaf1e5ed4d16550008172584
			6.8.2
			SHA384=ac688e786a555714bf03b882b1975020fe6c0c0d1085d0fbf60a37d8e2ab94a78249d1879391004babee91268db187a2
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				ALL ${FLAGS_SEPARATOR} ${MULTIMEDIA_FLAGS}
		PATCHES
			6.7.2
				patches/embool_qtmultimedia.diff
	)
endif()

if(TOOLS)
	# Required translations dependency
	build_dependency(qttools ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.16
			SHA384=6e294b46e790ea46e96ddffcee3e93b5e2bf9e4f5ab88f39dc55a3229b7630555206687bb6749eae916d31718807cc98
			6.7.2
			SHA384=ecd8e52476dfb7aad42acf6bb68a8ad03a307c066cb0f0be235d0a4724725fbb297ddfcd90c704667415eef73ba2a6cb
			6.8.2
			SHA384=5f71cbc0b6ced546e721aefdaefee55cb2a958fa2feda476887bb4e84b519c3ba88d0d2e270ce6c6ab977d6d034ab7e2
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
			5.15.16
			SHA384=e5db5dd11fa0d57f25ed3547f9db6da3db7bc33ccb06d83565f4615bb5839d3bb16812e219a5f7f2b5c4f28f7409c22d
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
			5.15.16
			SHA384=037591740dae69f87b4cf661c028d9d03c8063c91d2c8e136323d0cc8fc42a0c1387e770e79d1411f0dac4e202249efd
			6.7.2
			SHA384=428908175b3f14da484fac4a682d85de16add57c734863ca55e4873d82826328cde2451dd2b353a8d77e96d4dbfafa82
			6.8.2
			SHA384=b788993ba61e704a9a19b3e0217a8f4c3aa57c6b67e4cc3fae562387bef219e532021a4ae43ca2df87c0cfdc91448d0b
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
			5.15.16
			SHA384=bb97b8d8dbffc18074a3e1be7c781cef89d42a8366821836f59e2b0fe433f6ad0613bdc32c238d2d18fc4c50f88923bc
			6.7.2
			SHA384=a668d863fb73d8e25e4e334cd9bc22d7b98333186496110ba4080d007fdd52b6134ccf50c5aa671ff04540b3e10fa6a9
			6.8.2
			SHA384=85d9ba85077b051a94bb4b443188157944c70be02afd5610ad26f493f82e2b92253f9331d41113ecbcda81e2162c7771
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()
