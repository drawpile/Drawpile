# src/unittest/templates/

link_libraries ( libdrawpile )

add_executable (
	test_byteswap_32b
	byteswap_32b.cpp
)

add_executable (
	test_byteswap_16b
	byteswap_16b.cpp
)

add_test ( "templates/byteswap_16b" ${EXECUTABLE_OUTPUT_PATH}/test_byteswap_16b )
add_test ( "templates/byteswap_32b" ${EXECUTABLE_OUTPUT_PATH}/test_byteswap_32b )
