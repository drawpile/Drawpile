# src/unittest/SHA1/01_Hex/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_01_Hex
	SHA1_01_Hex.cpp
)

add_test ( "SHA1_01/Hex" ${EXECUTABLE_OUTPUT_PATH}/unittest_SHA1_01_Hex )
