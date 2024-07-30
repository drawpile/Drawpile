# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
	${CMAKE_CURRENT_LIST_DIR}/../../cmake
)

set(LIBX264 "31e19f92f00c7003fa115047ce50978bc98c3a0d" CACHE STRING
	"The commit of libx264 to build")
set(FFMPEG "7.0.1" CACHE STRING
	"The version of ffmpeg to build")
option(KEEP_ARCHIVES "Keep downloaded archives instead of deleting them" OFF)
option(KEEP_SOURCE_DIRS "Keep source directories instead of deleting them" OFF)
option(KEEP_BINARY_DIRS "Keep build directories instead of deleting them" OFF)
set(TARGET_ARCH "x86_64" CACHE STRING
	"Target architecture (x86, x86_64, arm32, arm64)")

include(BuildDependency)

if(LIBX264)
	build_dependency(x264 ${LIBX264} ${BUILD_TYPE}
		URL https://code.videolan.org/videolan/x264/-/archive/@version@/x264-@version@.tar.gz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			31e19f92f00c7003fa115047ce50978bc98c3a0d
			SHA384=bed835fcf11b4befa8341661b996c4f51842dfee6f7f87c9c2e767cebca0b7871a7f59435b4e92d89c2b13a659d1d737
		ALL_PLATFORMS
			AUTOMAKE
				ASSIGN_HOST ASSIGN_PREFIX WIN32_CC_CL
				ALL
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
endif()

if(FFMPEG)
	set(ffmpeg_configure_args
		--enable-gpl
		--enable-version3
		--disable-doc
		--disable-autodetect
		--disable-programs
		--disable-avdevice
		--disable-avfilter
		--disable-swresample
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
		--enable-encoder=libx264
		--enable-muxer=mp4
	)

	if(ANDROID)
		if(TARGET_ARCH STREQUAL "arm32")
			list(PREPEND ffmpeg_configure_args
				--arch=armv7-a --cpu=armv7-a --disable-neon)
		elseif(TARGET_ARCH STREQUAL "arm64")
			list(PREPEND ffmpeg_configure_args
				--arch=aarch64 --cpu=armv8-a --enable-neon)
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
		URL https://ffmpeg.org/releases/ffmpeg-@version@.tar.xz
		TARGET_ARCH "${TARGET_ARCH}"
		VERSIONS
			7.0.1
			SHA384=25650331f409bf7efc09f0d859ce9a1a8e16fe429e4f9b2593743eb68e723b186559739e8b02aac83c6e5c96137fec7e
		ALL_PLATFORMS
			AUTOMAKE
				ASSIGN_PREFIX FFMPEG_QUIRKS
				PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
				ALL ${ffmpeg_configure_args}
		PATCHES
			ALL
				patches/ffmpeg_configure.diff
	)

	# The pkg-config files generated on Windows contain garbage we have to fix.
	if(WIN32)
		execute_process(
			COMMAND
				perl
				"${CMAKE_CURRENT_LIST_DIR}/fix-win32-ffmpeg-pkg-config-files.pl"
				"${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
			COMMAND_ECHO STDOUT
			COMMAND_ERROR_IS_FATAL ANY
		)
	endif()
endif()
