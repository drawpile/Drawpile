find_package(Qt5Test REQUIRED)

set(TEST_PREFIX server)

set(
	TEST_LIBS
	"${SRVNAME}lib"
	Qt5::Test
	)

AddUnitTest(serverconfig)

