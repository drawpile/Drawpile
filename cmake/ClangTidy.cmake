if(CLANG_TIDY)
    find_program(clang_tidy_exe NAMES "clang-tidy")
    if(clang_tidy_exe)
        message(STATUS "Using clang-tidy at ${clang_tidy_exe}")
    else()
        message(STATUS "Did not find clang-tidy, linting disabled")
    endif()
else()
    message(STATUS "Not using clang-tidy, as requested")
endif()

function(add_clang_tidy target)
    if(clang_tidy_exe)
        message(STATUS "Enabling clang-tidy on ${target}")
        set_target_properties("${target}" PROPERTIES
            C_CLANG_TIDY "${clang_tidy_exe}"
            CXX_CLANG_TIDY "${clang_tidy_exe}")
    endif()
endfunction()
