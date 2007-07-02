# src/unittest/SHA1/

link_libraries ( libdrawpile )

add_executable (
	test_SHA1_bin_01
	SHA1_bin_01.cpp
)

add_executable (
	test_SHA1_bin_02
	SHA1_bin_02.cpp
)

add_executable (
	test_SHA1_hex_01
	SHA1_hex_01.cpp
)

add_executable (
	test_SHA1_hex_02
	SHA1_hex_02.cpp
)

add_executable (
	test_SHA1_hex_03
	SHA1_hex_03.cpp
)

add_executable (
	test_SHA1_hex_04
	SHA1_hex_04.cpp
)

add_executable (
	test_SHA1_cavs5_monte
	SHA1_cavs5_monte.cpp
)

add_executable (
	test_SHA1_string_01
	SHA1_string_01.cpp
)

add_test ( "SHA1/CAVS5.0/Monte_0" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_cavs5_monte 0 )
add_test ( "SHA1/CAVS5.0/Monte_1" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_cavs5_monte 1 )
add_test ( "SHA1/CAVS5.0/Monte_3" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_cavs5_monte 3 )
add_test ( "SHA1/CAVS5.0/Monte_6" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_cavs5_monte 6 )
add_test ( "SHA1/string/pangram_01" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_string_01 )
add_test ( "SHA1/binary_01" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_bin_01 )
add_test ( "SHA1/binary_02" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_bin_02 )
add_test ( "SHA1/hex_01" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_hex_01 )
add_test ( "SHA1/hex_02" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_hex_02 )
add_test ( "SHA1/hex_03" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_hex_03 )
add_test ( "SHA1/hex_04" ${EXECUTABLE_OUTPUT_PATH}/test_SHA1_hex_04 )
