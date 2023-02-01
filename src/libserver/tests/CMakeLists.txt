set(TEST_PREFIX shared)
set(TEST_LIBS dpserver DP::QtTest)

AddUnitTest(filedhistory)
AddUnitTest(sessionban)
AddUnitTest(idqueue)
AddUnitTest(serverlog)
