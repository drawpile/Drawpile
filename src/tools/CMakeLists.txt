# SPDX-License-Identifier: MIT

set_property(SOURCE dprectool/main.c PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE drawpile-cmd/main.c PROPERTY SKIP_AUTOGEN ON)

add_cargo_library(dprectool_rust dprectool)
add_executable(dprectool dprectool/main.c)
target_link_libraries(dprectool PUBLIC dprectool_rust drawdance cmake-config)

add_cargo_library(drawpile-cmd_rust drawpilecmd)
add_executable(drawpile-cmd drawpile-cmd/main.c)
target_link_libraries(
    drawpile-cmd PUBLIC drawpile-cmd_rust drawdance cmake-config)

add_cargo_library(drawpile-timelapse_rust drawpiletimelapse)
add_executable(
    drawpile-timelapse drawpile-timelapse/main.cpp drawpile-timelapse/logo.qrc)
target_link_libraries(drawpile-timelapse PUBLIC
    Qt${QT_VERSION_MAJOR}::Core drawpile-timelapse_rust drawdance cmake-config)

install(TARGETS dprectool drawpile-cmd drawpile-timelapse)
