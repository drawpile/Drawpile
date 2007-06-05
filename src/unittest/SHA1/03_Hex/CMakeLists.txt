# src/unittest/SHA1/03_Hex/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_03_Hex
	SHA1_03_Hex.cpp
)

add_test ( "SHA1_03/Hex" ${EXECUTABLE_OUTPUT_PATH}/unittest_SHA1_03_Hex )
