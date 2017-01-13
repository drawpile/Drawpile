#include "../record/reader.h"
#include "../record/writer.h"
#include "../record/header.h"

#include "../net/control.h"
#include "../net/meta.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QtEndian>

#include <QDebug>

using namespace recording;
using namespace protocol;

// Hex encoded test recording.
// Header contains one extra key: "test": "TESTING"
// Protocol version is "dp:4.20.1"
// Body contains one message: UserJoin(1, 0, "hello", "world")
static const char *TEST_RECORDING = "44505245430000427b2274657374223a2254455354494e47222c2276657273696f6e223a2264703a342e32302e31222c2277726974657276657273696f6e223a22322e302e306232227d000c2001000568656c6c6f776f726c64";

class TestRecording: public QObject
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
		quint16 mdlen = qFromBigEndian<quint16>((const uchar*)b.constData()+6);
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

	void testReader()
	{
		QByteArray testRecording = QByteArray::fromHex(TEST_RECORDING);
		QBuffer buffer;
		buffer.setBuffer(&testRecording);
		buffer.open(QBuffer::ReadOnly);

		{
			Reader reader("test", &buffer, false);
			QCOMPARE(reader.filesize(), testRecording.length());
			QCOMPARE(reader.isCompressed(), false);

			Compatibility compat = reader.open();
			QCOMPARE(compat, COMPATIBLE);

			QCOMPARE(reader.formatVersion().asString(), QString("dp:4.20.1"));
			QCOMPARE(reader.metadata()["test"].toString(), QString("TESTING"));

			// No message read yet
			QCOMPARE(reader.currentIndex(), -1);
			const qint64 firstPosition = reader.filePosition();

			// There should be exactly one message in the test recording
			MessageRecord mr = reader.readNext();

			QCOMPARE(mr.status, MessageRecord::OK);

			MessagePtr testMsg(new UserJoin(1, 0, QByteArray("hello"), QByteArray("world")));
			MessagePtr readMsg(mr.message);

			QVERIFY(readMsg.equals(testMsg));

			// current* returns the index and position of the last read message
			QCOMPARE(reader.currentIndex(), 0);
			QCOMPARE(reader.currentPosition(), firstPosition);

			// Next message should be EOF
			mr = reader.readNext();
			QCOMPARE(mr.status, MessageRecord::END_OF_RECORDING);
			QVERIFY(reader.isEof());

			// Rewinding should take us back to the beginning
			reader.rewind();
			QCOMPARE(reader.currentIndex(), -1);
			QCOMPARE(reader.filePosition(), firstPosition);

			mr = reader.readNext();
			QCOMPARE(mr.status, MessageRecord::OK);
			MessagePtr readMsg2(mr.message);
			QVERIFY(readMsg.equals(readMsg2));
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
};


QTEST_MAIN(TestRecording)
#include "recording.moc"

