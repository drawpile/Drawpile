find_package(Qt5Test REQUIRED)
find_package(Sodium)
#find_package(Qt5Network REQUIRED)
#find_package(KF5Archive REQUIRED NO_MODULE)

set(TEST_PREFIX shared)

set(
	TEST_LIBS
	dpshared
	Qt5::Test
	)

AddUnitTest(passwordhash)
AddUnitTest(filename)
AddUnitTest(messages)
AddUnitTest(recording)
AddUnitTest(messagequeue)
AddUnitTest(listings)
AddUnitTest(ulid)

if(Sodium_FOUND)
	AddUnitTest(authtoken)
endif()

