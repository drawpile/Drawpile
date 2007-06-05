# src/unittest/SHA1/

link_libraries ( libdrawpile )

add_executable (
	test_SHA1_01_Hex
	SHA1_01_Hex.cpp
)

add_executable (
	test_SHA1_02_Hex
	SHA1_02_Hex.cpp
)

add_executable (
	test_SHA1_03_Hex
	SHA1_03_Hex.cpp
)

add_executable (
	test_SHA1_04_Hex
	SHA1_04_Hex.cpp
)


add_test ( "SHA1_01/Hex" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_01_Hex )
add_test ( "SHA1_02/Hex" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_02_Hex )
add_test ( "SHA1_03/Hex" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_03_Hex )
add_test ( "SHA1_04/Hex" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_04_Hex )
