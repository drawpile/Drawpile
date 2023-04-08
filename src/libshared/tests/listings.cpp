// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/listings/listserverfinder.h"

#include <QtTest/QtTest>
#include <QBuffer>

class TestListings final : public QObject
{
	Q_OBJECT
private slots:
	void testUrlFinding_data()
	{
		QTest::addColumn<QByteArray>("html");
		QTest::addColumn<QString>("link");

		QTest::newRow("blank")
			<< QByteArray()
			<< QString();
		QTest::newRow("none")
			<< QByteArray("<!DOCTYPE html><html><head></head></html>")
			<< QString();
		QTest::newRow("xhtml")
			<< QByteArray("<!DOCTYPE html><html><head><meta name=\"something\" content=\"other\" /><meta name=\"drawpile:list-server\" content=\"http://example.com\" /></head><body><p>body stuff</p></body></html>")
			<< "http://example.com";

		QTest::newRow("html")
			<< QByteArray("<!DOCTYPE html><html><head><meta name=\"something\" content=\"other\"><meta name=\"drawpile:list-server\" content=\"http://example.com\"></head><meta name=\"drawpile:list-server\" content=\"ignore this\"></body></html>")
			<< "http://example.com";
		QTest::newRow("html w/link")
			<< QByteArray("<!DOCTYPE html><html><head><link rel=\"stuff\"><title>title</title><meta name=\"something\" content=\"other\"><meta name=\"drawpile:list-server\" content=\"http://example.com\"></head><meta name=\"drawpile:list-server\" content=\"ignore this\"></body></html>")
			<< "http://example.com";
		QTest::newRow("inbody")
			<< QByteArray("<!DOCTYPE html><html><head><meta name=\"something\" content=\"other\"></head><meta name=\"drawpile:list-server\" content=\"ignore this\"></body></html>")
			<< QString();
	}

	void testUrlFinding()
	{
		QFETCH(QByteArray, html);
		QFETCH(QString, link);

		QBuffer buf(&html);
		buf.open(QBuffer::ReadOnly);

		const auto extractedLink = sessionlisting::findListserverLinkHtml(&buf);
		QCOMPARE(extractedLink, link);
	}
};


QTEST_MAIN(TestListings)
#include "listings.moc"

