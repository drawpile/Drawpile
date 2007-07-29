# src/unittest/protocol/

link_libraries ( libdrawpile )

add_executable (
	test_protocol_01
	identifier_01.cpp
)

add_test ( "protocol/identifier" ${EXECUTABLE_OUTPUT_PATH}/test_protocol_01 )
