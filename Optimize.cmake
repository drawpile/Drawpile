# Optimize.cmake

# http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86_002d64-Options.html
set ( CPU pentium3 )

if ( GENERIC )
	set ( CPU pentium3 ) # our target audience likely don't have older
endif ( GENERIC )

# 0 - no optimization
# 1 - minimal optimization
# 2 - recommended optimization level
# 3 - highest optimization level, may break things.
# s - same as 2, except this optimizes for size as well

set ( OPTIMIZATION 2 )

###   DO NOT TOUCH THE FOLLOWING   ###

set ( ARCH "-march=${CPU}" )
set ( MTUNE "" )

set ( MTUNE_NATIVE "-mtune=native" ) # GCC >=4.x
set ( MTUNE_GENERIC "-mtune=generic") # GCC >=4.x

set ( FASTMATH "-ffast-math" )

set ( PROFILING_FLAGS "-pg" )
set ( DEBUG_FLAGS "-g -Wall" )
set ( OPT "-O${OPTIMIZATION}" )

set ( FOMIT "-fomit-frame-pointer" )

set ( WARNALL "-Wall" )

# a bit questionable optimizations
set ( U_MFPMATH "-mfpmath=sse,387" ) # does not break anything, only requires SSE to be present
set ( U_MALIGN "-malign-double" ) # breaks binary compatibility?
set ( U_SCHED "-fschedule-insns" )
set ( U_NOTRAP "-fno-trapping-math" )
set ( U_UNSAFE "-funsafe-math-optimizations" )

# only speeds up compilation
set ( PIPE "-pipe " )

###   Test them   ###

include ( TestCXXAcceptsFlag )

###   Architecture   ###

check_cxx_accepts_flag ( ${ARCH} ACCEPT_MARCH )
if ( NOT ACCEPT_MARCH )
	set ( ARCH "" )
endif ( NOT ACCEPT_MARCH )

if ( GENERIC )
	check_cxx_accepts_flag ( ${MTUNE_GENERIC} ACCEPT_MTUNE )
	if ( ACCEPT_MTUNE )
		set ( MTUNE ${MTUNE_GENERIC} )
	endif ( ACCEPT_MTUNE )
else ( GENERIC )
	check_cxx_accepts_flag ( ${MTUNE_NATIVE} ACCEPT_MTUNE )
	if ( ACCEPT_MTUNE )
		set ( MTUNE ${MTUNE_NATIVE} )
	endif ( ACCEPT_MTUNE )
endif ( GENERIC )

###   TEST -O#   ###

check_cxx_accepts_flag ( ${OPT} ACCEPT_OPT )
if ( NOT ACCEPT_OPT )
	set ( OPT "" )
endif ( NOT ACCEPT_OPT)

###   TEST -ffast-math   ###

check_cxx_accepts_flag ( ${FASTMATH} FAST_MATH )
if ( NOT FAST_MATH )
	set ( FASTMATH "" )
endif ( NOT FAST_MATH )

###   TEST -fomit-frame-pointer   ###

if ( NOT PROFILE )
	check_cxx_accepts_flag ( ${FOMIT} ACCEPT_FOMIT )
endif ( NOT PROFILE )

if ( NOT ACCEPT_FOMIT )
	set ( FOMIT "" )
endif ( NOT ACCEPT_FOMIT )

###   TEST -pg   ###

if ( PROFILE )
	check_cxx_accepts_flag ( ${PROFILING_FLAGS} ACCEPT_PROFILE )
endif ( PROFILE )

if ( NOT ACCEPT_PROFILE )
	set ( PROFILING_FLAGS "" )
endif ( NOT ACCEPT_PROFILE )

###   TEST -pipe   ###

check_cxx_accepts_flag ( ${PIPE} ACCEPT_PIPE )
if ( NOT ACCEPT_PIPE )
	set ( PIPE "" )
endif ( NOT ACCEPT_PIPE )

###   TEST unsafe math optimizations   ###

if ( UNSAFE_MATH )
	check_cxx_accepts_flag ( ${U_MFPMATH} ACCEPT_MFPMATH )
	check_cxx_accepts_flag ( ${U_MALIGN} ACCEPT_MALIGN )
	check_cxx_accepts_flag ( ${U_SCHED} ACCEPT_SCHED )
	check_cxx_accepts_flag ( ${U_NOTRAP} ACCEPT_NOTRAP )
	check_cxx_accepts_flag ( ${U_UNSAFE} ACCEPT_UNSAFE )
	
	if ( NOT ACCEPT_MFPMATH )
		set ( U_MFPMATH "" )
	endif ( NOT ACCEPT_MFPMATH )
	if ( NOT ACCEPT_MALIGN )
		set ( U_MALIGN "" )
	endif ( NOT ACCEPT_MALIGN )
	if ( NOT ACCEPT_SCHED )
		set ( U_SCHED "" )
	endif ( NOT ACCEPT_SCHED )
	if ( NOT ACCEPT_NOTRAP )
		set ( U_NOTRAP "" )
	endif ( NOT ACCEPT_NOTRAP )
	if ( NOT ACCEPT_UNSAFE )
		set ( U_UNSAFE "" )
	endif ( NOT ACCEPT_UNSAFE )
	
	set ( UNSAFE_MATH_OPT "${U_MFPMATH} ${U_MALIGN} ${U_SCHED} ${U_NOTRAP} ${U_UNSAFE}")
else ( UNSAFE_MATH )
	set ( UNSAFE_MATH_OPT "" )
endif ( UNSAFE_MATH )

###   TEST -Wall   ###

check_cxx_accepts_flag ( ${WARNALL} ACCEPT_WALL )
if ( NOT ACCEPT_WALL )
	set ( WARNALL "" )
endif ( NOT ACCEPT_WALL )

###   Set flags   ###

set ( CMAKE_CXX_FLAGS "${WARNALL} ${PIPE} ${ARCH} ${MTUNE} ${OPT} ${FASTMATH} ${PROFILING_FLAGS} ${UNSAFE_MATH_OPT} ${REGPARM}" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${FOMIT} -DNDEBUG" )
