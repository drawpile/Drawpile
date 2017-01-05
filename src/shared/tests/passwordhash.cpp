#include "../util/passwordhash.h"

#include <QtTest/QtTest>

class TestPasswordHash: public QObject
{
	Q_OBJECT
private slots:
	void testPasswordChecking_data()
	{
		QTest::addColumn<QString>("password");
		QTest::addColumn<QByteArray>("hash");
		QTest::addColumn<bool>("match");

		QTest::newRow("blank") << QString() << QByteArray() << true;
		QTest::newRow("blank2") << QString() << server::passwordhash::hash("pass") << false;
		QTest::newRow("unsupported") << "nosuchthing" << QByteArray("invalid hash") << false;
		QTest::newRow("sha1") << "sha1pass" << server::passwordhash::hash("sha1pass", server::passwordhash::SALTED_SHA1) << true;
	}

	void testPasswordChecking()
	{
		QFETCH(QString, password);
		QFETCH(QByteArray, hash);
		QFETCH(bool, match);

		QCOMPARE(server::passwordhash::check(password, hash), match);
	}
};


QTEST_MAIN(TestPasswordHash)
#include "passwordhash.moc"

