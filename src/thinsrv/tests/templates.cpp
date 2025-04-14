// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/inmemoryhistory.h"
#include "libshared/net/protover.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/ulid.h"
#include "thinsrv/templatefiles.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <dpcommon/platform_qt.h>
extern "C" {
#include <dpmsg/message.h>
}

using namespace server;

class TestTemplates final : public QObject {
	Q_OBJECT
private slots:
	void testTemplateLoading()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir dir(tempDir.path());

		// Copy some test files to the temp dir
		QVERIFY(QFile(":test/test-config.cfg")
					.copy(dir.absoluteFilePath("test.cfg")));
		{
			QFile testFile(dir.absoluteFilePath("test.dptxt"));
			QVERIFY(testFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
			QByteArray testContent =
				QByteArrayLiteral("!version=" DP_PROTOCOL_VERSION "\n"
								  "!maxUserCount=1\n"
								  "!founder=tester\n"
								  "!password=plain;qwerty123\n"
								  "!title=Test\n"
								  "!nsfm=false\n"
								  "\n"
								  "1 resize bottom=3000 right=3000\n"
								  "1 newlayer id=0x0101 title=background\n");
			QCOMPARE(testFile.write(testContent), testContent.size());
			QVERIFY(testFile.flush());
			testFile.close();
		}
		QVERIFY(touch(dir.absoluteFilePath("empty.dptxt")));

		// Scan templates
		TemplateFiles templates(dir);

		// There should now be one available template
		QCOMPARE(templates.templateDescriptions(true).size(), 1);
		QCOMPARE(templates.exists("test"), true);
		QCOMPARE(templates.exists("empty"), false);

		QJsonObject desc = templates.templateDescription("test");
		QCOMPARE(desc.value("maxUserCount").toInt(), 1);
		QCOMPARE(desc.value("founder").toString(), QString("tester"));
		QCOMPARE(desc.value("hasPassword").toBool(), true);
		QCOMPARE(desc.value("title").toString(), QString("Test"));
		QCOMPARE(desc.value("nsfm").toBool(), false);

		// Try loading the template
		InMemoryHistory history(
			Ulid::make().toString(), "test",
			protocol::ProtocolVersion::fromString(
				desc.value("protocol").toString()),
			desc.value("founder").toString());

		QVERIFY(templates.init(&history));

		// Check session metadata
		QCOMPARE(history.founderName(), QString("tester"));
		QCOMPARE(history.maxUsers(), 1);
		QCOMPARE(history.title(), QString("Test"));
		QCOMPARE(int(history.flags()), 0);
		QCOMPARE(
			passwordhash::check("qwerty123", history.passwordHash()), true);

		// History content should now match the test template
		net::MessageList msgs;
		int last;
		std::tie(msgs, last) = history.getBatch(-1);

		QCOMPARE(msgs.size(), 2);

		QCOMPARE(msgs.at(0).type(), DP_MSG_CANVAS_RESIZE);
		QCOMPARE(msgs.at(1).type(), DP_MSG_LAYER_CREATE);
	}

private:
	bool touch(const QString &path)
	{
		QFile f{path};
		if(!f.open(DP_QT_WRITE_FLAGS))
			return false;
		f.close();
		return true;
	}
};


QTEST_MAIN(TestTemplates)
#include "templates.moc"
