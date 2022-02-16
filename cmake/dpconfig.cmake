if(BUILD_WITH_GL_CHECKS)
    set(DP_GL_CHECKS ON)
else()
    set(DP_GL_CHECKS OFF)
endif()

message(STATUS "DP_GL_CHECKS is ${DP_GL_CHECKS}")

set(dpconfig_h "${CMAKE_BINARY_DIR}/config/dpconfig.h")
configure_file(cmake/dpconfig.tpl.h "${dpconfig_h}")
