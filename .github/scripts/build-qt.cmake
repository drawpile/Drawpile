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
option(MULTIMEDIA "Build qtmultimedia and dependencies" OFF)
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

if(QT_VERSION MATCHES "^5.K([0-9a-f]+)$")
	set(krita_qt_commit "${CMAKE_MATCH_1}")
	message(STATUS "Using Krita patched Qt5 at ${krita_qt_commit}")
	set(KRITA_QT ON)
	set(QT_VERSION "5.15.7")
else()
	set(KRITA_QT OFF)
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
			-android-ndk-platform "${ANDROID_PLATFORM}"
			# This gets detected incorrectly, it's not available on all devices.
			-no-feature-copy_file_range
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

	if(QT_VERSION VERSION_LESS 6.10)
		set(TOOLS_FLAGS -no-feature-clang -no-feature-clangcpp)
	else()
		set(TOOLS_FLAGS -no-feature-clang)
	endif()
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

if(OPENSSL)
	if("${OPENSSL_SOURCE_DATE_EPOCH}" MATCHES "^[0-9]+$")
		message(STATUS "Using given OPENSSL_SOURCE_DATE_EPOCH '${OPENSSL_SOURCE_DATE_EPOCH}'")
		list(APPEND OPENSSL_ENV "SOURCE_DATE_EPOCH=${OPENSSL_SOURCE_DATE_EPOCH}")
	else()
		message(STATUS "No valid OPENSSL_SOURCE_DATE_EPOCH, using current time")
	endif()
	string(REPLACE "." "_" openssl_prefix "${OPENSSL}")
	build_dependency(openssl ${OPENSSL} ${BUILD_TYPE}
		URL "https://github.com/openssl/openssl/releases/download/OpenSSL_${openssl_prefix}/openssl-@version@.tar.gz"
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
		PATCHES
			1.1.1t
				patches/openssl_arm64.diff
	)
endif()

if(KRITA_QT)
	set(krita_qt_commit 4b3b2a81b7a285e5705e70d11e5535fddc89b3e1)
	build_dependency(qt ${krita_qt_commit} ${BUILD_TYPE}
		URL "https://invent.kde.org/dkazakov/qt5.git"
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "qt"
		USE_GIT ON
		GIT_SUBMODULES
			qtandroidextras
			qtbase
			qtimageformats
			qtsvg
			qttools
			qttranslations
			qtwebsockets
		ALL_PLATFORMS
			${BASE_GENERATOR}
				ALL
					-opensource -confirm-license -make libs -nomake tests
					-nomake examples -no-sql-mysql -no-sql-odbc
					-no-sql-sqlite -no-sql-psql -qt-libjpeg -qt-libpng
					-qt-harfbuzz -qt-webp -no-feature-jasper
					-no-feature-mng -no-feature-tiff -no-feature-assistant
					-no-feature-distancefieldgenerator
					-no-feature-kmap2qmap -no-feature-pixeltool
					-no-feature-qdbus -no-feature-qev
					-no-feature-qtattributionsscanner -no-feature-qtdiag
					-no-feature-qtplugininfo
					-skip qt3d
					-skip qtactiveqt
					-skip qtcanvas3d
					-skip qtcharts
					-skip qtconnectivity
					-skip qtdatavis3d
					-skip qtdeclarative
					-skip qtdoc
					-skip qtdocgallery
					-skip qtfeedback
					-skip qtgamepad
					-skip qtgraphicaleffects
					-skip qtlocation
					-skip qtlottie
					-skip qtmacextras
					-skip qtmultimedia
					-skip qtnetworkauth
					-skip qtpim
					-skip qtpurchasing
					-skip qtqa
					-skip qtquick3d
					-skip qtquickcontrols
					-skip qtquickcontrols2
					-skip qtquicktimeline
					-skip qtremoteobjects
					-skip qtrepotools
					-skip qtscript
					-skip qtscxml
					-skip qtsensors
					-skip qtserialbus
					-skip qtserialport
					-skip qtspeech
					-skip qtsystems
					-skip qtvirtualkeyboard
					-skip qtwayland
					-skip qtwebchannel
					-skip qtwebengine
					-skip qtwebglplugin
					-skip qtwebview
					-skip qtwinextras
					-skip qtx11extras
					-skip qtxmlpatterns
					${BASE_FLAGS} ${TOOLS_FLAGS}
				DEBUG
					${BASE_DEBUG_FLAGS} ${BASE_DEBUG_INFO_FLAGS}
				RELWITHDEBINFO
					-release -force-debug-info
					${BASE_DEBUG_INFO_FLAGS} ${BASE_RELEASE_FLAGS}
				RELEASE
					-release ${BASE_RELEASE_FLAGS}
		PATCHES
			${krita_qt_commit}
				qtbase:patches/qtbug-111538.diff
				qtbase:patches/android_update_krita.diff
				qtbase:patches/androiddeployqt_keystore_env.diff
				qtbase:patches/cast_types_for_egl_x11_test.diff
				qtbase:patches/qmake_android_ltcg.diff
				qtbase:patches/qandroidassetsfileenginehandler_dot_slash.diff
				qtbase:patches/fusioncontrast-qt5.diff
				qtbase:patches/macostabs-qt5.diff
				qtbase:patches/windows_clipboard_sadness_krita.diff
				qtbase:patches/qt5androidsupport_params.diff
				qtbase:patches/android_backtrace_krita.diff
				qtbase:patches/android_no_build_id.diff
				qtbase:patches/kineticscrollfilter-qt5.diff
				qtbase:patches/android35-qt5-krita.diff
				qtbase:patches/android_krita_exit.diff
				qtbase:patches/qtglobal_patched_define.diff
				qtbase:patches/androidinputfocus-qt5-krita.diff
				qtbase:patches/android-fallbackpaths-qt5-krita.diff
				qtbase:patches/android-wacom-qt5-krita.diff
				qtbase:patches/android-xiaomi-qt5-krita.diff
				qtbase:patches/android-screen-density-adjustment-qt5-krita.diff
				qtbase:patches/android-mousebuttons-qt5-krita.diff
				qtbase:patches/androidf13to24-qt5-krita.diff
				qtbase:patches/androidimhnoeditmenu-qt5-krita.diff
				qtbase:patches/androidhuawei-qt5-krita.diff
				TARGET_BITS=64@qtbase:patches/android-16k-alignment-qt5-krita.diff
	)
else()
	set(TEMPLATE_URL "https://download.qt.io/archive/qt/@version_major@/@version@/submodules/@name@-everywhere-${URL_LICENSE}src-@version@.tar.xz")
	set(TEMPLATE_SOURCE_DIR "@name@-everywhere-src-@version@")

	if(BASE)
		build_dependency(qtbase ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=9d6af04e494ac144d69f09edb583be5d30267fbf700e39e8120fb47c01055f8a5a50b57e2d9cdb0794b01dfc0b1b48da
				6.7.2
				SHA384=47f0fb85e5a34220e91a5f9d4ecb3fa358442fbcd7fdeb4e7f7a86c4ecd6271268d6fbf07b03abb301e858253b8f76a2
				6.10.3
				SHA384=051ba10019d7be57fd1d413dd10fff998c7938438a77baa1f6a73f9e5ceafeb76cbed1fb55b38bb332e04837550ed412
			ALL_PLATFORMS
				${BASE_GENERATOR}
					ALL
						-opensource -confirm-license
						-nomake tests -nomake examples
						-no-sql-mysql -no-sql-odbc -no-sql-sqlite -no-sql-psql
						-no-feature-accessibility
						-qt-libjpeg -qt-libpng -qt-harfbuzz
						${BASE_FLAGS}
					DEBUG
						${BASE_DEBUG_FLAGS} ${BASE_DEBUG_INFO_FLAGS}
					RELWITHDEBINFO
						-release -force-debug-info
						${BASE_DEBUG_INFO_FLAGS} ${BASE_RELEASE_FLAGS}
					RELEASE
						-release ${BASE_RELEASE_FLAGS}
			PATCHES
				5.15.18
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
					patches/android_no_build_id.diff
					patches/kineticscrollfilter-qt5.diff
					patches/android35-qt5.diff
					# TODO patches/androidinputfocus-qt5.diff
					# TODO patches/android-fallbackpaths-qt5.diff
					# TODO 64bit patches/android-16k-alignment-qt5.diff
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
					patches/android_no_build_id.diff
					patches/browser_keyboard_input.diff
					patches/browser_file_accept.diff
					patches/kineticscrollfilter-qt6.diff
					patches/isfloattypeodr.diff
					# TODO androidinputfocus-qt6.diff
					# TODO android-fallbackpaths-qt6.diff
					# TODO 64bit android-16k-alignment-qt6.diff
				6.10.3
					patches/qtbug-113394.diff
					# TODO patches/cancel_touch_on_pen.diff
					# TODO patches/qtbug-121416.diff
					# TODO patches/qtbug-116754.diff
					# TODO patches/touchstart_prevent_default.diff
					patches/fusioncontrast-qt6.10.diff
					patches/macostabs-qt6.diff
					patches/qt6androidmacros_build_tools_revision.diff
					patches/findeglemscripten.diff
					# TODO: make these patches work.
					# patches/noasyncify.diff
					# patches/browser_keyboard_input.diff
					# patches/browser_file_accept.diff
					patches/android_no_build_id.diff
					patches/kineticscrollfilter-qt6.10.diff
					patches/nonativemessagebox.diff
					# TODO androidinputfocus-qt6.diff
					# TODO android-fallbackpaths-qt6.diff
					# TODO 64bit android-16k-alignment-qt6.diff
		)
	endif()

	if(SVG)
		build_dependency(qtsvg ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=9bf81ec5140edcac23680e9cfeb9a4c42d8a968d0f50530b78f5249ae238161e3645928b4762ac7fbc42d50961a27502
				6.7.2
				SHA384=384c7dac3434be7063fca4931c0da179657c275b2e3a343fb6b9f597c1b1390a4c17ca4a16b73f31c23841fd987685c8
				6.10.3
				SHA384=78afabc693273ffb586df02e61b6f5cf57af90f735e4c73351759a8587fc07b58e93d3ae37252c0544711074cb3d224a
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()

	if(IMAGEFORMATS)
		build_dependency(qtimageformats ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=54ab33cc6567ff0cffa8de44090440eca2a96834b1adc8cae269087af06f65b6aa79cf30b04f3387845f826b68f3345d
				6.7.2
				SHA384=fd1a98a1ff4643a4eccbcbb6d383eebf306a1f421889e4ef2a9d6cfef80360fe10c506b673db45bda0d21647c49b75aa
				6.10.3
				SHA384=392d69870ce353f6e8f3a724df6a39852c2ad97dcb22d959503df37f0a3318c39630a44a7314795bdbc756efced6474a
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
		if(NOT qtshadertools_url)
			set(qtshadertools_url "${TEMPLATE_URL}")
		endif()
		if(NOT qtshadertools_source_dir)
			set(qtshadertools_source_dir "@name@-everywhere-src-@version@")
		endif()
		build_dependency(qtshadertools ${QT_VERSION} ${BUILD_TYPE}
			URL "${qtshadertools_url}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${qtshadertools_source_dir}"
			VERSIONS
				6.7.2
				SHA384=56a7ea77e78006f805283befa415b3d78c21e7509d790b95e3fbb836495c824a736abaaf4ff01cccd019e66198edc4fe
				6.10.3
				SHA384=dbab30b387a0fc75b4e3cce6dfb48330e0e546012dac4c82e57a163ca72b17c54a5d303ab0da15496b6a02c3cba10325
			ALL_PLATFORMS
				${MODULE_GENERATOR}
			PATCHES
				6.7.2
					patches/shadertoolscstdint.diff
		)
	endif()

	if(MULTIMEDIA)
		build_dependency(qtmultimedia ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=ae7cbdd4c1a25d33a68f3df66e88affcb9f77ba693361a0a7fb5bbd6a063f619c04a1d6abb3a91344cc025b6ad608209
				6.7.2
				SHA384=ef58f56cc00b9d32dcbdcf2713f2cc7145be09b12cf919d0c8e1e7a1ec88c312c4a477d1aaf1e5ed4d16550008172584
				6.10.3
				SHA384=56d4bf4f11247095a49c531235c83f49eae9df2cdb79aede47ef2660526bc472434fbb0a67cbc242a5236df20cb74e54
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
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=ec5cf38985d8ed24c5477021a7a5ee41dc82901147e65b09f593d5fdd097a4d05f795d57edd1e159589f35c338352029
				6.7.2
				SHA384=ecd8e52476dfb7aad42acf6bb68a8ad03a307c066cb0f0be235d0a4724725fbb297ddfcd90c704667415eef73ba2a6cb
				6.10.3
				SHA384=fdaae1215e4329de70d3a2ba9d0b07ab85d7826d94c284289e1ef6b3e7cd2e7075d7e399bb5a7366f4bad399be1301d2
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
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=ff88bd8ac6e0883dbfdc5b3a17292d1fbc7bd83cb979516b31748c1eaa7fb06d32e9944d87c66f1d1267fed10234b0e4
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()

	if(TRANSLATIONS)
		build_dependency(qttranslations ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=1d3d2072d2bec21fd0cd66bad8ae1eb19fe4193feb72cf2ac6715c737008d0ccab4812876d1de442b53d7bc3d4f4e83e
				6.7.2
				SHA384=428908175b3f14da484fac4a682d85de16add57c734863ca55e4873d82826328cde2451dd2b353a8d77e96d4dbfafa82
				6.10.3
				SHA384=90d5a22165f66fc5f6349386137044106bd884d861546cdc70dc440c6258684ccbde1504f4dde52c30ca0ba2aee3454b
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()

	if(WEBSOCKETS)
		build_dependency(qtwebsockets ${QT_VERSION} ${BUILD_TYPE}
			URL "${TEMPLATE_URL}"
			TARGET_ARCH "${TARGET_ARCH}"
			SOURCE_DIR "${TEMPLATE_SOURCE_DIR}"
			VERSIONS
				5.15.18
				SHA384=37eeeb6498f50012799eea8a50ee6995ccb9a55f7c38789ae88d094dc4606b4b0e4a157aa0ed6976b2bb14388408a840
				6.7.2
				SHA384=a668d863fb73d8e25e4e334cd9bc22d7b98333186496110ba4080d007fdd52b6134ccf50c5aa671ff04540b3e10fa6a9
				6.10.3
				SHA384=f04967f255412605c8e34983633d8ae7453c817fd8aa80b9f4a74a1552cab877cac8ec08553d0612a36bca653e2eb2ad
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()
endif()
