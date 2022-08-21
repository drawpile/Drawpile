set(TEST_PREFIX shared)
set(TEST_LIBS dpshared Qt5::Test)

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
