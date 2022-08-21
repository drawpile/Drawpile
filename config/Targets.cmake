function(set_dp_target_properties target)
    target_compile_definitions("${target}" PRIVATE "HAVE_CONFIG_H")
    target_compile_options("${target} PRIVATE -pedantic -Wall -Wextra")
    if(APPLE)
        target_compile_options("${target}" PRIVATE "-mmacosx-version-min=10.10")
    endif()
    set_target_properties("${target}" PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endfunction()
