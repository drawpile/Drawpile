# src/unittest/

set (
	UNITTEST_HDRS
	testing.h
	test.sha1.h
	test.memcpy_t.h
	test.bswap.h
	test.bounds.h
)

set (
	UNITTEST_SRCS
	unittest.cpp
)

link_libraries( ${LIBNAME} )

add_executable ( dptest ${UNITTEST_SRCS} ${UNITTEST_HDRS} )

