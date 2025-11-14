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
				qtbase:patches/android-fallbackpaths-qt5.diff
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
				5.15.17
				SHA384=7a39dfa69b2ef819afde52f2f905907a31ff1a9f9f5c4698f8d042994a2fa08e5632bd98e7ced76b8e333645bb849d15
				6.7.2
				SHA384=47f0fb85e5a34220e91a5f9d4ecb3fa358442fbcd7fdeb4e7f7a86c4ecd6271268d6fbf07b03abb301e858253b8f76a2
				6.8.3
				SHA384=ad909f70006e9ab97da86505a4b93dd23507a647b95f05c4ae249751442cb5badb1db9b2ebda14e6844f50509489ada4
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
				5.15.17
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
					TARGET_BITS=64@patches/android-16k-alignment-qt5.diff
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
				6.8.3
					patches/qtbug-113394.diff
					patches/cancel_touch_on_pen.diff
					patches/qtbug-121416.diff
					patches/qtbug-116754.diff
					patches/touchstart_prevent_default.diff
					patches/fusioncontrast-qt6.diff
					patches/macostabs-qt6.diff
					patches/qt6androidmacros_build_tools_revision.diff
					patches/findeglemscripten.diff
					# TODO: make these patches work.
					# patches/noasyncify.diff
					# patches/browser_keyboard_input.diff
					# patches/browser_file_accept.diff
					patches/android_no_build_id.diff
					patches/kineticscrollfilter-qt6.diff
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
				5.15.17
				SHA384=7346d8ea7f756406610614087bee6ac90883b22ac017fa0e15361fae5191cbdea5f72ae3c80292ab7b169a2603241362
				6.7.2
				SHA384=384c7dac3434be7063fca4931c0da179657c275b2e3a343fb6b9f597c1b1390a4c17ca4a16b73f31c23841fd987685c8
				6.8.3
				SHA384=79d688174b899383b87da518f8817db6203bc7d847e29b19c1a180ae75baa67c6934185fa76f95b0f59e664eaa9fc7d5
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
				5.15.17
				SHA384=457917b0c8eeb6ebcbe8bf754a3dc6482b91149e910354d1c88958edd2e0dba614feecfda8afe18c1932ad950c0e569a
				6.7.2
				SHA384=fd1a98a1ff4643a4eccbcbb6d383eebf306a1f421889e4ef2a9d6cfef80360fe10c506b673db45bda0d21647c49b75aa
				6.8.3
				SHA384=f9ee66cb619e6070dbf8974b0bcc15a4dfc89e0eca202da2ee8a201224d54570eb469a072a349f198b20649b70e1f4ff
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
				6.8.3
				SHA384=cb2d4f4fe3e5aa556d6923cdb1408444114730ef9cb52f10796bd363ad9f4da38b3676633f1fe06678d10eee695cf97f
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
				5.15.17
				SHA384=ff33b29081e7f7f609121e4b06aaa4f44a9b999210a0a37e4966f97b9b4ab8a8fd31ccf6970268ca57e741e9e9e4e432
				6.7.2
				SHA384=ef58f56cc00b9d32dcbdcf2713f2cc7145be09b12cf919d0c8e1e7a1ec88c312c4a477d1aaf1e5ed4d16550008172584
				6.8.3
				SHA384=bdac10fe1da84398f026a32cda120d109ab9ef3e6c938d64d20e93e2208da0d44b73027924057bf6dd06992fb0e139c1
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
				5.15.17
				SHA384=fba513518a1fbaef8b9515466a47ae9dc2edc513a8a5e988ebc2b1df19ca980b5a70f74e131970bba2f1d63d4a10b2b8
				6.7.2
				SHA384=ecd8e52476dfb7aad42acf6bb68a8ad03a307c066cb0f0be235d0a4724725fbb297ddfcd90c704667415eef73ba2a6cb
				6.8.3
				SHA384=1c0d200820356f8cc297ba471d1873a4d3b5feda8a8c2d765cf12e216dcafbc350d3bebdf526f777484b6321fcd4e49c
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
				5.15.17
				SHA384=54fa55d66ef7983ac62e22a0d2b5b2866d53bfffd8bb8aacfec388d5c89dbca15d4dc39b7e99f6ee180e6147341f0814
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
				5.15.17
				SHA384=fe4edf5d4a8867950574c3218190a71be58d99eadde34505a6b0a2d759fda241822ada837cc2e08035dc23ffb6957117
				6.7.2
				SHA384=428908175b3f14da484fac4a682d85de16add57c734863ca55e4873d82826328cde2451dd2b353a8d77e96d4dbfafa82
				6.8.3
				SHA384=a7c5bef878c88646f9db9bc102fdc733a748e2a22d1d40f050c435c929acd858cf742a3408ae983e7df97eccc1334240
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
				5.15.17
				SHA384=4fd8b976f17b9a51eeac08eb0ca2cc90786f649ce4b801004aa2cfd22db618bce89f81ecff5b89cc461f9cdc20c2fd6f
				6.7.2
				SHA384=a668d863fb73d8e25e4e334cd9bc22d7b98333186496110ba4080d007fdd52b6134ccf50c5aa671ff04540b3e10fa6a9
				6.8.3
				SHA384=1bf6667c83d90b1100e4e5f2bc14a2936392d5a7cd4bb7842f573390b1d9913c6c6d09405d1c2a47525ca174c7edc86c
			ALL_PLATFORMS
				${MODULE_GENERATOR}
		)
	endif()
endif()
