find_package(Qt5Test REQUIRED)

set(TEST_PREFIX shared)

set(
	TEST_LIBS
	dpserver
	Qt5::Test
	)

AddUnitTest(filedhistory)
AddUnitTest(sessionban)
AddUnitTest(idqueue)
AddUnitTest(serverlog)

