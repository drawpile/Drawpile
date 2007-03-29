# Optimize.cmake

# [arch] - [instruction sets]

# i386      - none
# i686      - MMX
# pentium2  - MMX, SSE
# athlon    - MMX, 3DNow
# athlon-xp - MMX, SSE, 3DNow!, Ext 3DNow!

set ( CPU pentium2 )
#set ( CPU i686 )

# 0 - no optimization
# 1 - minimal optimization
# 2 - recommended optimization level
# 3 - highest optimization level, may break.
# s - optimize for small size (both executable and memory-wise)

if ( DEBUG )
	set ( OPTIMIZATION 2 ) # anything greater may cause problems with debugging
else ( DEBUG )
	set ( OPTIMIZATION 3 )
endif ( DEBUG )

###   DO NOT TOUCH THE FOLLOWING   ###

###   Set args   ###

if ( NOT NOARCH )
	set ( ARCH "-march=${CPU}" )
endif ( NOT NOARCH )

set ( FASTMATH "-ffast-math" )

set ( PROFILING "-pg" )
set ( DEBUG_FLAGS "-g -Wall" )
set ( OPT "-O${OPTIMIZATION}")

set ( FOMIT "-fomit-frame-pointer")

###   Test them   ###

include ( TestCXXAcceptsFlag )

###   TEST -O#   ###

check_cxx_accepts_flag ( ${OPT} ACCEPT_OPT )

if ( NOT ACCEPT_OPT )
	set ( OPT "" )
endif ( NOT ACCEPT_OPT)

###   TEST -march=CPUNAME   ###

check_cxx_accepts_flag ( ${ARCH} COMPILE_MARCH )

if ( NOT COMPILE_MARCH )
	set ( ARCH "" )
endif ( NOT COMPILE_MARCH )

###   TEST -ffast-math   ###

check_cxx_accepts_flag ( ${FASTMATH} FAST_MATH )

if ( NOT FAST_MATH )
	set ( FASTMATH "" )
endif ( NOT FAST_MATH )

###   TEST -fomit-frame-pointer   ###

check_cxx_accepts_flag ( ${FOMIT} ACCEPT_FOMIT )

if ( NOT ACCEPT_FOMIT )
	set ( FOMIT "" )
endif ( NOT ACCEPT_FOMIT )

###   Set flags   ###

set ( CMAKE_CXX_FLAGS_DEBUG "${ARCH} ${OPT} ${FASTMATH} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${ARCH} ${OPT} ${FASTMATH} ${FOMIT} -DNDEBUG" )
