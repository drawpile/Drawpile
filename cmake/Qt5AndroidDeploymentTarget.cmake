#[[ This module sets the variables required to generate a Qt5 Android build. #]]

# The defaults in Qt5 are outdated, so make them match the current NDK
set(ANDROID_MIN_SDK_VERSION ${NDK_MIN_PLATFORM_LEVEL})
set(ANDROID_TARGET_SDK_VERSION ${NDK_MAX_PLATFORM_LEVEL})

# If ANDROID_SDK is not defined Qt will default it to being one level up from
# NDK, but actually it is two levels up by default due to versioning and this
# could change in the future so just do a tree walk to find it
if(NOT DEFINED ANDROID_SDK)
	set(ANDROID_SDK "${CMAKE_ANDROID_NDK}")
	while(ANDROID_SDK)
		# androiddeployqt is looking for platforms in the SDK directory
		if(IS_DIRECTORY "${ANDROID_SDK}/platforms")
			break()
		endif()
		get_filename_component(ANDROID_SDK "${ANDROID_SDK}" DIRECTORY)
	endwhile()
	if(NOT ANDROID_SDK)
		message(FATAL_ERROR "Could not find Android SDK"
			" starting from ${CMAKE_ANDROID_NDK}; make sure you have at least"
			" one platform version installed"
		)
	endif()
	message(STATUS "Auto-detected ANDROID_SDK at '${ANDROID_SDK}'")
endif()

set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_BINARY_DIR}/src/desktop/android")
