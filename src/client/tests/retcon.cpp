#include "../canvas/retcon.h"
#include "../../shared/net/textmode.h"
#include "../../shared/net/brushes.h"

#include <QtTest/QtTest>

using namespace protocol;
using namespace canvas;

Q_DECLARE_METATYPE(AffectedArea)

class TestRetcon : public QObject
{
	Q_OBJECT
private slots:
	void testAffectedAreas_data()
	{
		QTest::addColumn<AffectedArea>("a1");
		QTest::addColumn<AffectedArea>("a2");
		QTest::addColumn<bool>("concurrent");

		// Changes to EVERYTHING are not concurrent with anything
		QTest::newRow("everything")
			<< AffectedArea(AffectedArea::EVERYTHING, 1, QRect(1,1,10,10))
			<< AffectedArea(AffectedArea::EVERYTHING, 2, QRect(100,100,10,10))
			<< false;
		QTest::newRow("everything2")
			<< AffectedArea(AffectedArea::EVERYTHING, 1, QRect(1,1,10,10))
			<< AffectedArea(AffectedArea::PIXELS, 2, QRect(100,100,10,10))
			<< false;

		// Same layer pixel changes are concurrent when bounding rects do not intersect
		QTest::newRow("same-layer1")
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,10,10))
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(100,1,10,10))
			<< true;
		QTest::newRow("same-layer2")
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,110,10))
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(100,1,10,10))
			<< false;

		// Pixels changes on different layers are always concurrent
		QTest::newRow("different-layer")
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,110,10))
			<< AffectedArea(AffectedArea::PIXELS, 2, QRect(100,1,10,10))
			<< true;

		// Changes to different domains are always concurrent (excluding the EVERYTHING domain)
		QTest::newRow("different-domains")
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,110,10))
			<< AffectedArea(AffectedArea::LAYERATTRS, 1, QRect(1, 1, 10, 10)) // bounds parameter is unused for this domain
			<< true;
		QTest::newRow("different-domains2")
			<< AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,110,10))
			<< AffectedArea(AffectedArea::ANNOTATION, 1, QRect(1, 1, 10, 10)) // bounds parameter is unused for this domain
			<< true;

		// Changes to different layers attributes are concurrent
		QTest::newRow("attrs")
			<< AffectedArea(AffectedArea::LAYERATTRS, 1)
			<< AffectedArea(AffectedArea::LAYERATTRS, 1)
			<< false;
		QTest::newRow("attrs2")
			<< AffectedArea(AffectedArea::LAYERATTRS, 1)
			<< AffectedArea(AffectedArea::LAYERATTRS, 2)
			<< true;

		// Changes to different annotations are concurrent
		QTest::newRow("annotations")
			<< AffectedArea(AffectedArea::ANNOTATION, 1)
			<< AffectedArea(AffectedArea::ANNOTATION, 2)
			<< true;
		QTest::newRow("annotations2")
			<< AffectedArea(AffectedArea::ANNOTATION, 2)
			<< AffectedArea(AffectedArea::ANNOTATION, 2)
			<< false;
	}

	void testAffectedAreas()
	{
		QFETCH(AffectedArea, a1);
		QFETCH(AffectedArea, a2);
		QFETCH(bool, concurrent);
		QCOMPARE(a1.isConcurrentWith(a2), concurrent);
	}

	void testConcurrent()
	{
		LocalFork lf;
		MessagePtr localMsg1 = msg("1 classicdabs layer=0x0101 x=10 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 5 512 255 255\n}");

		MessagePtr localMsg2 = msg("1 classicdabs layer=0x0101 x=20 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 0 512 255 255\n}");

		MessagePtr remoteMsg1 = msg("2 classicdabs layer=0x0101 x=100 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 5 512 255 255\n}");

		MessagePtr remoteMsg2 = msg("2 classicdabs layer=0x0101 x=120 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 0 512 255 255\n}");

		// First, we'll add our own local messages
		lf.addLocalMessage(
			localMsg1,
			AffectedArea(AffectedArea::PIXELS, 1, localMsg1.cast<DrawDabsClassic>().bounds())
			);
		lf.addLocalMessage(
			localMsg2,
			AffectedArea(AffectedArea::PIXELS, 1, localMsg2.cast<DrawDabsClassic>().bounds())
			);

		// Then, we'll "receive" messages from the server
		// The received messages are concurrent (do not intersect) with out local fork

		QCOMPARE(
			lf.handleReceivedMessage(
				remoteMsg1,
				AffectedArea(AffectedArea::PIXELS, 1, remoteMsg1.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::CONCURRENT
		);
		QCOMPARE(
			lf.handleReceivedMessage(
				remoteMsg2,
				AffectedArea(AffectedArea::PIXELS, 1, remoteMsg2.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::CONCURRENT
		);
		// Our own messages after making the roundtrip
		QCOMPARE(
			lf.handleReceivedMessage(
				localMsg1,
				AffectedArea(AffectedArea::PIXELS, 1, localMsg1.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::ALREADYDONE
		);
		QCOMPARE(
			lf.handleReceivedMessage(
				localMsg2,
				AffectedArea(AffectedArea::PIXELS, 1, localMsg2.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::ALREADYDONE
		);
	}

	void testConflict()
	{
		LocalFork lf;
		MessagePtr localMsg1 = msg("1 classicdabs layer=0x0101 x=10 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 5 512 255 255\n}");

		MessagePtr localMsg2 = msg("1 classicdabs layer=0x0101 x=20 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n"
			"5 0 512 255 255\n}");

		MessagePtr remoteMsg1 = msg("2 classicdabs layer=0x0101 x=200 y=10 color=#00ffffff mode=1 {\n"
			"2 0 512 255 255\n"
			"5 5 512 255 255\n}");

		MessagePtr remoteMsg2 = msg("2 classicdabs layer=0x0101 x=20 y=10 color=#00ffffff mode=1 {\n"
			"2 0 512 255 255\n"
			"5 0 512 255 255\n}");

		// First, we'll add our own local messages
		lf.addLocalMessage(
			localMsg1,
			AffectedArea(AffectedArea::PIXELS, 1, localMsg1.cast<DrawDabsClassic>().bounds())
			);
		lf.addLocalMessage(
			localMsg2,
			AffectedArea(AffectedArea::PIXELS, 1, localMsg2.cast<DrawDabsClassic>().bounds())
			);

		// Then, we'll "receive" messages from the server
		// The received messages are causally dependent on our messages, and thus
		// trigger a conflict
		QCOMPARE(
			lf.handleReceivedMessage(
				remoteMsg1,
				AffectedArea(AffectedArea::PIXELS, 1, remoteMsg1.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::CONCURRENT
		);
		QCOMPARE(
			lf.handleReceivedMessage(
				remoteMsg2,
				AffectedArea(AffectedArea::PIXELS, 1, remoteMsg2.cast<DrawDabsClassic>().bounds())
			),
			LocalFork::ROLLBACK
		);
	}

	void testUnexpected()
	{
		LocalFork lf;

		MessagePtr msg1 = msg("1 classicdabs layer=0x0101 x=10 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n}");

		MessagePtr msg2 = msg("1 classicdabs layer=0x0101 x=20 y=10 color=#00ffffff mode=1 {\n"
			"0 0 512 255 255\n}");

		MessagePtr msg3 = msg("1 classicdabs layer=0x0101 x=22 y=10 color=#00ffffff mode=1 {\n"
			"2 0 512 255 255\n}");

		// First, we'll add our own local messages
		lf.addLocalMessage(
			msg1,
			AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,1,1))
			);
		lf.addLocalMessage(
			msg2,
			AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,10,10))
			);

		QVERIFY(!lf.isEmpty());

		// Then, we'll "receive" something unexpected!
		QCOMPARE(
			lf.handleReceivedMessage(
				msg3,
				AffectedArea(AffectedArea::PIXELS, 1, QRect(100,100,1,1))
			),
			LocalFork::ROLLBACK
		);

		// Local fork should be automatically cleared
		QCOMPARE(lf.isEmpty(), true);
	}

	void testFallBehind()
	{
		LocalFork lf;
		lf.setFallbehind(10);

		lf.addLocalMessage(msg("1 penup"), AffectedArea(AffectedArea::PIXELS, 1, QRect(1,1,1,1)));

		protocol::MessagePtr recv = msg("2 penup");
		for(int i=0;i<9;++i) {
			QCOMPARE(
				lf.handleReceivedMessage(recv, AffectedArea(AffectedArea::ANNOTATION, 1)),
				LocalFork::CONCURRENT
			);
		}

		// Next one should go over the limit
		QCOMPARE(
			lf.handleReceivedMessage(recv, AffectedArea(AffectedArea::ANNOTATION, 1)),
			LocalFork::ROLLBACK
		);
	}

private:
	MessagePtr msg(const QString &line)
	{
		text::Parser p;
		QStringList lines = line.split('\n');
		text::Parser::Result r;
		int i=0;
		do {
			r = p.parseLine(lines.at(i++));
		} while(r.status==text::Parser::Result::NeedMore);

		if(r.status != text::Parser::Result::Ok || !r.msg)
			qFatal("invalid message: %s", qPrintable(line));

		return MessagePtr(r.msg);
	}
};


QTEST_MAIN(TestRetcon)
#include "retcon.moc"

