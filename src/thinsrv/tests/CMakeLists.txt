set(TEST_PREFIX server)
set(TEST_LIBS "${SRVNAME}lib" Qt5::Test)
qt5_add_resources(TestResources resources.qrc)

AddUnitTest(serverconfig)
AddUnitTest(templates)
AddUnitTest(dblog)
