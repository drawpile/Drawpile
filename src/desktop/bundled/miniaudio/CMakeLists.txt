# SPDX-License-Identifier: MIT
add_library(miniaudio OBJECT)
set(miniaudio_sources miniaudio.c miniaudio.h)
target_sources(miniaudio PRIVATE ${miniaudio_sources})
target_include_directories(miniaudio SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_disable_all_warnings(miniaudio)

target_compile_definitions(miniaudio PUBLIC
    MA_NO_ENCODING=1
    MA_NO_FLAC=1
    MA_NO_MP3=1
    MA_NO_GENERATION=1
)

# Some platforms need extra flags.
if(APPLE)
    target_link_options(miniaudio PUBLIC -lpthread -lm)
    if(IOS)
        target_compile_definitions(miniaudio PUBLIC
            MA_NO_RUNTIME_LINKING=1
        )
        target_link_libraries(miniaudio PUBLIC
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework AVFoundation"
        )
        set_source_files_properties(${miniaudio_sources} PROPERTIES LANGUAGE OBJC)
    endif()
elseif(UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
    target_link_libraries(miniaudio PUBLIC -latomic -ldl -lpthread -lm)
endif()
