set(TEST_PREFIX client)
set(TEST_LIBS dpclient dpserver DP::QtTest)
dp_qt_add_resources(TestResources resources.qrc)

AddUnitTest(html)
AddUnitTest(listingfiltering)
AddUnitTest(newversion)
