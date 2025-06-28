# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(LIBVPX "1.15.2" CACHE STRING
	"The version of libvpx to build")
set(LIBWEBP "1.5.0" CACHE STRING
	"The version of libwebp to build")
set(FFMPEG "7.1.1" CACHE STRING
	"The version of ffmpeg to build")
option(KEEP_ARCHIVES "Keep downloaded archives instead of deleting them" OFF)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)
option(EMSCRIPTEN "Build for WebAssembly via Emscripten" OFF)
option(EMSCRIPTEN_THREADS "Enable threads in Emscripten" ON)
set(TARGET_ARCH "x86_64" CACHE STRING
	"Target architecture (x86, x86_64, arm32, arm64, wasm)")
set(OVERRIDE_CMAKE_COMMAND "" CACHE STRING
	"Command to use to run cmake (instead of ${CMAKE_COMMAND})")

include(BuildDependency)

if(LIBWEBP)
	set(libwebp_cmake_args
		-DBUILD_SHARED_LIBS=OFF
		-DWEBP_BUILD_ANIM_UTILS=OFF
		-DWEBP_BUILD_CWEBP=OFF
		-DWEBP_BUILD_DWEBP=OFF
		-DWEBP_BUILD_GIF2WEBP=OFF
		-DWEBP_BUILD_IMG2WEBP=OFF
		-DWEBP_BUILD_VWEBP=OFF
		-DWEBP_BUILD_WEBPINFO=OFF
		-DWEBP_BUILD_LIBWEBPMUX=ON
		-DWEBP_BUILD_WEBPMUX=OFF
		-DWEBP_BUILD_EXTRAS=OFF
		-DWEBP_BUILD_WEBP_JS=OFF
		-DWEBP_NEAR_LOSSLESS=ON
	)

	# libwebp will happily pass a gcc-style warning parameter to MSVC.
	if(WIN32)
		list(APPEND libwebp_cmake_args -DWEBP_ENABLE_WUNUSED_RESULT=OFF)
	else()
		list(APPEND libwebp_cmake_args -DWEBP_ENABLE_WUNUSED_RESULT=ON)
	endif()

	if(EMSCRIPTEN AND EMSCRIPTEN_THREADS)
		list(APPEND libwebp_cmake_args -DCMAKE_C_FLAGS=-pthread)
	endif()

	build_dependency(libwebp ${LIBWEBP} ${BUILD_TYPE}
		URL https://github.com/webmproject/libwebp/archive/refs/tags/v@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.5.0
			SHA384=430aeec458a5f376efb19c16b636d45a8a6be8b0eb29331296e5cfd2d52b446849b410411d1dbdeea7de0094942d4c11
		ALL_PLATFORMS
			CMAKE
				ALL
					${libwebp_cmake_args}
		PATCHES
			ALL
				patches/libwebp_pc.diff
	)
endif()

if(WIN32 AND LIBVPX)
	# Building libvpx correctly on Windows is immensely difficult. Whatever
	# vcpkg is doing works correctly though, so we just use that instead.
	if(LIBVPX STREQUAL "1.15.2")
		set(libvpx_vcpkg_commit "696237761650114ff0fb12e1a73b90ed1537b6d8")
		set(libvpx_vcpkg_version "1.15.2#0")
	else()
		message(FATAL_ERROR "Unhandled LIBVPX version '${LIBVPX}'")
	endif()

	file(WRITE "vcpkg.json"
		"{\n"
		"    \"builtin-baseline\": \"${libvpx_vcpkg_commit}\",\n"
		"	 \"dependencies\": [\n"
		"        {\"name\": \"libvpx\", \"default-features\": false}\n"
		"    ]\n,"
		"    \"overrides\": [\n"
		"        {\"name\": \"libvpx\", \"version\": \"${libvpx_vcpkg_version}\"}\n"
		"    ]\n"
		"}\n"
	)

	if(TARGET_ARCH STREQUAL "x86")
		set(libvpx_arch "x86")
	elseif(TARGET_ARCH STREQUAL "x86_64")
		set(libvpx_arch "x64")
	else()
		message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
	endif()

	set(libvpx_triplet "${libvpx_arch}-windows-drawpile")

	file(MAKE_DIRECTORY "triplets")
	file(WRITE "triplets/${libvpx_triplet}.cmake"
		"set(VCPKG_TARGET_ARCHITECTURE ${libvpx_arch})\n"
		"set(VCPKG_CRT_LINKAGE dynamic)\n"
		"set(VCPKG_LIBRARY_LINKAGE static)\n"
		"set(VCPKG_BUILD_TYPE release)\n"
	)
	file(WRITE "vcpkg-configuration.json"
		"{\"overlay-triplets\": [\"triplets\"]}\n"
	)

	execute_process(
		COMMAND
			vcpkg --disable-metrics --triplet "${libvpx_triplet}" install
		COMMAND_ECHO STDOUT
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(INSTALL "vcpkg_installed/${libvpx_triplet}/"
		DESTINATION "${CMAKE_INSTALL_PREFIX}"
	)

	file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" libvpx_prefix)
	file(WRITE "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/vpx.pc"
		"prefix=${libvpx_prefix}\n"
		"exec_prefix=\${prefix}\n"
		"libdir=\${prefix}/lib\n"
		"includedir=\${prefix}/include\n"
		"\n"
		"Name: vpx\n"
		"Description: WebM Project VPx codec implementation\n"
		"Version: ${LIBVPX}\n"
		"Conflicts:\n"
		"Libs: -L\${libdir} -lvpx\n"
		"Requires:\n"
		# The -MD is bogus here, but ffmpeg fails to configure without it.
		# We strip it back out later when fixing up the generated pc files.
		"Cflags: -MD -I\${includedir}\n"
	)

	file(REMOVE_RECURSE "vcpkg_installed")
elseif(NOT EMSCRIPTEN AND LIBVPX)
	set(libvpx_configure_args
		--disable-debug
		--disable-debug-libs
		--disable-dependency-tracking
		--disable-docs
		--disable-examples
		--disable-tools
		--disable-unit-tests
		--disable-vp8-decoder
		--disable-vp9-decoder
		--enable-pic
		--enable-vp8-encoder
		--enable-vp9-encoder
	)

	if(APPLE)
		if(TARGET_ARCH STREQUAL "arm64")
			list(PREPEND libvpx_configure_args --target=arm64-darwin20-gcc)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND libvpx_configure_args --target=x86_64-darwin20-gcc)
		else()
			message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
		endif()
	elseif(ANDROID)
		if(TARGET_ARCH STREQUAL "arm32")
			# Need to disable neon asm here, otherwise it dies with a linker
			# error somewhere complaining about being unable to resolve 'main'.
			list(PREPEND libvpx_configure_args
				--target=armv7-android-gcc --disable-neon-asm)
		elseif(TARGET_ARCH STREQUAL "arm64")
			list(PREPEND libvpx_configure_args --target=arm64-android-gcc)
		elseif(TARGET_ARCH STREQUAL "x86")
			# Need to use yasm as the assembler for x86 and x86_64, otherwise
			# invalid flags get passed to clang and the compilation fails.
			list(PREPEND libvpx_configure_args
				--target=x86-android-gcc --as=yasm)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND libvpx_configure_args
				--target=x86_64-android-gcc --as=yasm)
		else()
			message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
		endif()
	elseif(UNIX)
		if(TARGET_ARCH STREQUAL "x86")
			list(PREPEND libvpx_configure_args --target=x86-linux-gcc)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND libvpx_configure_args --target=x86_64-linux-gcc)
		else()
			message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
		endif()
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()

	build_dependency(libvpx ${LIBVPX} ${BUILD_TYPE}
		URL https://github.com/webmproject/libvpx/archive/refs/tags/v@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			1.15.0
			SHA384=cd8aac7124b17379120e58a38465ea4ce52f2d18950cf5f53c81e57f53b0b392d5bc281499deef6122e9c12983b327b4
			1.15.2
			SHA384=12ebfaf8c4c2f2e62ab1bb34bd059d853dd4c48edf8e65a01c3f558ef6f7b9bf13d365b2f32e3d5df730bd4d83eba24f
		ALL_PLATFORMS
			AUTOMAKE
				ASSIGN_PREFIX BROKEN_INSTALL
				QUIRKS libvpx
				ALL
					${libvpx_configure_args}
		PATCHES
			ALL
				patches/libvpx_configure.diff
	)
endif()

if(NOT EMSCRIPTEN AND FFMPEG)
	set(ffmpeg_configure_args
		# This referes to warnings in configure, not build warnings. Warnings
		# include pretty important stuff like not including a requested encoder,
		# so we want this to fail in that case so we actually notice instead of
		# being left with an incomplete build.
		--fatal-warnings
		--enable-gpl
		--enable-version3
		--disable-doc
		--disable-autodetect
		--disable-programs
		--disable-avdevice
		--disable-postproc
		--disable-alsa
		--disable-appkit
		--disable-avfoundation
		--disable-bzlib
		--disable-coreimage
		--disable-iconv
		--disable-lzma
		--disable-metal
		--disable-sndio
		--disable-schannel
		--disable-sdl2
		--disable-securetransport
		--disable-xlib
		--disable-zlib
		--enable-libvpx
		--enable-libwebp
		--disable-encoders
		--disable-decoders
		--disable-muxers
		--disable-demuxers
		--disable-parsers
		--disable-bsfs
		--disable-protocols
		--disable-indevs
		--disable-outdevs
		--disable-devices
		--disable-filters
		--enable-encoder=gif
		--enable-encoder=libvpx_vp8
		--enable-encoder=libvpx_vp9
		--enable-encoder=libwebp
		--enable-encoder=libwebp_anim
		--enable-encoder=rawvideo
		--enable-muxer=gif
		--enable-muxer=mp4
		--enable-muxer=rawvideo
		--enable-muxer=webm
		--enable-muxer=webp
		--enable-filter=palettegen
		--enable-filter=paletteuse
	)

	if(ANDROID)
		if(TARGET_ARCH STREQUAL "arm32")
			list(PREPEND ffmpeg_configure_args
				--arch=armv7-a --cpu=armv7-a --disable-neon)
		elseif(TARGET_ARCH STREQUAL "arm64")
			list(PREPEND ffmpeg_configure_args
				--arch=aarch64 --cpu=armv8-a --enable-neon)
		elseif(TARGET_ARCH STREQUAL "x86")
			# ffmpeg produces invalid assembly when building a static library
			# for Android x86 and x86_64, so we disable assembly on these ABIs.
			list(PREPEND ffmpeg_configure_args --arch=x86_32 --disable-asm)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND ffmpeg_configure_args --arch=x86_64 --disable-asm)
		else()
			message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
		endif()
		list(PREPEND ffmpeg_configure_args
			--enable-cross-compile
			--target-os=android
			--enable-asm
			--enable-inline-asm
		)
	elseif(WIN32)
		if(TARGET_ARCH STREQUAL "x86")
			list(PREPEND ffmpeg_configure_args --arch=x86_32)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND ffmpeg_configure_args --arch=x86_64)
		else()
			message(FATAL_ERROR "Unhandled TARGET_ARCH '${TARGET_ARCH}'")
		endif()
		list(PREPEND ffmpeg_configure_args --toolchain=msvc)
	endif()

	build_dependency(ffmpeg ${FFMPEG} ${BUILD_TYPE}
		URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		SOURCE_DIR "FFmpeg-n@version@"
		VERSIONS
			7.1
			SHA384=6c9c4971415ab500cd336a349c38231b596614a6bdff3051663954e4a3d7fb6f51547de296686efa3fa1be8f8251f1db
			7.1.1
			SHA384=f7307cf7fe789a3def5b9e7dce33c6386d3101b0e904d61525e8b4d906f521c6b2f2f326304e2c22a6b134b6e9500b86
		ALL_PLATFORMS
			AUTOMAKE
				ASSIGN_PREFIX
				QUIRKS ffmpeg
				PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
				ALL ${ffmpeg_configure_args}
		PATCHES
			ALL
				patches/ffmpeg_configure.diff
	)

	# The pkg-config files generated on Windows and Android contain garbage
	# that we need to fix.
	if(ANDROID)
		set(fix_pkg_config_files_system android)
	elseif(WIN32)
		set(fix_pkg_config_files_system win32)
	else()
		unset(fix_pkg_config_files_system)
	endif()

	if(fix_pkg_config_files_system)
		execute_process(
			COMMAND
				perl
				"${CMAKE_CURRENT_LIST_DIR}/fix-ffmpeg-pkg-config-files.pl"
				"${fix_pkg_config_files_system}"
				"${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
			COMMAND_ECHO STDOUT
			COMMAND_ERROR_IS_FATAL ANY
		)
	endif()
endif()
