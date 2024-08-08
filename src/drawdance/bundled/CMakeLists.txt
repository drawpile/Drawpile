# SPDX-License-Identifier: MIT

add_library(parson OBJECT parson/parson.c parson/parson.h)
target_include_directories(
    parson SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/parson)

add_library(qgrayraster OBJECT qgrayraster/qgrayraster.c
                               qgrayraster/qgrayraster_inc.h
                               qgrayraster/qgrayraster_p.h
                               qgrayraster/qrasterdefs_p.h)
target_include_directories(
    qgrayraster SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/qgrayraster)

add_library(mypaint OBJECT libmypaint/brushsettings-gen.h
            libmypaint/config.h libmypaint/fastapprox/cast.h
            libmypaint/fastapprox/fasterf.h libmypaint/fastapprox/fastexp.h
            libmypaint/fastapprox/fastgamma.h
            libmypaint/fastapprox/fasthyperbolic.h
            libmypaint/fastapprox/fastlambertw.h libmypaint/fastapprox/fastlog.h
            libmypaint/fastapprox/fastonebigheader.h
            libmypaint/fastapprox/fastpow.h libmypaint/fastapprox/fastsigmoid.h
            libmypaint/fastapprox/fasttrig.h libmypaint/fastapprox/sse.h
            libmypaint/helpers.c libmypaint/helpers.h libmypaint/mypaint-brush.c
            libmypaint/mypaint-brush.h libmypaint/mypaint-brush-settings.c
            libmypaint/mypaint-brush-settings-gen.h
            libmypaint/mypaint-brush-settings.h libmypaint/mypaint.c
            libmypaint/mypaint-config.h libmypaint/mypaint-glib-compat.h
            libmypaint/mypaint.h libmypaint/mypaint-mapping.c
            libmypaint/mypaint-mapping.h libmypaint/mypaint-rectangle.c
            libmypaint/mypaint-rectangle.h libmypaint/mypaint-surface.c
            libmypaint/mypaint-surface.h libmypaint/rng-double.c
            libmypaint/rng-double.h )
target_include_directories(
    mypaint SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libmypaint)

add_library(uthash INTERFACE)
target_include_directories(uthash SYSTEM INTERFACE "uthash")

if(CLIENT OR TOOLS)
    add_library(jo_gifx OBJECT jo_gifx/jo_gifx.h jo_gifx/jo_gifx.c)
    target_include_directories(
        jo_gifx SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/jo_gifx)

    set(psd_sdk_sources
    psd_sdk/Psd.h
    psd_sdk/PsdAllocator.cpp
    psd_sdk/PsdAllocator.h
    psd_sdk/PsdAssert.h
    psd_sdk/PsdBitUtil.h
    psd_sdk/PsdBitUtil.inl
    psd_sdk/PsdBlendMode.cpp
    psd_sdk/PsdBlendMode.h
    psd_sdk/PsdChannel.h
    psd_sdk/PsdColorMode.cpp
    psd_sdk/PsdColorMode.h
    psd_sdk/PsdCompilerMacros.h
    psd_sdk/PsdCompressionType.h
    psd_sdk/PsdDecompressRle.cpp
    psd_sdk/PsdDecompressRle.h
    psd_sdk/PsdDocument.h
    psd_sdk/PsdEndianConversion.h
    psd_sdk/PsdEndianConversion.inl
    psd_sdk/PsdFile.cpp
    psd_sdk/PsdFile.h
    psd_sdk/PsdFixedSizeString.cpp
    psd_sdk/PsdFixedSizeString.h
    psd_sdk/PsdKey.h
    psd_sdk/PsdLayer.h
    psd_sdk/PsdLayerMask.h
    psd_sdk/PsdLayerMaskSection.h
    psd_sdk/PsdLayerType.h
    psd_sdk/PsdLog.h
    psd_sdk/PsdMallocAllocator.cpp
    psd_sdk/PsdMallocAllocator.h
    psd_sdk/PsdMemoryUtil.h
    psd_sdk/PsdMemoryUtil.inl
    psd_sdk/PsdNamespace.h
    psd_sdk/PsdParseDocument.cpp
    psd_sdk/PsdParseDocument.h
    psd_sdk/PsdParseLayerMaskSection.cpp
    psd_sdk/PsdParseLayerMaskSection.h
    psd_sdk/PsdPch.cpp
    psd_sdk/PsdPch.h
    psd_sdk/PsdPlatform.h
    psd_sdk/PsdSection.h
    psd_sdk/PsdSyncFileReader.cpp
    psd_sdk/PsdSyncFileReader.h
    psd_sdk/PsdSyncFileUtil.h
    psd_sdk/PsdSyncFileUtil.inl
    psd_sdk/PsdTypes.h
    psd_sdk/PsdUnionCast.h
    psd_sdk/PsdUnionCast.inl
    psd_sdk/PsdVectorMask.h
    psd_sdk/Psdinttypes.h
    psd_sdk/Psdispod.h
    psd_sdk/Psdisunsigned.h
    psd_sdk/Psdminiz.c
    psd_sdk/Psdminiz.h
    psd_sdk/Psdstdint.h
    )

    add_library(psd_sdk STATIC ${psd_sdk_sources})
    target_include_directories(
        psd_sdk SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/psd_sdk)
endif()

# The project root adds compiler warnings and tidy to all targets, but
# third-party code does not follow the same code hygiene rules, so turn those
# off for everything in this directory
get_ignore_warnings_in_directory(bundled_ignore_options)
get_directory_property(third_party_targets BUILDSYSTEM_TARGETS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
    set_property(TARGET ${third_party_targets} APPEND PROPERTY
        COMPILE_OPTIONS "${bundled_ignore_options}"
    )
else()
    foreach(target IN LISTS third_party_targets)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "INTERFACE_LIBRARY")
            set(option INTERFACE_COMPILE_OPTIONS)
        else()
            set(option COMPILE_OPTIONS)
        endif()
        set_property(TARGET ${target} APPEND PROPERTY
            ${option} "${bundled_ignore_options}"
        )
    endforeach()
endif()

# It is not possible to use the same approach that was used to disable
# warnings because CMake apparently discards empty list items when a list is
# passed to set_target_properties, so instead we must loop to unset each
# property
foreach(ignore_tidy IN LISTS IGNORE_TIDY_PROPERTY_NAMES)
    set_property(TARGET ${third_party_targets} PROPERTY ${ignore_tidy})
endforeach()
unset(third_party_targets)
