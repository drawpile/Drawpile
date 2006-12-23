# Optimize.cmake

# i386
# i686 - MMX
# pentium2 - MMX, SSE
# athlon - MMX, 3DNow
# athlon-xp - MMX, SSE, 3DNow!, Ext 3DNow!

set ( CPU pentium2 )

# 0 - no optimization
# 1 - minimal optimization
# 2 - recommended optimization level
# 3 - highest optimization level, may break.
# s - optimize for small size (both executable and memory-wise)

set ( OPTIMIZATION 2 )

### DO NOT TOUCH THE FOLLOWING ###

### Set args ###

if ( NOT NOARCH )
	set ( ARCH "-march=${CPU}" )
endif ( NOT NOARCH )

set ( DEBUG_FLAGS "-g -Wall" )
set ( OPT "-O${OPTIMIZATION}")

### Test them ###

include ( TestCXXAcceptsFlag )

check_Cxx_accepts_flag (  ${ARCH} COMPILE_MARCH )

if ( NOT COMPILE_MARCH )
	set ( ARCH "" )
endif ( NOT COMPILE_MARCH )

### Set flags ###

set ( CMAKE_CXX_FLAGS_DEBUG "${ARCH} ${OPT} ${DEBUG_FLAGS}" )
set ( CMAKE_CXX_FLAGS_RELEASE " ${ARCH} ${OPT} -DNDEBUG" )


