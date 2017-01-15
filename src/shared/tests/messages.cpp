#include "../net/annotation.h"
#include "../net/control.h"
#include "../net/meta.h"
#include "../net/meta2.h"
#include "../net/recording.h"
#include "../net/layer.h"
#include "../net/image.h"
#include "../net/undo.h"
#include "../net/pen.h"

#include <QtTest/QtTest>

using namespace protocol;

Q_DECLARE_METATYPE(Message*)

typedef QList<uint16_t> IdList;

class TestMessages: public QObject
{
	Q_OBJECT
private slots:
	void testMessageSerialization_data()
	{
		QTest::addColumn<Message*>("msg");
		QTest::newRow("command") << (Message*)new Command(0, QByteArray("testing..."));
		QTest::newRow("disconnect") << (Message*)new Disconnect(1, Disconnect::KICK, "hello");
		QTest::newRow("ping") << (Message*)new Ping(2, true);

		QTest::newRow("userjoin") << (Message*)new UserJoin(4, 0x03, QString("Test"), "asd");
		QTest::newRow("userjoin(no hash)") << (Message*)new UserJoin(4, 0x03, QString("Test"), QByteArray());
		QTest::newRow("userleave") << (Message*)new UserLeave(5);
		QTest::newRow("sessionowner") << (Message*)new SessionOwner(6, QList<uint8_t>() << 1 << 2 << 5);
		QTest::newRow("chat") << (Message*)new Chat(7, 0x01, 0x04, QByteArray("Test"));

		QTest::newRow("interval") << (Message*)new Interval(8, 0x1020);
		QTest::newRow("lasertrail") << (Message*)new LaserTrail(9, 0x11223344, 0x80);
		QTest::newRow("movepointer") << (Message*)new MovePointer(10, 0x11223344, 0x55667788);
		QTest::newRow("marker") << (Message*)new Marker(11, QString("Test"));
		QTest::newRow("useracl") << (Message*)new UserACL(12, QList<uint8_t>() << 1 << 2 << 4);
		QTest::newRow("layeracl") << (Message*)new LayerACL(13, 0x1122, 0x01, QList<uint8_t>() << 1 << 2 << 4);
		QTest::newRow("sessionacl") << (Message*)new SessionACL(14, 0x1122);

		QTest::newRow("undopoint") << (Message*)new UndoPoint(15);
		QTest::newRow("canvasresize") << (Message*)new CanvasResize(16, -0xfff, 0xaaa, -0xbbb, 0xccc);
		QTest::newRow("layercreate") << (Message*)new LayerCreate(17, 0xaabb, 0xccdd, 0x11223344, 0x01, QString("Test"));
		QTest::newRow("layerattributes") << (Message*)new LayerAttributes(18, 0xaabb, 0xcc, 0x10);
		QTest::newRow("layerretitle") << (Message*)new LayerRetitle(19, 0xaabb, QString("Test"));
		QTest::newRow("layerorder") << (Message*)new LayerOrder(20, QList<uint16_t>() << 0x1122 << 0x3344 << 0x4455);
		QTest::newRow("layervisibility") << (Message*)new LayerVisibility(21, 0x1122, 1);
		QTest::newRow("putimage") << (Message*)new PutImage(22, 0x1122, 0x10, 100, 200, 300, 400, QByteArray("Test"));
		QTest::newRow("fillrect") << (Message*)new FillRect(23, 0x1122, 0x10, 100, 200, 300, 400, 0x11223344);
		QTest::newRow("toolchange") << (Message*)new ToolChange(24, 0x1122, 1, 2, 3, 0xaabbccdd, 10, 11, 20, 21, 30, 31, 40, 41, 60);
		QTest::newRow("penmove") << (Message*)new PenMove(25, PenPointVector() << PenPoint {-10, 10, 0x00ff} << PenPoint { -100, 100, 0xff00 });
		QTest::newRow("penup") << (Message*)new PenUp(26);
		QTest::newRow("annotationcreate") << (Message*)new AnnotationCreate(27, 0x1122, -100, -100, 200, 200);
		QTest::newRow("annotationreshape") << (Message*)new AnnotationReshape(28, 0x1122, -100, -100, 200, 200);
		QTest::newRow("annotationedit") << (Message*)new AnnotationEdit(29, 0x1122, 0x12345678, QByteArray("Test"));
		QTest::newRow("annotationdelete") << (Message*)new AnnotationDelete(30, 0x1122);

		QTest::newRow("undo") << (Message*)new Undo(254, 1, true);
	}

	void testMessageSerialization()
	{
		QFETCH(Message*, msg);

		auto notEqual = Command(1, QByteArray("testing..."));
		QVERIFY(!msg->equals(notEqual));
		QVERIFY(msg->equals(*msg));

		QByteArray buffer(msg->length(), 0);
		QCOMPARE(msg->serialize(buffer.data()), msg->length());

		Message *msg2 = Message::deserialize(reinterpret_cast<const uchar*>(buffer.constData()), buffer.size(), true);
		QVERIFY(msg2);

		QVERIFY(msg->equals(*msg2));
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

