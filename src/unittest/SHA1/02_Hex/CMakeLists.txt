# src/unittest/SHA1/01_Hex/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_02_Hex
	SHA1_02_Hex.cpp
)

add_test ( "SHA1_02/Hex" ${EXECUTABLE_OUTPUT_PATH}/unittest_SHA1_02_Hex )
