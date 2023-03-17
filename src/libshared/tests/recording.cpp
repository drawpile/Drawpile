#include "libshared/record/reader.h"
#include "libshared/record/writer.h"
#include "libshared/record/header.h"

#include "libshared/net/control.h"
#include "libshared/net/meta.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QtEndian>

#include <QDebug>

using namespace recording;
using namespace protocol;

// Hex encoded test recording.
// Header contains one extra key: "test": "TESTING"
// Protocol version is "dp:4.21.2"
// Body contains one message: UserJoin(1, 0, "hello", "world")
static const char *TEST_RECORDING = "44505245430000427b2274657374223a2254455354494e47222c2276657273696f6e223a2264703a342e32312e32222c2277726974657276657273696f6e223a22322e302e306232227d000c2001000568656c6c6f776f726c64";

// A test recording with a version number of dp:4.10.0, containing a single NewLayer message.
static const char *TEST_RECORDING_OLD = "44505245430000317b2276657273696f6e223a2264703a342e31302e30222c2277726974657276657273696f6e223a22322e302e306232227d00098201000100000000000000";

static const char *TEST_TEXTMODE =
	"!version=dp:4.21.2\n"
	"!test=TESTING\n"
	"1 join name=hello avatar=d29ybGQ=\n";

class TestRecording final : public QObject
{
	Q_OBJECT
private slots:
	void testWriter()
	{
		QBuffer buffer;
		buffer.open(QBuffer::ReadWrite);

		MessagePtr testMsg(new UserJoin(1, 0, QByteArray("hello"), QByteArray("world")));
		{
			Writer writer(&buffer, false);
			QJsonObject header;
			header["test"] = "TESTING";
			writer.writeHeader(header);

			// Non-recordable message: this should not be written
			writer.recordMessage(MessagePtr(new Command(0, QByteArray("{}"))));

			// This should be written
			writer.recordMessage(testMsg);
		}

		// Autoclose is off: writer shouldn't have closed the IO device
		QVERIFY(buffer.isOpen());

		// Verify that the format looks like expected
		const QByteArray b = buffer.buffer();

		// File should start with the magic number
		QVERIFY(b.startsWith(QByteArray("DPREC", 6)));

		// Followed by the JSON encoded metadata block
		quint16 mdlen = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(b.constData())+6);
        QJsonDocument mddoc = QJsonDocument::fromJson(b.mid(8, mdlen));

		QVERIFY(mddoc.isObject());

		QJsonObject mdobj = mddoc.object();

		// The protocol version should be automatically included
		QCOMPARE(mdobj["version"].toString(), ProtocolVersion::current().asString());

		// The extra header we added should be there too
		QCOMPARE(mdobj["test"].toString(), QString("TESTING"));

		// After the header, the recording should contain just the one recordable message
		QCOMPARE(b.length() - 8 - mdlen, testMsg->length());

		QByteArray msgbuf(testMsg->length(), 0);
		testMsg->serialize(msgbuf.data());

		QCOMPARE(msgbuf, b.mid(8+mdlen));
	}

	void testReader_data() {
		QTest::addColumn<QByteArray>("testRecording");
		QTest::addColumn<int>("encoding");
		QTest::newRow("bin") << QByteArray::fromHex(TEST_RECORDING) << int(Reader::Encoding::Binary);
		QTest::newRow("text") << QByteArray(TEST_TEXTMODE) << int(Reader::Encoding::Text);
	}

	void testReader()
	{
		QFETCH(QByteArray, testRecording);
		QFETCH(int, encoding);
		QBuffer buffer(&testRecording);
		buffer.open(QBuffer::ReadOnly);

		{
			Reader reader("test", &buffer, false);
			QCOMPARE(reader.filesize(), testRecording.length());
			QCOMPARE(reader.isCompressed(), false);

			Compatibility compat = reader.open();
			QCOMPARE(reader.formatVersion().asString(), QString("dp:4.21.2"));
			QCOMPARE(compat, COMPATIBLE);

			QCOMPARE(int(reader.encoding()), encoding);

			QCOMPARE(reader.formatVersion().asString(), QString("dp:4.21.2"));
			QCOMPARE(reader.metadata()["test"].toString(), QString("TESTING"));

			// No message read yet
			QCOMPARE(reader.currentIndex(), -1);
			const qint64 firstPosition = reader.filePosition();

			// There should be exactly one message in the test recording
			const MessageRecord mr1 = reader.readNext();

			QCOMPARE(mr1.status, MessageRecord::OK);

			MessagePtr testMsg(new UserJoin(1, 0, QByteArray("hello"), QByteArray("world")));

			QVERIFY(mr1.message.equals(testMsg));

			// current* returns the index and position of the last read message
			QCOMPARE(reader.currentIndex(), 0);
			QCOMPARE(reader.currentPosition(), firstPosition);

			// Next message should be EOF
			const MessageRecord mr2 = reader.readNext();
			QCOMPARE(mr2.status, MessageRecord::END_OF_RECORDING);
			QVERIFY(reader.isEof());

			// Rewinding should take us back to the beginning
			reader.rewind();
			QCOMPARE(reader.currentIndex(), -1);
			QCOMPARE(reader.filePosition(), firstPosition);

			const MessageRecord mr3 = reader.readNext();
			QCOMPARE(mr3.status, MessageRecord::OK);
			QVERIFY(mr1.message.equals(mr3.message));
		}

		// Autoclose is not enabled
		QVERIFY(buffer.isOpen());
	}

	void testSkip()
	{
		QBuffer buffer;
		buffer.open(QBuffer::ReadWrite);

		MessagePtr testMsg(new UserJoin(1, 0, QByteArray("hello"), QByteArray("world")));
		Writer writer(&buffer, false);
		writer.writeMessage(*testMsg);
		writer.writeMessage(*testMsg);

		buffer.seek(0);
		uint8_t mtype, ctx;
		QCOMPARE(skipRecordingMessage(&buffer, &mtype, &ctx), testMsg->length());
		QCOMPARE(buffer.pos(), testMsg->length());
		QCOMPARE(mtype, uint8_t(MSG_USER_JOIN));
		QCOMPARE(ctx, uint8_t(1));

		QCOMPARE(skipRecordingMessage(&buffer), testMsg->length());
		QCOMPARE(buffer.pos(), buffer.size());

		QVERIFY(skipRecordingMessage(&buffer)<0);
	}

	void testVersionMismatch()
	{
		QByteArray testRecording = QByteArray::fromHex(TEST_RECORDING_OLD);
		QBuffer buffer(&testRecording);
		buffer.open(QBuffer::ReadOnly);

		Reader reader("test", &buffer, false);

		Compatibility compat = reader.open();
		QCOMPARE(compat, INCOMPATIBLE);
	}

	void testTextVersionMismatch_data() {
		QByteArray data("1 join name=hello hash=world\n");
		QTest::addColumn<QByteArray>("testRecording");
		QTest::addColumn<bool>("compat");
		QTest::newRow("ok") << "!version=" + protocol::ProtocolVersion::current().asString().toUtf8() + "\n" + data << true;
		QTest::newRow("wrongMajor") << "!version=dp:4.10.0\n" + data << false;
		QTest::newRow("wrongServer") << "!version=dp:3.20.0\n" + data << false;
		QTest::newRow("wrongNs") << "!version=pd:4.20.0\n" + data << false;
	}

	void testTextVersionMismatch()
	{
		QFETCH(QByteArray, testRecording);
		QFETCH(bool, compat);
		QBuffer buffer(&testRecording);
		buffer.open(QBuffer::ReadOnly);

		// In opaque mode, version must match exactly (except for the minor number)
		Reader reader("test", &buffer, false);
		Compatibility c = reader.openOpaque();
		int expectedCompat = compat ? int(COMPATIBLE) : int(INCOMPATIBLE);
		QCOMPARE(int(c), expectedCompat);
	}

	void testOpaqueBinary()
	{
		QByteArray testRecording = QByteArray::fromHex(TEST_RECORDING_OLD);
		QBuffer buffer(&testRecording);
		buffer.open(QBuffer::ReadOnly);

		Reader reader("test", &buffer, false);

		// In opaque mode, any binary recording with a matching server number is compatible
		Compatibility compat = reader.openOpaque();
		QCOMPARE(compat, COMPATIBLE);

		QCOMPARE(reader.formatVersion().asString(), QString("dp:4.10.0"));

		// The actual message should be of type OpaqueMessage
		MessageRecord mr = reader.readNext();
		QCOMPARE(mr.status, MessageRecord::OK);
		QVERIFY(!mr.message.isNull());
		QCOMPARE(mr.message->type(), protocol::MSG_LAYER_CREATE);
	}
};


QTEST_MAIN(TestRecording)
#include "recording.moc"

