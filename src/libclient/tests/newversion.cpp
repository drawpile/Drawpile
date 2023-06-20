// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/newversion.h"

#include <QXmlStreamReader>
#include <QFile>
#include <QTest>

class TestNewVersionCheck final : public QObject
{
	Q_OBJECT
private slots:

	void initTestCase()
	{
		QFile testfile(":/test/appdata-test.xml");
		testfile.open(QFile::ReadOnly);
		m_doc = testfile.readAll();

		QVERIFY(m_doc.length()>0);
	}

	void testVersions_data()
	{
		// This version
		QTest::addColumn<int>("server");
		QTest::addColumn<int>("major");
		QTest::addColumn<int>("minor");
		// Available updates
		QTest::addColumn<QString>("newer");

		QTest::newRow("1.0.0") << 1 << 0 << 0 << "3.0.0 2.2.0 2.1.3 2.1.0 2.0.0";
		QTest::newRow("2.1.1") << 2 << 1 << 1 << "3.0.0 2.2.0 2.1.3";
		QTest::newRow("2.1.3") << 2 << 1 << 3 << "3.0.0 2.2.0";
		QTest::newRow("2.1.4") << 2 << 1 << 4 << "3.0.0 2.2.0";
		QTest::newRow("2.2.0") << 2 << 2 << 0 << "3.0.0";
		QTest::newRow("2.2.9999") << 2 << 2 << 9999 << "3.0.0";
		QTest::newRow("3.0.0") << 3 << 0 << 0 << QString();
		QTest::newRow("3.0.1") << 3 << 0 << 1 << QString();
	}

	void testVersions()
	{
		QFETCH(int, server);
		QFETCH(int, major);
		QFETCH(int, minor);
		QFETCH(QString, newer);

		NewVersionCheck vc(server, major, minor, 0);

		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		QVector<NewVersionCheck::Version> available = vc.getNewer();

		const QStringList expected = newer.isEmpty() ? QStringList() : newer.split(" ");
		QCOMPARE(expected.size(), available.size());

		for(int i=0;i<expected.size();++i) {
			QCOMPARE(available.at(i).version, expected.at(i));
		}
	}

	void testBetas()
	{
		NewVersionCheck vc(0, 0, 1, 1);

		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		// All versions included when showBetas is true
		QCOMPARE(vc.getNewer().size(), 6);
	}

	void testDescription()
	{
		NewVersionCheck vc(2, 9999, 9999, 0);
		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		QCOMPARE(vc.getNewer().size(), 1);
		NewVersionCheck::Version latest = vc.getNewer().at(0);

		const QString expected = "<p>Future test release</p><p>New features:</p><ul><li>All the features</li></ul><p>Another bad html test</p>";

		QCOMPARE(latest.description, expected);
	}

	void testArtifact()
	{
		NewVersionCheck vc(2, 9999, 9999, 0);
		vc.setPlatform("win64");
		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		NewVersionCheck::Version latest = vc.getNewer().at(0);

		QCOMPARE(latest.downloadUrl, QString("http://example.com/download-win64.zip"));
		QCOMPARE(latest.downloadChecksum, QString("test"));
		QCOMPARE(latest.downloadChecksumType, QString("sha256"));
		QCOMPARE(latest.downloadSize, 100);
	}

private:
	QByteArray m_doc;
};


QTEST_MAIN(TestNewVersionCheck)
#include "newversion.moc"

