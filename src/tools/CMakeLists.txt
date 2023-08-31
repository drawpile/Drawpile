# SPDX-License-Identifier: MIT

set_property(SOURCE dprectool/main.c PROPERTY SKIP_AUTOGEN ON)

add_cargo_library(dprectool_rust dprectool)
add_executable(dprectool dprectool/main.c)
target_link_libraries(dprectool PUBLIC dprectool_rust dpengine cmake-config)

install(TARGETS dprectool)
