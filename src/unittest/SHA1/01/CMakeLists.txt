# src/unittest/SHA1/01/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_01
	01.cpp
)

add_test ( "SHA1/01" unittest_SHA1_01 )
