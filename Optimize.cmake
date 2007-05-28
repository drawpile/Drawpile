# Optimize.cmake

# only used if -march=native doesn't work
# i383, i686, pentium2, pentium3, athlon-xp, etc.
set ( CPU pentium3 )

set ( GENERIC_CPU i686 )

# 0 - no optimization
# 1 - minimal optimization
# 2 - recommended optimization level
# 3 - highest optimization level, may break things.
# s - same as 2, except this optimizes for size as well

if ( DEBUG )
	set ( OPTIMIZATION 2 ) # anything greater may cause problems with debugging
else ( DEBUG )
	set ( OPTIMIZATION 3 )
endif ( DEBUG )

###   DO NOT TOUCH THE FOLLOWING   ###

set ( ARCH "" )

set ( FASTMATH "-ffast-math" )

set ( PROFILING_FLAGS "-pg" )
set ( DEBUG_FLAGS "-g -Wall" )
set ( OPT "-O${OPTIMIZATION}")

set ( FOMIT "-fomit-frame-pointer")

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

if ( NOT NOARCH ) # optimized for specific arch
	set ( ARCHNATIVE "-march=native" )
	check_cxx_accepts_flag ( ${ARCHNATIVE} COMPILE_MARCH_NATIVE )
	if ( COMPILE_MARCH_NATIVE )
		set ( ARCH ${ARCHSPECIFIC} )
	else ( COMPILE_MARCH_NATIVE )
		set ( ARCHSPECIFIC "-march=${CPU}" )
		check_cxx_accepts_flag ( ${ARCHSPECIFIC} COMPILE_MARCH_SPECIFIC )
		if ( COMPILE_MARCH_SPECIFIC )
			set ( ARCH ${ARCHSPECIFIC} )
		endif ( COMPILE_MARCH_SPECIFIC )
	endif ( COMPILE_MARCH_NATIVE )
else ( NOT NOARCH ) # for generic archs
	set ( ARCHGENERIC "-march=generic")
	check_cxx_accepts_flag ( ${ARCHGENERIC} COMPILE_MARCH_GENERIC )
	if ( COMPILE_MARCH_GENERIC )
		set ( ARCH ${ARCHGENERIC} )
	else ( COMPILE_MARCH_GENERIC )
		set ( ARCHSPECIFIC "-march=${GENERIC_CPU}" )
		check_cxx_accepts_flag ( ${ARCHSPECIFIC} COMPILE_MARCH_SPECIFIC )
		if ( COMPILE_MARCH_SPECIFIC )
			set ( ARCH ${ARCHSPECIFIC} )
		endif ( COMPILE_MARCH_SPECIFIC )
	endif ( COMPILE_MARCH_GENERIC )
endif ( NOT NOARCH )

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

set ( CMAKE_CXX_FLAGS "${WARNALL} ${PIPE} ${ARCH} ${OPT} ${FASTMATH} ${PROFILING_FLAGS} ${UNSAFE_MATH_OPT} ${REGPARM}" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${FOMIT} -DNDEBUG" )
