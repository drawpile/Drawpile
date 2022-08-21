set(TEST_PREFIX client)
set(TEST_LIBS dpclient dpserver Qt5::Test)
qt5_add_resources(TestResources resources.qrc)

AddUnitTest(html)
AddUnitTest(listingfiltering)
AddUnitTest(newversion)
