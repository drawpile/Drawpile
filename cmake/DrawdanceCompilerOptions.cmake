# SPDX-License-Identifier: MIT

if(MSVC)
    add_compile_options(
        # Source code is in UTF-8, obviously.
        /utf-8
        # Highest warning level.
        /W4
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
        # C4310: cast truncates constant value
        #  - ridiculous warning in the totally reasonable case of a negated bit
        #    mask being cast to the correct type
        /wd4310
		# C4324: structure was padded due to alignment specifier
		#  - well yeah that's what an alignment specifier does
		/wd4324
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
	if(UNIX AND NOT APPLE)
		add_compile_definitions(_XOPEN_SOURCE=600)
	endif()
endif()
