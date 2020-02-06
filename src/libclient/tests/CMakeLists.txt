find_package(Qt5Test REQUIRED)

set(TEST_PREFIX client)

qt5_add_resources( TestResources resources.qrc )

set(
	TEST_LIBS
	dpclient
	dpserver
	Qt5::Test
	)

AddUnitTest(html)
AddUnitTest(retcon)
AddUnitTest(aclfilter)
AddUnitTest(listingfiltering)
AddUnitTest(newversion)

