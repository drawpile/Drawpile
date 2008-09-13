# config

message ( STATUS ${CMAKE_CURRENT_BINARY_DIR} )

configure_file ( config.h.cmake ${CMAKE_SOURCE_DIR}/src/config.h )
