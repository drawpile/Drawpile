# src/unittest/SHA1/01/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_01
	SHA1_01.cpp
)

add_test ( "SHA1_01" ${EXECUTABLE_OUTPUT_PATH}/unittest_SHA1_01 )
