// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/news.h"
#include <QtTest/QtTest>

class TestNews final : public QObject {
	Q_OBJECT
private slots:
	void testIsNewerThan_data()
	{
		QTest::addColumn<QString>("version1");
		QTest::addColumn<QString>("version2");
		QTest::addColumn<bool>("expected");

		QTest::newRow("same version") << "2.2.2" << "2.2.2" << false;
		QTest::newRow("newer server") << "3.0.0" << "2.2.2" << true;
		QTest::newRow("older server") << "1.3.4" << "2.2.2" << false;
		QTest::newRow("newer major") << "2.3.0" << "2.2.2" << true;
		QTest::newRow("older major") << "2.1.3" << "2.2.2" << false;
		QTest::newRow("newer minor") << "2.2.3" << "2.2.2" << true;
		QTest::newRow("older minor") << "2.2.1" << "2.2.2" << false;
		QTest::newRow("newer beta") << "2.2.2-beta.3" << "2.2.2-beta.2" << true;
		QTest::newRow("older beta")
			<< "2.2.2-beta.1" << "2.2.2-beta.2" << false;
		QTest::newRow("same stable on beta")
			<< "2.2.2" << "2.2.2-beta.2" << true;
		QTest::newRow("newer stable on older beta")
			<< "2.2.3" << "2.2.2-beta.2" << true;
		QTest::newRow("older stable on beta")
			<< "2.2.1" << "2.2.2-beta.2" << false;
	}

	void testIsNewerThan()
	{
		QFETCH(QString, version1);
		utils::News::Version v1 = utils::News::Version::parse(version1);
		QVERIFY(v1.isValid());

		QFETCH(QString, version2);
		utils::News::Version v2 = utils::News::Version::parse(version2);
		QVERIFY(v2.isValid());

		QFETCH(bool, expected);
		QCOMPARE(v1.isNewerThan(v2), expected);
	}
};


QTEST_MAIN(TestNews)
#include "news.moc"
