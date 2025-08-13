# SPDX-License-Identifier: MIT
add_library(miniaudio OBJECT)
target_sources(miniaudio PRIVATE miniaudio.c miniaudio.h)
target_include_directories(miniaudio SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_disable_all_warnings(miniaudio)

target_compile_definitions(miniaudio PUBLIC
    MA_NO_ENCODING=1
    MA_NO_FLAC=1
    MA_NO_MP3=1
    MA_NO_GENERATION=1
    MA_NO_WASAPI=1 # WASAPI on Windows causes hangs and crashes.
)

# Some platforms need extra flags.
if(APPLE)
    target_link_options(miniaudio PUBLIC -lpthread -lm)
elseif(UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
    target_link_libraries(miniaudio PUBLIC -latomic -ldl -lpthread -lm)
endif()
