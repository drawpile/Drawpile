# src/unittest/SHA1/04_Hex/

link_libraries ( libdrawpile )

add_executable (
	unittest_SHA1_04_Hex
	SHA1_04_Hex.cpp
)

add_test ( "SHA1_04/Hex" ${EXECUTABLE_OUTPUT_PATH}/unittest_SHA1_04_Hex )
