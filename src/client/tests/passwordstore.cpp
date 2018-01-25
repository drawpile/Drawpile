#include "../utils/passwordstore.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>

Q_DECLARE_METATYPE(PasswordStore::Type)

class TestPasswordStore: public QObject
{
	Q_OBJECT
private slots:

	void initTestCase()
	{
		ps = PasswordStore(":/test/passwordlist.json");
		QString err;
		bool loaded = ps.load(&err);

		if(!loaded)
			qWarning("Test list parsing error: %s", qPrintable(err));
		QVERIFY(loaded);
	}

	void testJsonParsing_data()
	{
		QTest::addColumn<QString>("server");
		QTest::addColumn<QString>("username");
		QTest::addColumn<PasswordStore::Type>("type");
		QTest::addColumn<QString>("password");

		QTest::newRow("server (found)") << "192.168.1.1" << "bob" << PasswordStore::Type::Server << "abc";
		QTest::newRow("server (not found)") << "192.168.1.1" << "alice" << PasswordStore::Type::Server << QString();
		QTest::newRow("server (found)") << "pub.drawpile.net" << "root" << PasswordStore::Type::Server << "qwerty123";
		QTest::newRow("server (wrong type)") << "pub.drawpile.net" << "root" << PasswordStore::Type::Extauth << QString();

		QTest::newRow("extauth (found)") << "drawpile.net" << "test" << PasswordStore::Type::Extauth << "hunter2";
		QTest::newRow("extauth (found)") << "drawpile.net" << "alt" << PasswordStore::Type::Extauth << "hunter3";
		QTest::newRow("extauth (not found)") << "drawpile.net" << "alt2" << PasswordStore::Type::Extauth << QString();
		QTest::newRow("extauth (case insensitive)") << "drawpile.net" << "TeSt" << PasswordStore::Type::Extauth << "hunter2";
	}

	void testJsonParsing()
	{
		QFETCH(QString, server);
		QFETCH(QString, username);
		QFETCH(PasswordStore::Type, type);
		QFETCH(QString, password);

		QCOMPARE(ps.getPassword(server, username, type), password);
	}

	void testToJsonDocument()
	{
		const QJsonDocument doc = ps.toJsonDocument();

		QFile f(":/test/passwordlist.json");
		QVERIFY(f.open(QFile::ReadOnly));
		const QJsonDocument expected = QJsonDocument::fromJson(f.readAll());

		QCOMPARE(doc, expected);
	}

	void testSaving()
	{
		QTemporaryDir tempdir;
		QVERIFY(tempdir.isValid());

		const QString tempfile = tempdir.filePath("test.json");
		qInfo("Writing test file to: %s", qPrintable(tempfile));

		QString err;
		const bool saved = ps.saveAs(tempfile, &err);
		if(!saved)
			qWarning("Couldn't save: %s", qPrintable(err));
		QVERIFY(saved);

		QFile f(tempfile);
		QVERIFY(f.open(QFile::ReadOnly));
		const QJsonDocument expected = QJsonDocument::fromJson(f.readAll());

		QCOMPARE(ps.toJsonDocument(), expected);
	}

	void testSetting()
	{
		PasswordStore ps2 = ps;
		ps2.setPassword("test", "Hello", PasswordStore::Type::Server, "srv");
		ps2.setPassword("test", "Hello", PasswordStore::Type::Extauth, "world");

		QCOMPARE(ps2.getPassword("test", "hello", PasswordStore::Type::Server), QString("srv"));
		QCOMPARE(ps2.getPassword("test", "hello", PasswordStore::Type::Extauth), QString("world"));
	}

	void testRemoval()
	{
		PasswordStore ps2 = ps;
		QVERIFY(!ps2.forgetPassword("test", "nosuchthing", PasswordStore::Type::Server));
		QVERIFY(!ps2.forgetPassword("pub.drawpile.net", "nosuchthing", PasswordStore::Type::Server));
		QVERIFY(ps2.forgetPassword("pub.drawpile.net", "root", PasswordStore::Type::Server));
		QCOMPARE(ps2.getPassword("pub.drawpile.net", "root", PasswordStore::Type::Server), QString());
	}

private:
	PasswordStore ps;
};


QTEST_MAIN(TestPasswordStore)
#include "passwordstore.moc"

