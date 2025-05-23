// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/html.h"

#include <QtTest/QtTest>

class TestHtmlUtils final : public QObject
{
	Q_OBJECT
private slots:
	void testLinkify_data()
	{
		QTest::addColumn<QString>("text");
		QTest::addColumn<QString>("expected");

		QTest::newRow("nourls") << "No URLs" << "No URLs";
		QTest::newRow("just domain") << "www.example.com" << "<a href=\"http://www.example.com\">www.example.com</a>";
		QTest::newRow("text w/domain") << "hello www.example.com world" << "hello <a href=\"http://www.example.com\">www.example.com</a> world";
		QTest::newRow("full URL") << "hello https://www.example.com/ world" << "hello <a href=\"https://www.example.com/\">https://www.example.com/</a> world";
		QTest::newRow("full URL with an @ in the path") << "hello https://www.example.com/@someone/12345 world" << "hello <a href=\"https://www.example.com/@someone/12345\">https://www.example.com/@someone/12345</a> world";
	}

	void testLinkify()
	{
		QFETCH(QString, text);
		QFETCH(QString, expected);

		QCOMPARE(htmlutils::linkify(text), expected);
	}
};


QTEST_MAIN(TestHtmlUtils)
#include "html.moc"

