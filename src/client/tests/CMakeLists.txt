find_package(Qt5Test REQUIRED)

set(TEST_PREFIX client)

set(
	TEST_LIBS
	${DPCLIENTLIB}
	Qt5::Test
	)

AddUnitTest(html)
AddUnitTest(retcon)
AddUnitTest(aclfilter)

