#include "libclient/utils/newversion.h"

#include <QXmlStreamReader>
#include <QFile>
#include <QTest>

class TestNewVersionCheck: public QObject
{
	Q_OBJECT
private slots:

	void initTestCase()
	{
		{
			QFile testfile(":/test/appdata-test.xml");
			testfile.open(QFile::ReadOnly);
			m_doc = testfile.readAll();
			QVERIFY(m_doc.length()>0);
		}

		{
			QFile testfile(":/test/appdata-lax.xml");
			testfile.open(QFile::ReadOnly);
			m_docLax = testfile.readAll();
			QVERIFY(m_docLax.length()>0);
		}
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

		NewVersionCheck vc(server, major, minor);

		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		QVector<NewVersionCheck::Version> available = vc.getNewer();

		const QStringList expected = newer.isEmpty() ? QStringList() : newer.split(" ");
		QCOMPARE(expected.size(), available.size());

		for(int i=0;i<expected.size();++i) {
			QCOMPARE(available.at(i).version, expected.at(i));
		}
	}

	void testBetas_data()
	{
		// This version
		QTest::addColumn<int>("server");
		QTest::addColumn<int>("major");
		QTest::addColumn<int>("minor");
		QTest::addColumn<QString>("tag");
		// Expected newer versions (from appdata-test.xml)
		QTest::addColumn<QString>("newer");

		QTest::newRow("1.0.0-alpha") << 1 << 0 << 0 << "alpha" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1 1.0.0-beta.11 1.0.0-beta.2 1.0.0-beta 1.0.0-alpha.beta 1.0.0-alpha.1";
		QTest::newRow("1.0.0-alpha.1") << 1 << 0 << 0 << "alpha.1" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1 1.0.0-beta.11 1.0.0-beta.2 1.0.0-beta 1.0.0-alpha.beta";
		QTest::newRow("1.0.0-alpha.beta") << 1 << 0 << 0 << "alpha.beta" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1 1.0.0-beta.11 1.0.0-beta.2 1.0.0-beta";
		QTest::newRow("1.0.0-beta") << 1 << 0 << 0 << "beta" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1 1.0.0-beta.11 1.0.0-beta.2";
		QTest::newRow("1.0.0-beta.2") << 1 << 0 << 0 << "beta.2" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1 1.0.0-beta.11";
		QTest::newRow("1.0.0-beta.11") << 1 << 0 << 0 << "beta.11" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0 1.0.0-rc.1";
		QTest::newRow("1.0.0-rc.1") << 1 << 0 << 0 << "rc.1" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0 1.0.0";
		QTest::newRow("1.0.0") << 1 << 0 << 0 << "" << "3.0.1 3.0.0 2.2.0 2.1.3 2.1.0 2.0.0";
	}

	void testBetas()
	{
		QFETCH(int, server);
		QFETCH(int, major);
		QFETCH(int, minor);
		QFETCH(QString, tag);
		QFETCH(QString, newer);

		NewVersionCheck vc(server, major, minor, tag);
		vc.setShowBetas(true);

		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		QVector<NewVersionCheck::Version> available = vc.getNewer();

		const QStringList expected = newer.isEmpty() ? QStringList() : newer.split(" ");
		QCOMPARE(expected.size(), available.size());

		for(int i=0;i<expected.size();++i) {
			QCOMPARE(available.at(i).version, expected.at(i));
		}
	}

	void testLax_data()
	{
		// This version
		QTest::addColumn<int>("server");
		QTest::addColumn<int>("major");
		QTest::addColumn<int>("minor");
		QTest::addColumn<QString>("tag");
		// Expected newer versions (from appdata-lax.xml)
		QTest::addColumn<QString>("newer");

		QTest::newRow("000.111") << 0 << 111 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201 10 10-beta.101 10-beta.02 10-beta.1 03.010";
		QTest::newRow("03.010") << 3 << 10 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201 10 10-beta.101 10-beta.02 10-beta.1";
		QTest::newRow("10-beta.1") << 10 << 0 << 0 << "beta.1" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201 10 10-beta.101 10-beta.02";
		QTest::newRow("10-beta.02") << 10 << 0 << 0 << "beta.02" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201 10 10-beta.101";
		QTest::newRow("10-beta.101") << 10 << 0 << 0 << "beta.101" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201 10";
		QTest::newRow("10") << 10 << 0 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1 20230201";
		QTest::newRow("20230201") << 20230201 << 0 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02 20230201.1";
		QTest::newRow("20230201.1") << 20230201 << 1 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta 20230201.02";
		QTest::newRow("20230201.02") << 20230201 << 2 << 0 << "" << "20230201.02.3.4.5garbage 20230201.02.3.4.5garbage-beta";
		QTest::newRow("20230201.02.3.4.5garbage-beta") << 20230201 << 2 << 3 << "beta" << "20230201.02.3.4.5garbage";
		QTest::newRow("20230201.02.3.4.5garbage") << 20230201 << 2 << 3 << "" << "";
	}

	void testLax()
	{
		QFETCH(int, server);
		QFETCH(int, major);
		QFETCH(int, minor);
		QFETCH(QString, tag);
		QFETCH(QString, newer);

		NewVersionCheck vc(server, major, minor, tag);
		vc.setShowBetas(true);

		QXmlStreamReader reader(m_docLax);
		QVERIFY(vc.parseAppDataFile(reader));

		QVector<NewVersionCheck::Version> available = vc.getNewer();

		const QStringList expected = newer.isEmpty() ? QStringList() : newer.split(" ");
		QCOMPARE(expected.size(), available.size());

		for(int i=0;i<expected.size();++i) {
			QCOMPARE(available.at(i).version, expected.at(i));
		}
	}

	void testDescription()
	{
		NewVersionCheck vc(2, 9999, 9999);
		QXmlStreamReader reader(m_doc);
		QVERIFY(vc.parseAppDataFile(reader));

		QCOMPARE(vc.getNewer().size(), 1);
		NewVersionCheck::Version latest = vc.getNewer().at(0);

		const QString expected = "<p>Future test release</p><p>New features:</p><ul><li>All the features</li></ul><p>Another bad html test</p>";

		QCOMPARE(latest.description, expected);
	}

	void testArtifact()
	{
		NewVersionCheck vc(2, 9999, 9999);
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
	QByteArray m_docLax;
};


QTEST_MAIN(TestNewVersionCheck)
#include "newversion.moc"

