set(TEST_PREFIX server)
set(TEST_LIBS "${SRVNAME}lib" DP::QtTest)
dp_qt_add_resources(TestResources resources.qrc)

AddUnitTest(serverconfig)
AddUnitTest(templates)
AddUnitTest(dblog)
