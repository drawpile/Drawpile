# SPDX-License-Identifier: MIT

if(MSVC)
    add_compile_options(
        # C4100: unreferenced formal parameter
        #  - MSVC has no `__attribute__((unused))` and omitting parameter names
        #    wouldn't be standard C
        $<$<COMPILE_LANGUAGE:C>:/wd4100>
        # C4127: conditional expression is constant
        #  - macros from uthash emit `do {} while(0);` and this triggers on
        #    `while(0)`
        /wd4127
        # C4200: nonstandard extension used: zero-sized array in struct/union
        #  - MSVC does not recognise C99 flexible array members as standard
        /wd4200
        # C4210: nonstandard extension used: function given file scope
        #  - MSVC does not recognize extern inside of functions as standard
        /wd4210
        # C4701 and 4703: potentially ininitialized (pointer) variable used
        #  - detection is so primitive that it spams innumerable false positives
        /wd4701 /wd4703
        # C4702: unreachable code
        #  - macros from uthash causing problems again
        /wd4702
    )
else()
    add_compile_options(
        -Wmissing-include-dirs -Wconversion
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    )
endif()
