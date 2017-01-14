find_package(Qt5Test REQUIRED)
#find_package(Qt5Network REQUIRED)
#find_package(KF5Archive REQUIRED NO_MODULE)

set(TEST_PREFIX shared)

set(
	TEST_LIBS
	${DPSHAREDLIB}
	Qt5::Test
	#Qt5::Network
	#KF5::Archive
	)

AddUnitTest(passwordhash)
AddUnitTest(filename)
AddUnitTest(messages)
AddUnitTest(recording)
AddUnitTest(filedhistory)
AddUnitTest(sessionban)

