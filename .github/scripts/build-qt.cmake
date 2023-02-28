cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(BUILD_TYPE "release" CACHE STRING
	"The type of build ('debug' or 'release')")
set(QT_VERSION "" CACHE STRING
	"The version of Qt to build")

option(BASE "Build qtbase" ON)
option(MULTIMEDIA "Build qtmultimedia and dependencies" ON)
option(SVG "Build qtsvg" ON)
option(IMAGEFORMATS "Build qtimageformats" ON)
option(TOOLS "Build qttools" ON)
option(TRANSLATIONS "Build qttranslations" ON)

if(NOT QT_VERSION)
	message(FATAL_ERROR "-DQT_VERSION is required")
endif()

string(TOLOWER "${BUILD_TYPE}" BUILD_TYPE)
if(BUILD_TYPE STREQUAL "debug")
	message(WARNING "This build type enables ASan for some dependencies!\n"
		"You may need to use `-DCMAKE_EXE_LINKER_FLAGS_INIT=-fsanitize=address`"
		" when linking to these libraries."
	)
elseif(NOT BUILD_TYPE STREQUAL "release")
	message(FATAL_ERROR "Unknown build type '${BUILD_TYPE}'")
endif()

include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(BuildDependency)

if(WIN32)
	set(SCRIPT_SUFFIX .bat)
	list(APPEND BASE_FLAGS -mp -schannel)
endif()

if(UNIX AND NOT APPLE)
	list(APPEND BASE_FLAGS -system-freetype -fontconfig)
else()
	list(APPEND BASE_FLAGS -qt-freetype)
endif()

if(QT_VERSION VERSION_GREATER_EQUAL 6.4)
	list(APPEND BASE_FLAGS -no-feature-androiddeployqt)
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
		-no-feature-androiddeployqt
		-no-feature-qmake
	)

	# "--" sends the cmake flags of the configurator to cmake instead of
	# treating them as unknown extra defines
	list(APPEND BASE_FLAGS --)

	set(TOOLS_FLAGS
		-no-feature-clang
		-no-feature-clangcpp
	)
else()
	set(URL_LICENSE opensource-)

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
					-release -opensource -confirm-license -ltcg
					-nomake examples
					-no-sql-mysql -no-sql-odbc -no-sql-psql -sql-sqlite
					-qt-libjpeg -qt-libpng -qt-sqlite -qt-harfbuzz
					${BASE_FLAGS}
				DEBUG -force-asserts -force-debug-info -sanitize address
		PATCHES
			ALL
				patches/qtbug-111538.diff
			5.15.8
				patches/cast_types_for_egl_x11_test.diff
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
