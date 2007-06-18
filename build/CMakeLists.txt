# cmake

message ( STATUS ${CMAKE_CURRENT_BINARY_DIR} )

set ( FOUND_CPU "(unknown)" )

# Test for AMD CPU
try_run (
	HAVE_AMD_CPU
	COMPILE_AMD_CPU
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_amd.c
	COMPILE_DEFINITIONS -O0
)

if ( HAVE_AMD_CPU )
	set ( FOUND_CPU "AMD" )
endif ( HAVE_AMD_CPU )

# Test for Intel CPU
try_run (
	HAVE_INTEL_CPU
	COMPILE_INTEL_CPU
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_intel.c
	COMPILE_DEFINITIONS -O0
)

if ( HAVE_INTEL_CPU )
	set ( FOUND_CPU "Intel" )
endif ( HAVE_INTEL_CPU )

message ( STATUS "CPU Vendor: ${FOUND_CPU}" )

set ( INSTRSETS "" )

# Test for MMX instruction set
try_run (
	HAVE_MMX
	COMPILE_MMX
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src/have_mmx.c
	COMPILE_DEFINITIONS -O0
)

if ( HAVE_MMX )
	set ( INSTRSETS ${INSTRSETS} "MMX" )
endif ( HAVE_MMX )

# Test for SSE instruction set
try_run (
	HAVE_SSE
	COMPILE_SSE
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src/have_sse.c
	COMPILE_DEFINITIONS -O0
)

if ( HAVE_SSE )
	set ( INSTRSETS ${INSTRSETS} "SSE" )
endif ( HAVE_SSE )

message ( STATUS "Instruction sets found: ${INSTRSETS}" )

configure_file ( ../config.h.cmake ${CMAKE_SOURCE_DIR}/src/config.h )
