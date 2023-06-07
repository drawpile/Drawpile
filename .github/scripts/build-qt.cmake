cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(ANDROID_SDK_ROOT "" CACHE STRING "Path to Android SDK")
set(ANDROID_NDK_ROOT "" CACHE STRING "Path to Android NDK")
set(ANDROID_HOST_PATH "" CACHE STRING "Path to Qt host installation for Android builds")
set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI to build")
set(ANDROID_PLATFORM "android-23" CACHE STRING "Android API to build")
if(ANDROID_NDK_ROOT AND ANDROID_SDK_ROOT)
	set(ANDROID true)
endif()

set(QT_VERSION "" CACHE STRING "The version of Qt to build")
if((QT_VERSION VERSION_EQUAL 5 AND WIN32) OR ANDROID)
	set(OPENSSL "1.1.1t" CACHE STRING "The version of OpenSSL to build")
else()
	set(OPENSSL "" CACHE STRING "The version of OpenSSL to build")
endif()
option(BASE "Build qtbase" ON)
option(MULTIMEDIA "Build qtmultimedia and dependencies" ON)
option(SVG "Build qtsvg" ON)
option(IMAGEFORMATS "Build qtimageformats" ON)
option(TOOLS "Build qttools" ON)
option(TRANSLATIONS "Build qttranslations" ON)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)

if(NOT QT_VERSION)
	message(FATAL_ERROR "-DQT_VERSION is required")
endif()

if(QT_VERSION VERSION_LESS 6)
	option(ANDROID_EXTRAS "Build qtandroidextras (Qt5 only)" ${ANDROID})
endif()

include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(BuildDependency)

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

	if(ANDROID_ABI STREQUAL "armeabi-v7a")
		set(openssl_abi arm)
	elseif(ANDROID_ABI STREQUAL "arm64-v8a")
		set(openssl_abi arm64)
	elseif(ANDROID_ABI STREQUAL "x86")
		set(openssl_abi x86)
	elseif(ANDROID_ABI STREQUAL "x86_64")
		set(openssl_abi x86_64)
	else()
		message(FATAL_ERROR "Unknown Android ABI '${ANDROID_ABI}' for OpenSSL")
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
endif()

if(WIN32)
	set(SCRIPT_SUFFIX .bat)
	list(APPEND BASE_FLAGS -mp -schannel)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	list(APPEND BASE_FLAGS -system-freetype -fontconfig)
else()
	list(APPEND BASE_FLAGS -qt-freetype)
endif()

# https://bugreports.qt.io/browse/QTBUG-72846 regressed in Qt6 due to
# https://gitlab.kitware.com/cmake/cmake/-/issues/23864
if(NOT APPLE OR QT_VERSION VERSION_LESS 6)
	list(APPEND BASE_FLAGS -ltcg)
endif()

if(USE_ASAN)
	list(APPEND BASE_FLAGS -sanitize address)
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
	build_dependency(openssl ${OPENSSL} ${BUILD_TYPE}
		URL "https://www.openssl.org/source/openssl-@version@.tar.gz"
		SOURCE_DIR "openssl-@version@"
		VERSIONS
			1.1.1t
			SHA384=ead831a5ccdae26e2f47f9a40dbf9ba74a073637c0fe51e137c01e7d49fc5619b71a950c8aea30020cf96710288b5d43
		ALL_PLATFORMS
			AUTOMAKE
				CONFIGURATOR "Configure"
				ASSIGN_PREFIX BROKEN_INSTALL
				MAKE_FLAGS ${OPENSSL_MAKE_FLAGS}
				INSTALL_TARGET install_sw
				ENV ${OPENSSL_ENV}
				ALL shared no-tests ${OPENSSL_FLAGS}
	)
endif()

if(BASE)
	build_dependency(qtbase ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=8bfe6995836a0c72f2c86d5bf581e512ad6552c71154bcec8afc3bcda0301da7c3a65d006529d6f03887732d23e8e34a
			6.3.2
			SHA384=42dd8d4ba9b403f1295880ecdc8dc61aa572829599414ab59bbf106dd1dc8b347e7e8dc1baeac0dff68297604bff7d3a
			6.4.2
			SHA384=d4a216e77c9a724f0ed42a6abe621c6749ec1b9309193c12f70078f11ef873b3e129a0a65ff22aa1b59eb6645b50100a
		ALL_PLATFORMS
			${BASE_GENERATOR}
				ALL
					-opensource -confirm-license
					-nomake tests -nomake examples
					-no-sql-mysql -no-sql-odbc -no-sql-psql -sql-sqlite
					-qt-libjpeg -qt-libpng -qt-sqlite -qt-harfbuzz
					${BASE_FLAGS}
				DEBUG
					${BASE_DEBUG_FLAGS} -separate-debug-info
				RELWITHDEBINFO
					-release -force-asserts -force-debug-info -separate-debug-info
				RELEASE
					-release
		PATCHES
			ALL
				patches/qtbug-111538.diff
			5.15.8
				patches/androiddeployqt_needed_libs.diff
				patches/androiddeployqt_keystore_env.diff
				patches/cast_types_for_egl_x11_test.diff
				patches/qmake_android_ltcg.diff
				patches/qtbug-112504.diff
				patches/qandroidassetsfileenginehandler_dot_slash.diff
				patches/qtbug-92199.diff
				patches/qtbug-98369.diff
			6.4.2
				patches/qtbug-109375.diff
				patches/qtbug-112697.diff
				patches/qtbug-112899.diff
				patches/qtbug-113394.diff
	)
endif()

if(SVG)
	build_dependency(qtsvg ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=e9a8b02960a39a0ba76901ec4e728fa8e97ea2c0295577658ea75817495280c561260d1874533236adc9f5bf8fd66735
			6.3.2
			SHA384=9bc135941af8a42c98a9815c85cc74a4003235031858ba6e60a231499ff427fd46076f2dca297c99a0a4dd2e4df16fb5
			6.4.2
			SHA384=ead5f7a6f1ef7ab597928d208df3139fcb6fa1f66e608a1b4406e058368715ff91e149f4553c91f07dc5ec9a33f2d40b
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(IMAGEFORMATS)
	build_dependency(qtimageformats ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=00cff893889b12f433c4d51cbe771e599a86cb022be35a7d98aa2bbf6cac30e1d77b1307bd722948dcce2eebff3ae386
			6.3.2
			SHA384=4edb67534b81e2fe7ccf646f55378f7b6d992ba8ecedab92471be035702dc748a8e97cfb4e77b6346881e9708471b3e9
			6.4.2
			SHA384=05d821bee1b39a39ac822d3fd3620028a6802171e777d219fb778d2c8df1e375b0282eef79b6203f2c3f32c9785cc3e4
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				ALL ${FLAGS_SEPARATOR}
					-qt-webp
					-no-feature-jasper
					-no-feature-mng
					-no-feature-tiff
	)
endif()

if(MULTIMEDIA)
	# Required multimedia dependency
	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		build_dependency(qtshadertools ${QT_VERSION} ${BUILD_TYPE}
			URL "${URL}"
			SOURCE_DIR "@name@-everywhere-src-@version@"
			VERSIONS
				6.3.2
				SHA384=42b85ce1cf1573d6ca514a8bed2cdbfb4215644fc0192739c3e7fe40afdfa01d8aff31460f8d8b2be8e948ecef822398
				6.4.2
				SHA384=024c1f39bcbc78213e189226b93ee54e5a0281ac0d78923fd04e3ceb6189424d9c404473c89719f78c466d19853cf1cf
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()

	build_dependency(qtmultimedia ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=2b1e7da52821e0f633e843817d0970fd712c4e01fac21ed658dd643b0ac2d5c1ed556023007c072b518b14e6936f0efa
			6.3.2
			SHA384=2e0e85c752cdd9a72aadf773a76b16604c07c0dd54ae55e966dd5ba1445349500de216e6e8ea2c4c6699863cf31f2692
			6.4.2
			SHA384=3a586108e8fd679026b4368579f3c7a12012b1143e8f98dc1b4f5b33abdd9aa2b3c75f05ce208122a3d6d1c6dda69ea4
		ALL_PLATFORMS
			${MODULE_GENERATOR}
				ALL ${FLAGS_SEPARATOR} ${MULTIMEDIA_FLAGS}
	)
endif()

if(TOOLS)
	# Required translations dependency
	build_dependency(qttools ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=07fb4db09f51a32a101ced0171291044ce9b83f7476f6ee2fa641f509e70a6e0d838952f991e9cbd029873d5f05d28ed
			6.3.2
			SHA384=b2f609dcd3f5439f1a03fd174fdf3022ecbe677cadcefa0a0c26bed34034675b4ab2055e8175a792a6e27b3c3c67a8fd
			6.4.2
			SHA384=45f265d5569bc4df3a59e92e6adf93191965869bb1b0b151f65ae04dc3f1cc79daf41759a7d701b80e1d58f58f7d1006
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
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=23ecb7927f98683770320537fe825aedb41b9a6155efa28a32b53b82c3bc9df80e125f86f63b561492f0b698824f227d
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()

if(TRANSLATIONS)
	build_dependency(qttranslations ${QT_VERSION} ${BUILD_TYPE}
		URL "${URL}"
		SOURCE_DIR "@name@-everywhere-src-@version@"
		VERSIONS
			5.15.8
			SHA384=87a78ce044c976ee4f989189ff472fcbd972d6346d78e009d45e6c2e4a92448b35fe2a8cce4fade5d18aba9d0c017c0d
			6.3.2
			SHA384=48a650886993d453399aac197de618fe634cb452e6716eac917392f5eb409feb891bd313f4d8b7dd80709468082ab7ba
			6.4.2
			SHA384=5e847ccbc6bde7da863d1269487525571a36e9d2193c21d1aa831c3a95d4378be9c5cbf6a9fb5f55fcbbf1dab649d823
		ALL_PLATFORMS
			${MODULE_GENERATOR}
	)
endif()
