# Optimize.cmake

# http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86_002d64-Options.html
set ( CPU pentium2 )

if ( GENERIC )
	set ( CPU pentium2 ) # our target audience likely don't have older
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

set ( PROFILING_FLAGS2 "-finstrument-functions" )
set ( PROFILING_FLAGS "-pg" )
set ( DEBUG_FLAGS "-g -Wall" )
set ( OPT "-O${OPTIMIZATION}" )

set ( WARNALL "-Wall" )

set ( NORTTI "-fno-rtti" )
set ( GCSELAS "-fgcse-las" )
set ( GCSELAS2 "-fgcse-after-reload" )

set ( PROFARCS "-fprofile-arcs" )
set ( PROFVPT "-fvpt" )

# a bit questionable optimizations
set ( U_MFPMATH "-mfpmath=sse,387" ) # does not break anything, only requires SSE to be present
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

###   TEST -pg   and other profiling flags  ###

if ( PROFILE )
	check_cxx_accepts_flag ( ${PROFILING_FLAGS} ACCEPT_PROFILE )
	check_cxx_accepts_flag ( ${PROFILING_FLAGS2} ACCEPT_PROFILE2 )
	check_cxx_accepts_flag ( ${PROFARCS} ACCEPT_PROFARCS )
endif ( PROFILE )


if ( NOT ACCEPT_PROFILE )
	set ( PROFILING_FLAGS "" )
endif ( NOT ACCEPT_PROFILE )
if ( NOT ACCEPT_PROFILE2 )
	set ( PROFILING_FLAGS2 "" )
endif ( NOT ACCEPT_PROFILE2 )
if ( NOT ACCEPT_PROFARCS )
	set ( PROFARCS "" )
endif ( NOT ACCEPT_PROFARCS )

set ( ALL_PROFILING_FLAGS ${PROFILING_FLAGS} ${PROFILING_FLAGS2} ${PROFARCS} )

###   TEST -pipe   ###

check_cxx_accepts_flag ( ${PIPE} ACCEPT_PIPE )
if ( NOT ACCEPT_PIPE )
	set ( PIPE "" )
endif ( NOT ACCEPT_PIPE )

###   TEST unsafe math optimizations   ###

if ( UNSAFEOPT )
	check_cxx_accepts_flag ( ${U_MFPMATH} ACCEPT_MFPMATH )
	check_cxx_accepts_flag ( ${U_SCHED} ACCEPT_SCHED )
	check_cxx_accepts_flag ( ${U_NOTRAP} ACCEPT_NOTRAP )
	check_cxx_accepts_flag ( ${U_UNSAFE} ACCEPT_UNSAFE )
	
	if ( NOT ACCEPT_MFPMATH )
		set ( U_MFPMATH "" )
	endif ( NOT ACCEPT_MFPMATH )
	if ( NOT ACCEPT_SCHED )
		set ( U_SCHED "" )
	endif ( NOT ACCEPT_SCHED )
	if ( NOT ACCEPT_NOTRAP )
		set ( U_NOTRAP "" )
	endif ( NOT ACCEPT_NOTRAP )
	if ( NOT ACCEPT_UNSAFE )
		set ( U_UNSAFE "" )
	endif ( NOT ACCEPT_UNSAFE )
	
	set ( UNSAFE_MATH_OPT "${U_MFPMATH} ${U_SCHED} ${U_NOTRAP} ${U_UNSAFE}")
else ( UNSAFEOPT )
	set ( UNSAFE_MATH_OPT "" )
endif ( UNSAFEOPT )

###   TEST PIE and PIC   ###

set ( NOPI_FLAGS "" )
if ( NOPI )
	set ( NOPIC "-fno-pic" )
	set ( NOPIE "-fno-pie" )
	
	check_cxx_accepts_flag ( ${NOPIC} ACCEPT_NOPIC )
	if ( NOT ACCEPT_NOPIC )
		set ( NOPIC "" )
	endif (NOT ACCEPT_NOPIC )
	
	check_cxx_accepts_flag ( ${NOPIE} ACCEPT_NOPIE )
	if ( NOT ACCEPT_NOPIE )
		set ( NOPIE "" )
	endif (NOT ACCEPT_NOPIE )
	
	set ( NOPI_FLAGS "${NOPIC} ${NOPIE}" )
endif ( NOPI )

###   TEST STUFF   ###

check_cxx_accepts_flag ( ${GCSELAS} ACCEPT_GCSELAS )
if ( NOT ACCEPT_GCSELAS )
	set ( GCSELAS "" )
endif ( NOT ACCEPT_GCSELAS )

check_cxx_accepts_flag ( ${GCSELAS2} ACCEPT_GCSELAS2 )
if ( NOT ACCEPT_GCSELAS2 )
	set ( GCSELAS2 "" )
endif ( NOT ACCEPT_GCSELAS2 )

set ( MISCOPTS "${GCSELAS} ${GCSELAS2}" )

###   TEST -fno-rtti   ###

check_cxx_accepts_flag ( ${NORTTI} ACCEPT_NORTTI )
if ( NOT ACCEPT_NORTTI )
	set ( NORTTI "" )
endif ( NOT ACCEPT_NORTTI )

###   TEST -Wall   ###

check_cxx_accepts_flag ( ${WARNALL} ACCEPT_WALL )
if ( NOT ACCEPT_WALL )
	set ( WARNALL "" )
endif ( NOT ACCEPT_WALL )

###   Set flags   ###

set ( CMAKE_CXX_FLAGS "${WARNALL} ${PIPE} ${ARCH} ${MTUNE} ${OPT} ${MISCOPTS} ${FASTMATH} ${ALL_PROFILING_FLAGS} ${UNSAFE_MATH_OPT} ${REGPARM} ${NOPI_FLAGS}" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DNDEBUG" )
