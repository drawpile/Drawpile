# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(LIBX264 "31e19f92f00c7003fa115047ce50978bc98c3a0d" CACHE STRING
	"The commit of libx264 to build")
set(LIBVPX "1.15.0" CACHE STRING
	"The version of libvpx to build")
set(LIBWEBP "1.5.0" CACHE STRING
	"The version of libwebp to build")
set(FFMPEG "7.1" CACHE STRING
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

if(NOT EMSCRIPTEN AND LIBVPX)
	set(libvpx_configure_args
		--disable-debug
		--disable-debug-libs
		--disable-dependency-tracking
		--disable-docs
		--disable-examples
		--disable-tools
		--disable-unit-tests
		--disable-vp8-decoder
		--disable-vp9
		--enable-pic
		--enable-vp8-encoder
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
	elseif(WIN32)
		if(TARGET_ARCH STREQUAL "x86")
			list(PREPEND libvpx_configure_args --target=x86-win32-vs17)
			set(libvpx_arch_dir Win32)
		elseif(TARGET_ARCH STREQUAL "x86_64")
			list(PREPEND libvpx_configure_args --target=x86_64-win64-vs17)
			set(libvpx_arch_dir x64)
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

	# libvpx doesn't generate a pkg-config file on Windows and puts the library
	# into a nested directory that ffmpeg can't deal with.
	if(WIN32)
		file(RENAME
			"${CMAKE_INSTALL_PREFIX}/lib/${libvpx_arch_dir}/vpxmd.lib"
			"${CMAKE_INSTALL_PREFIX}/lib/vpxmd.lib"
		)
		file(REMOVE "${CMAKE_INSTALL_PREFIX}/lib/${libvpx_arch_dir}")
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
			"Requires:\n"
			"Conflicts:\n"
			"Libs: -L\${libdir} -lvpxmd\n"
			"Libs.private:\n"
			# The -MD is bogus here, but ffmpeg fails to configure without it.
			# We strip it back out later when fixing up the generated pc files.
			"Cflags: -MD -I\${includedir}\n"
		)
	endif()
endif()

if(NOT EMSCRIPTEN AND LIBX264)
	set(x264_configure_args
		--enable-static
		--enable-pic
		--disable-lavf
		--disable-swscale
		--disable-avs
		--disable-ffms
		--disable-gpac
		--disable-lsmash
		--disable-bashcompletion
		--disable-cli
		--enable-strip
	)

	if(ANDROID)
		if(TARGET_ARCH STREQUAL "arm32" OR TARGET_ARCH STREQUAL "x86")
			# Configure only checks if *linkage* of functions works, but not if
			# they're actually present in any headers. This causes weird
			# compile errors about fseeko and ftello not being present on 32
			# bit Android. The code appears to work fine though, so either this
			# doesn't matter or we don't hit that code. So we just disable the
			# error and carry on.
			list(APPEND x264_configure_args "--extra-cflags=-Wno-error=implicit-function-declaration")
		endif()
		if(TARGET_ARCH STREQUAL "x86" OR TARGET_ARCH STREQUAL "x86_64")
			# Configure fails to check for the nasm version properly somehow,
			# failing even if a more recent version is found than required.
			# We just disable assembly on this ABI then.
			list(APPEND x264_configure_args "--disable-asm")
		endif()
	endif()

	build_dependency(x264 ${LIBX264} ${BUILD_TYPE}
		URL https://code.videolan.org/videolan/x264/-/archive/@version@/x264-@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			31e19f92f00c7003fa115047ce50978bc98c3a0d
			SHA384=bed835fcf11b4befa8341661b996c4f51842dfee6f7f87c9c2e767cebca0b7871a7f59435b4e92d89c2b13a659d1d737
		ALL_PLATFORMS
			AUTOMAKE
				ASSIGN_HOST ASSIGN_PREFIX WIN32_CC_CL
				ALL ${x264_configure_args}
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
		--enable-libx264
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
		--enable-encoder=libwebp
		--enable-encoder=libwebp_anim
		--enable-encoder=libx264
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
