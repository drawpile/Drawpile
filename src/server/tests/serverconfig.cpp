#include "../../shared/server/inmemoryconfig.h"
#include "../database.h"

#include <QtTest/QtTest>

using server::ServerConfig;
using server::InMemoryConfig;
using server::Database;
using server::ConfigKey;

class TestServerConfig : public QObject
{
	Q_OBJECT
private slots:
	void testStringKey()
	{
		InMemoryConfig cfg;
		const ConfigKey key(0, "test", "default", ConfigKey::STRING);
		const QString value = "Test";

		bool ok = cfg.setConfigString(key, "Test");
		QVERIFY(ok);

		QString val = cfg.getConfigString(key);

		QCOMPARE(val, value);
	}

	void testTimeKey()
	{
		InMemoryConfig cfg;
		const ConfigKey key(0, "test", "0", ConfigKey::TIME);

		bool ok = cfg.setConfigString(key, "1m");
		QVERIFY(ok);

		int val = cfg.getConfigTime(key);
		QCOMPARE(val, 60);

		cfg.setConfigInt(key, 10);
		val = cfg.getConfigTime(key);
		QCOMPARE(val, 10);
	}

	void testTimeParser()
	{
		QVERIFY(ServerConfig::parseTimeString("invalid")<0);
		QCOMPARE(ServerConfig::parseTimeString("30"), 30);
		QCOMPARE(ServerConfig::parseTimeString("60s"), 60);
		QCOMPARE(ServerConfig::parseTimeString("1m"), 60);
		QCOMPARE(ServerConfig::parseTimeString("2 m"), 120);
		QCOMPARE(ServerConfig::parseTimeString("1h"), 60*60);
		QCOMPARE(ServerConfig::parseTimeString("1.5H"), 90*60);
		QCOMPARE(ServerConfig::parseTimeString("1.5  h"), 90*60);
		QCOMPARE(ServerConfig::parseTimeString("2d"), 2*24*60*60);
	}

	void testSizeKey()
	{
		InMemoryConfig cfg;
		const ConfigKey key(0, "test", "0", ConfigKey::SIZE);

		bool ok = cfg.setConfigString(key, "1mb");
		QVERIFY(ok);

		int val = cfg.getConfigSize(key);
		QCOMPARE(val, 1024*1024);

		cfg.setConfigInt(key, 10);
		val = cfg.getConfigSize(key);
		QCOMPARE(val, 10);
	}

	void testSizeParser()
	{
		QVERIFY(ServerConfig::parseSizeString("invalid")<0);
		QCOMPARE(ServerConfig::parseSizeString("10"), 10);
		QCOMPARE(ServerConfig::parseSizeString("100b"), 100);
		QCOMPARE(ServerConfig::parseSizeString("100 b"), 100);
		QCOMPARE(ServerConfig::parseSizeString("1kb"), 1024);
		QCOMPARE(ServerConfig::parseSizeString("1mb"), 1024*1024);
		QCOMPARE(ServerConfig::parseSizeString("1.5   mb"), 1536*1024);
		QCOMPARE(ServerConfig::parseSizeString("1GB"), 1024*1024*1024);
	}

	void testIntKey()
	{
		InMemoryConfig cfg;
		const ConfigKey key(0, "test", "0", ConfigKey::INT);

		bool ok = cfg.setConfigString(key, "123");
		QVERIFY(ok);

		int val = cfg.getConfigInt(key);
		QCOMPARE(val, 123);

		cfg.setConfigInt(key, -10);
		val = cfg.getConfigInt(key);
		QCOMPARE(val, -10);
	}

	void testBoolKey_data()
	{
		QTest::addColumn<QString>("str");
		QTest::addColumn<bool>("expected");

		QTest::newRow("lc true") << "true" << true;
		QTest::newRow("uc true") << "True" << true;
		QTest::newRow("lc false") << "false" << false;
		QTest::newRow("one") << "1" << true;
		QTest::newRow("zero") << "0" << false;
		QTest::newRow("blank") << "" << false;
	}

	void testBoolKey()
	{
		InMemoryConfig cfg;
		const ConfigKey key(0, "test", "0", ConfigKey::BOOL);

		QFETCH(QString, str);
		QFETCH(bool, expected);

		bool ok = cfg.setConfigString(key, str);
		QVERIFY(ok);

		bool val = cfg.getConfigBool(key);
		QCOMPARE(val, expected);

		cfg.setConfigBool(key, val);
		QCOMPARE(cfg.getConfigString(key), QString(val ? "true" : "false"));
	}

	void testDatabase()
	{
		Database db;
		QVERIFY(db.openFile(":memory:"));

		const ConfigKey strKey = ConfigKey(0, "str", "", ConfigKey::STRING);
		const ConfigKey timeKey = ConfigKey(1, "time", "0", ConfigKey::TIME);
		const ConfigKey sizeKey = ConfigKey(2, "size", "0", ConfigKey::SIZE);
		const ConfigKey intKey = ConfigKey(3, "int", "0", ConfigKey::INT);
		const ConfigKey boolKey = ConfigKey(4, "bool", "0", ConfigKey::BOOL);
		QVERIFY(db.setConfigString(strKey, "test"));
		QVERIFY(db.setConfigString(timeKey, "1m"));
		QVERIFY(db.setConfigString(sizeKey, "1kb"));
		QVERIFY(db.setConfigString(intKey, "999"));
		QVERIFY(db.setConfigString(boolKey, "true"));

		QCOMPARE(db.getConfigString(strKey), QString("test"));
		QCOMPARE(db.getConfigTime(timeKey), 60);
		QCOMPARE(db.getConfigSize(sizeKey), 1024);
		QCOMPARE(db.getConfigInt(intKey), 999);
		QCOMPARE(db.getConfigBool(boolKey), true);
	}
};


QTEST_MAIN(TestServerConfig)
#include "serverconfig.moc"

