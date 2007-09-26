# Optimize.cmake

# http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86_002d64-Options.html
set ( CPU pentium3 )
set ( OPT "-O2" )

###   DO NOT TOUCH THE FOLLOWING   ###

if ( GENERIC )
	message ( STATUS "Optimization: Generic" )
	set ( ARCH "-march=${CPU}" )
	set ( MTUNE "-mtune=generic") # GCC >=4.x
else ( GENERIC )
	message ( STATUS "Optimization: Native" )
	set ( MTUNE "-mtune=native" ) # GCC >=4.x
	set ( ARCH "-march=native" ) # GCC >=4.x
endif ( GENERIC )

set ( MTHREADS "-mthreads" )

set ( FASTMATH "-ffast-math" )

set ( DEBUG_FLAGS "-g" )

set ( WARN "-Wall" )
set ( WEFFCPP "-Weffc++" ) # unused

set ( NORTTI "-fno-rtti" ) # RTTI not needed

set ( PIPE "-pipe" ) # only speeds up compilation

include ( config/Macros.cmake )

TestCompilerFlag ( WARN WARNAC )
TestCompilerFlag ( ARCH ARCHAC )
TestCompilerFlag ( MTUNE MTUNEAC )
TestCompilerFlag ( PIPE PIPEAC )
TestCompilerFlag ( FASTMATH FASTMATHAC )
TestCompilerFlag ( NORTTI NORTTIAC )
#TestCompilerFlag ( WEFFCPP WEFFCPPC )
TestCompilerFlag ( OPT OPTAC )
TestCompilerFlag ( MTHREADS MTHREADSAC )

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

###   Set flags   ###

set ( CMAKE_CXX_FLAGS "${OPT} ${WARN} ${PIPE} ${ARCH} ${MTUNE} ${FASTMATH} ${PROFILING_FLAGS} ${NORTTI} ${MTHREADS}" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DNDEBUG" )
if ( NOT GENERIC AND USEENV )
	set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} $ENV{CXXFLAGS}" )
endif ( NOT GENERIC AND USEENV )
