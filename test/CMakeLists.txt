add_clang_format_files(dptest/dptest.c dptest/dptest.h)

if(BUILD_TESTS)
    add_library(dptest STATIC dptest/dptest.c dptest/dptest.h)
    set_dp_target_properties(dptest NO_EXPORT)
    target_include_directories(dptest INTERFACE
                               "${CMAKE_CURRENT_LIST_DIR}/dptest")
    target_link_libraries(dptest PUBLIC dpcommon)
endif()
