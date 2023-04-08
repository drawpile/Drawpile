// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/passwordhash.h"
#include "libshared/util/qtcompat.h"

#include <QtTest/QtTest>

class TestPasswordHash final : public QObject
{
	Q_OBJECT
private slots:
	void testPasswordChecking_data()
	{
		QTest::addColumn<QString>("password");
		QTest::addColumn<QByteArray>("hash");
		QTest::addColumn<bool>("match");
		QTest::addColumn<bool>("valid");

		QTest::newRow("blank") << QString() << QByteArray() << true << false;
		QTest::newRow("blank2") << QString() << server::passwordhash::hash("pass") << false << true;
		QTest::newRow("blocked") << "pass" << QByteArray("*") << false << true;
		QTest::newRow("unsupported") << "nosuchthing" << QByteArray("invalid hash") << false << false;
		QTest::newRow("sha1") << "sha1pass" << server::passwordhash::hash("sha1pass", server::passwordhash::SALTED_SHA1) << true << true;
		QTest::newRow("sha1(blocked)") << "sha1pass" << QByteArray("*") + server::passwordhash::hash("sha1pass", server::passwordhash::SALTED_SHA1) << false << true;
		QTest::newRow("sha1(wrong)") << "sha1pass" << server::passwordhash::hash("wrong", server::passwordhash::SALTED_SHA1) << false << true;
		QTest::newRow("plaintext") << "plainpassword" << server::passwordhash::hash("plainpassword", server::passwordhash::PLAINTEXT) << true << true;
		QTest::newRow("plaintext") << "plainpassword" << server::passwordhash::hash("wrong", server::passwordhash::PLAINTEXT) << false << true;
		QTest::newRow("plaintext") << "plainpassword" << QByteArray("plain;plainpassword") << true << true;

#ifdef HAVE_QT_COMPAT_PBKDF2
		QTest::newRow("pbkdf2") << "pbk" << server::passwordhash::hash("pbk", server::passwordhash::PBKDF2) << true << true;
		QTest::newRow("pbkdf2(wrong)") << "pbkx" << server::passwordhash::hash("pbk", server::passwordhash::PBKDF2) << false << true;
#endif

#ifdef HAVE_LIBSODIUM
		QTest::newRow("sodium") << "sodiumpass" << server::passwordhash::hash("sodiumpass", server::passwordhash::SODIUM) << true << true;
		QTest::newRow("sodium(wrong)") << "sodiumpass2" << server::passwordhash::hash("sodiumpass", server::passwordhash::SODIUM) << false << true;
#endif

	}

	void testPasswordChecking()
	{
		QFETCH(QString, password);
		QFETCH(QByteArray, hash);
		QFETCH(bool, match);
		QFETCH(bool, valid);

		QCOMPARE(server::passwordhash::check(password, hash), match);
		QCOMPARE(server::passwordhash::isValidHash(hash), valid);
	}
};


QTEST_MAIN(TestPasswordHash)
#include "passwordhash.moc"

