# Optimize.cmake

# http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86_002d64-Options.html
set ( CPU pentium3 )
set ( OPT "-O2" )

###   DO NOT TOUCH THE FOLLOWING   ###

if ( GENERIC )
	set ( CPU pentium3 ) # our target audience likely don't have older
endif ( GENERIC )

set ( ARCH "-march=${CPU}" )

if ( GENERIC )
	set ( MTUNE "-mtune=generic") # GCC >=4.x
else ( GENERIC )
	set ( MTUNE "-mtune=native" ) # GCC >=4.x
	set ( ARCH "-march=native" ) # GCC >=4.x
endif ( GENERIC )

set ( FASTMATH "-ffast-math" )

set ( DEBUG_FLAGS "-g" )

set ( WARNALL "-Wall" )
set ( WEFFCPP "-Weffc++" ) # unused

set ( NORTTI "-fno-rtti" ) # RTTI not needed

# a bit questionable optimizations
set ( UNSAFE_MATH_OPT "-funsafe-math-optimizations" )

# only speeds up compilation
set ( PIPE "-pipe" )

###   Architecture   ###
include ( config/Macros.cmake )

TestCompilerFlag ( ARCH ARCHAC )
TestCompilerFlag ( MTUNE MTUNEAC )
TestCompilerFlag ( PIPE PIPEAC )
TestCompilerFlag ( FASTMATH FASTMATHAC )
TestCompilerFlag ( NORTTI NORTTIAC )
TestCompilerFlag ( WARNALL WARNALLAC )
#TestCompilerFlag ( WEFFCPP WEFFCPPC )
TestCompilerFlag ( OPT OPTAC )

### Test profiling arcs if enabled ###
if ( PROFILE )
	set ( PROG_GEN "-profile-generate" )
	set ( PROF_PG "-pg" )
	set ( PROF_INSTR "-finstrument-functions" )
	TestCompilerFlag ( PROF_PG PROF_PG_AC )
	TestCompilerFlag ( PROG_GEN PROG_GEN_AC )
	TestCompilerFlag ( PROF_INSTR PROF_INSTR_AC )
	set ( PROFILING_FLAGS "${PROF_PG} ${PROG_GEN} ${PROF_INSTR}" )
else ( PROFILE )
	set ( PROFILING_FLAGS "" )
endif ( PROFILE )

###   TEST unsafe math optimizations   ###
if ( UNSAFEOPT )
	TestCompilerFlag ( UNSAFE_MATH_OPT UNSAFE_MATH_OPTAC )
else ( UNSAFEOPT )
	set ( UNSAFE_MATH_OPT "" )
endif ( UNSAFEOPT )

###   Set flags   ###

set ( CMAKE_CXX_FLAGS "${OPT} ${WARNALL} ${PIPE} ${ARCH} ${MTUNE} ${FASTMATH} ${PROFILING_FLAGS} ${UNSAFE_MATH_OPT} ${NORTTI}" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DNDEBUG" )
if ( NOT GENERIC )
	if ( USEENV )
		set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} $ENV{CXXFLAGS}" )
	endif ( USEENV )
endif ( NOT GENERIC )
