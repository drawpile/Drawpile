#include "libshared/net/annotation.h"
#include "libshared/net/control.h"
#include "libshared/net/meta.h"
#include "libshared/net/meta2.h"
#include "libshared/net/recording.h"
#include "libshared/net/layer.h"
#include "libshared/net/image.h"
#include "libshared/net/undo.h"
#include "libshared/net/brushes.h"
#include "libshared/net/textmode.h"

#include <QtTest/QtTest>

using namespace protocol;

Q_DECLARE_METATYPE(NullableMessageRef)

typedef QList<uint16_t> IdList;

class TestMessages final : public QObject
{
	Q_OBJECT
private slots:
	void testMessageSerialization_data()
	{
		QTest::addColumn<NullableMessageRef>("msg");
		QTest::newRow("command") << NullableMessageRef{static_cast<Message *>(new Command(0, QByteArray("testing...")))};
		QTest::newRow("disconnect") << NullableMessageRef{static_cast<Message *>(new Disconnect(1, Disconnect::KICK, "hello"))};
		QTest::newRow("ping") << NullableMessageRef{static_cast<Message *>(new Ping(2, true))};

		QTest::newRow("userjoin") << NullableMessageRef{static_cast<Message *>(new UserJoin(4, 0x03, QString("Test"), "asd"))};
		QTest::newRow("userjoin(no hash)") << NullableMessageRef{static_cast<Message *>(new UserJoin(4, 0x03, QString("Test"), QByteArray()))};
		QTest::newRow("userleave") << NullableMessageRef{static_cast<Message *>(new UserLeave(5))};
		QTest::newRow("sessionowner") << NullableMessageRef{static_cast<Message *>(new SessionOwner(6, QList<uint8_t>() << 1 << 2 << 5))};
		QTest::newRow("softreset") << NullableMessageRef{static_cast<Message *>(new SoftResetPoint(60))};
		QTest::newRow("chat") << NullableMessageRef{static_cast<Message *>(new Chat(7, 0x01, 0x04, QByteArray("Test")))};

		QTest::newRow("interval") << NullableMessageRef{static_cast<Message *>(new Interval(8, 0x1020))};
		QTest::newRow("lasertrail") << NullableMessageRef{static_cast<Message *>(new LaserTrail(9, 0xff223344, 0x80))};
		QTest::newRow("movepointer") << NullableMessageRef{static_cast<Message *>(new MovePointer(10, 0x11223344, 0x55667788))};
		QTest::newRow("marker") << NullableMessageRef{static_cast<Message *>(new Marker(11, QString("Test")))};
		QTest::newRow("useracl") << NullableMessageRef{static_cast<Message *>(new UserACL(12, QList<uint8_t>() << 1 << 2 << 4))};
		QTest::newRow("layeracl") << NullableMessageRef{static_cast<Message *>(new LayerACL(13, 0x1122, 0x01, 0x02, QList<uint8_t>() << 3 << 4 << 5))};
		QTest::newRow("featureaccess") << NullableMessageRef{static_cast<Message *>(new FeatureAccessLevels(14, reinterpret_cast<const uint8_t*>("\0\1\2\3\0\1\2\3\0")))};
		QTest::newRow("defaultlayer") << NullableMessageRef{static_cast<Message *>(new DefaultLayer(14, 0x1401))};

		QTest::newRow("undopoint") << NullableMessageRef{static_cast<Message *>(new UndoPoint(15))};
		QTest::newRow("canvasresize") << NullableMessageRef{static_cast<Message *>(new CanvasResize(16, -0xfff, 0xaaa, -0xbbb, 0xccc))};
		QTest::newRow("background(color)") << NullableMessageRef{static_cast<Message *>(new CanvasBackground(17, 0x00ff0000))};
		QTest::newRow("background(img)") << NullableMessageRef{static_cast<Message *>(new CanvasBackground(17, QByteArray(64*64*4, '\xff')))};
		QTest::newRow("layercreate") << NullableMessageRef{static_cast<Message *>(new LayerCreate(17, 0xaabb, 0xccdd, 0x11223344, 0x01, QString("Test layer")))};
		QTest::newRow("layerattributes") << NullableMessageRef{static_cast<Message *>(new LayerAttributes(18, 0xaabb, 0xcc, LayerAttributes::FLAG_CENSOR, 0x10, 0x22))};
		QTest::newRow("layerretitle") << NullableMessageRef{static_cast<Message *>(new LayerRetitle(19, 0xaabb, QString("Test")))};
		QTest::newRow("layerorder") << NullableMessageRef{static_cast<Message *>(new LayerOrder(20, QList<uint16_t>() << 0x1122 << 0x3344 << 0x4455))};
		QTest::newRow("layervisibility") << NullableMessageRef{static_cast<Message *>(new LayerVisibility(21, 0x1122, 1))};
		QTest::newRow("putimage") << NullableMessageRef{static_cast<Message *>(new PutImage(22, 0x1122, 0x10, 100, 200, 300, 400, QByteArray("Test")))};
		QTest::newRow("puttile") << NullableMessageRef{static_cast<Message *>(new PutTile(22, 0x1122, 0x10, 1, 2, 3, 0xaabbccdd))};
		QTest::newRow("fillrect") << NullableMessageRef{static_cast<Message *>(new FillRect(23, 0x1122, 0x10, 3, 200, 300, 400, 0x11223344))};
		QTest::newRow("penup") << NullableMessageRef{static_cast<Message *>(new PenUp(26))};
		QTest::newRow("annotationcreate") << NullableMessageRef{static_cast<Message *>(new AnnotationCreate(27, 0x1122, -100, -100, 200, 200))};
		QTest::newRow("annotationreshape") << NullableMessageRef{static_cast<Message *>(new AnnotationReshape(28, 0x1122, -100, -100, 200, 200))};
		QTest::newRow("annotationedit") << NullableMessageRef{static_cast<Message *>(new AnnotationEdit(29, 0x1122, 0x12345678, 7, 0x0a, QByteArray("Test")))};
		QTest::newRow("annotationdelete") << NullableMessageRef{static_cast<Message *>(new AnnotationDelete(30, 0x1122))};
		QTest::newRow("moveregion") << NullableMessageRef{static_cast<Message *>(new MoveRegion(30, 0x1122, 0, 1, 2, 3, 10, 11, 20, 21, 30, 31, 40, 41, QByteArray("test")))};

		QTest::newRow("classicdabs") << NullableMessageRef{static_cast<Message *>(new DrawDabsClassic(31, 0x1122, 100, -100, 0xff223344, 0x10, ClassicBrushDabVector() << ClassicBrushDab {1, 2, 3, 4, 5} << ClassicBrushDab {10, 20, 30, 40, 50}))};
		QTest::newRow("pixeldabs") << NullableMessageRef{static_cast<Message *>(new DrawDabsPixel(DabShape::Round, 32, 0x1122, 100, -100, 0xff223344, 0x10, PixelBrushDabVector() << PixelBrushDab {1, 2, 3, 4} << PixelBrushDab {10, 20, 30, 40}))};
		QTest::newRow("squarepixeldabs") << NullableMessageRef{static_cast<Message *>(new DrawDabsPixel(DabShape::Square, 32, 0x1122, 100, -100, 0xff223344, 0x10, PixelBrushDabVector() << PixelBrushDab {1, 2, 3, 4} << PixelBrushDab {10, 20, 30, 40}))};

		QTest::newRow("undo") << NullableMessageRef{static_cast<Message *>(new Undo(254, 1, false))};
		QTest::newRow("redo") << NullableMessageRef{static_cast<Message *>(new Undo(254, 1, true))};
	}

	void testMessageSerialization()
	{
		QFETCH(NullableMessageRef, msg);

		// Test binary serialization
		auto notEqual = Command(1, QByteArray("testing..."));
		QVERIFY(!msg->equals(notEqual));
		QVERIFY(msg->equals(*msg));

		QByteArray buffer(msg->length(), 0);
		QCOMPARE(msg->serialize(buffer.data()), msg->length());

		NullableMessageRef msg2 = Message::deserialize(reinterpret_cast<const uchar*>(buffer.constData()), buffer.size(), true);
		QVERIFY(!msg2.isNull());

		QVERIFY(msg->equals(*msg2));

		// Test text serialization (only valid for recordable types)
		if(msg->isRecordable()) {
			QStringList text = msg->toString().split('\n');

			text::Parser parser;
			text::Parser::Result r { text::Parser::Result::NeedMore, nullptr };
			for(const QString &line : text) {
				QCOMPARE(r.status, text::Parser::Result::NeedMore);
				r = parser.parseLine(line.trimmed());
				if(r.status == text::Parser::Result::Error)
					QFAIL(parser.errorString().toLocal8Bit().constData());
			};
			QCOMPARE(r.status, text::Parser::Result::Ok);
			QVERIFY(!r.msg.isNull());
			QVERIFY(msg->equals(*r.msg));
		}
	}

	void testFilteredWrapping()
	{
		MessagePtr original = MessagePtr(new CanvasResize(1, 2, 3, 4, 5));
		MessagePtr filtered = original->asFiltered();

		// Serializing filtered messages should work
		QByteArray serialized(filtered->length(), 0);
		const int written = filtered->serialize(serialized.data());
		QCOMPARE(written, filtered->length());

		// As should deserializing
		NullableMessageRef deserialized = Message::deserialize(reinterpret_cast<const uchar*>(serialized.data()), serialized.length(), true);
		QVERIFY(!deserialized.isNull());
		QCOMPARE(deserialized->type(), MSG_FILTERED);

		// The wrapped message should stay intact through the process
		// (assuming payload length is less than 65535)
		NullableMessageRef unwrapped = deserialized.cast<Filtered>().decodeWrapped();
		QVERIFY(!unwrapped.isNull());
		QVERIFY(unwrapped->equals(*original));
	}

	void testLayerOrderSanitation_data()
	{
		QTest::addColumn<IdList>("reorder");
		QTest::addColumn<IdList>("expected");

		QTest::newRow("valid") << (IdList() << 4 << 3 << 1 << 2) << (IdList() << 4 << 3 << 1 << 2);
		QTest::newRow("missing") << (IdList() << 3 << 1 << 2) << (IdList() << 3 << 1 << 2 << 4);
		QTest::newRow("extra") << (IdList() << 5 << 4 << 3 << 2 << 1) << (IdList() << 4 << 3 << 2 << 1);
		QTest::newRow("doubles") << (IdList() << 4 << 4 << 3 << 2 << 1) << (IdList() << 4 << 3 << 2 << 1);
		QTest::newRow("empty") << (IdList()) << (IdList() << 1 << 2 << 3 << 4);
	}

	void testLayerOrderSanitation()
	{
		QFETCH(IdList, reorder);
		QFETCH(IdList, expected);

		QList<uint16_t> current;
		current << 1 << 2 << 3 << 4;

		QCOMPARE(LayerOrder(1, reorder).sanitizedOrder(current), expected);
	}
};


QTEST_MAIN(TestMessages)
#include "messages.moc"

