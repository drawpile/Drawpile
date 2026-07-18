# SPDX-License-Identifier: MIT

set_property(SOURCE dprectool/main.c PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE drawpile-cmd/main.cpp PROPERTY SKIP_AUTOGEN ON)

add_cargo_library(dprectool_rust dprectool)
add_executable(dprectool dprectool/main.c)
dp_sign_executable(dprectool)
target_link_libraries(
    dprectool PUBLIC dprectool_rust drawdance_client cmake-config)

add_cargo_library(drawpile-cmd_rust drawpilecmd)
add_executable(drawpile-cmd drawpile-cmd/main.cpp)
dp_sign_executable(drawpile-cmd)
target_link_libraries(
    drawpile-cmd PUBLIC Qt${QT_VERSION_MAJOR}::Core drawpile-cmd_rust
    drawdance_client cmake-config)

add_cargo_library(drawpile-timelapse_rust drawpiletimelapse)
add_executable(
    drawpile-timelapse drawpile-timelapse/main.cpp drawpile-timelapse/logo.qrc)
dp_sign_executable(drawpile-timelapse)
target_link_libraries(drawpile-timelapse PUBLIC
    Qt${QT_VERSION_MAJOR}::Core drawpile-timelapse_rust drawdance_client
    cmake-config)

dp_install_executables(TARGETS dprectool drawpile-cmd drawpile-timelapse)

if(TESTS)
    add_test(
        NAME drawpile_cmd_jpeg
        COMMAND
            "${CMAKE_COMMAND}"
            "-DDRAWPILE_CMD=$<TARGET_FILE:drawpile-cmd>"
            "-DINPUT_IMAGE=${PROJECT_SOURCE_DIR}/test/data/logo256x256.png"
            "-DOUTPUT_IMAGE=${CMAKE_CURRENT_BINARY_DIR}/drawpile-cmd-test.jpg"
            -P "${CMAKE_CURRENT_LIST_DIR}/drawpile-cmd/test_jpeg.cmake"
    )
endif()
