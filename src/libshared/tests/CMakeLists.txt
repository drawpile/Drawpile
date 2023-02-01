set(TEST_PREFIX shared)
set(TEST_LIBS dpshared DP::QtTest)

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
