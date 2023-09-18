// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/filedhistory.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/qtcompat.h"
#include "libshared/util/ulid.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <memory>

using namespace server;

class TestFiledHistory final : public QObject {
	Q_OBJECT
private slots:
	void initTestCase()
	{
		QVERIFY(m_tempdir.isValid());
		m_dir.setPath(m_tempdir.path());
	}

	// Test that all metadata is stored correctly
	void testMetadata()
	{
		const QString password = "pass";
		const QString opword = "magic";
		const int maxUsers = 11;
		const QString title = "Hello world";
		const QString idAlias = "test";
		const protocol::ProtocolVersion protover =
			protocol::ProtocolVersion::current();
		const QString founder = "me!";
		const SessionHistory::Flags flags = SessionHistory::Persistent |
											SessionHistory::PreserveChat |
											SessionHistory::Nsfm;

		const QString bannedUser = "troll user with a long \\ \"name}%[";
		const QString opUser = "op";
		const QHostAddress bannedAddress("::ffff:192.168.0.100");
		const QString bannedExtAuthId = "trololo";

		const QString announcementUrl = "http://example.com/";

		auto testId = Ulid::make().toString();
		{
			std::unique_ptr<FiledHistory> fh{FiledHistory::startNew(
				m_dir, testId, idAlias, protover, founder)};
			QVERIFY(fh.get());
			fh->setPasswordHash(passwordhash::hash(password));
			fh->setOpwordHash(passwordhash::hash(opword));
			fh->setMaxUsers(200);
			fh->setMaxUsers(
				maxUsers); // this should replace the previously set value
			fh->setTitle(title);
			fh->setFlags(flags);
			fh->addBan(bannedUser, bannedAddress, QString(), opUser);
			fh->addBan(
				"test", QHostAddress("192.168.0.101"), QString(), opUser);
			fh->addBan(
				"test3", QHostAddress("192.168.0.102"), bannedExtAuthId,
				opUser);
			fh->removeBan(2);
			fh->addAnnouncement(announcementUrl);
			fh->addAnnouncement("http://example.com/2/");
			fh->removeAnnouncement("http://example.com/2/");
			fh->joinUser(1, "u1");
			fh->joinUser(2, "u2");
			fh->setAuthenticatedOperator("u1", true);
			fh->setAuthenticatedOperator("u2", true);
			fh->setAuthenticatedOperator("u1", false);

			// The history file must have some content before it can be loaded
			fh->addMessage(
				net::makeChatMessage(1, 0, 0, QStringLiteral("test")));
		}

		{
			std::unique_ptr<FiledHistory> fh{FiledHistory::load(
				m_dir.absoluteFilePath(FiledHistory::journalFilename(testId)))};
			QVERIFY(fh.get());

			QCOMPARE(fh->id(), testId);
			QCOMPARE(fh->idAlias(), idAlias);
			QCOMPARE(fh->founderName(), founder);
			QCOMPARE(fh->protocolVersion(), protover);
			QVERIFY(passwordhash::check(password, fh->passwordHash()));
			QVERIFY(passwordhash::check(opword, fh->opwordHash()));
			QCOMPARE(fh->maxUsers(), maxUsers);
			QCOMPARE(fh->title(), title);
			QCOMPARE(fh->flags(), flags);

			QJsonArray banlist = fh->banlist().toJson(true);
			QCOMPARE(banlist.size(), 2);
			QCOMPARE(
				banlist.at(0).toObject()["username"].toString(), bannedUser);
			QCOMPARE(banlist.at(0).toObject()["bannedBy"].toString(), opUser);
			QCOMPARE(
				banlist.at(0).toObject()["ip"].toString(),
				bannedAddress.toString());
			QCOMPARE(
				banlist.at(0).toObject()["extauthid"].toString(), QString());

			QCOMPARE(
				banlist.at(1).toObject()["username"].toString(),
				QString("test3"));
			QCOMPARE(
				banlist.at(1).toObject()["extauthid"].toString(),
				bannedExtAuthId);
			QCOMPARE(banlist.at(1).toObject()["bannedBy"].toString(), opUser);

			QStringList announcements = fh->announcements();
			QCOMPARE(announcements.size(), 1);
			QCOMPARE(announcements.at(0), announcementUrl);

			QCOMPARE(fh->idQueue().getIdForName("u1"), uint8_t(1));
			QCOMPARE(fh->idQueue().getIdForName("u2"), uint8_t(2));

			QCOMPARE(fh->isAuthenticatedOperators(), true);
			QCOMPARE(fh->isOperator("u1"), false);
			QCOMPARE(fh->isOperator("u2"), true);
		}
	}

	static QString getChatMessage(const net::Message &msg)
	{
		size_t len;
		const char *message = DP_msg_chat_message(msg.toChat(), &len);
		return QString::fromUtf8(message, compat::castSize(len));
	}

	// Test that a recording can be loaded correctly
	void testLoading()
	{
		QString file = makeTestRecording();
		std::unique_ptr<FiledHistory> fh{
			FiledHistory::load(m_dir.absoluteFilePath(file))};

		net::MessageList msgs;
		int lastIdx;

		std::tie(msgs, lastIdx) = fh->getBatch(-1);

		QCOMPARE(msgs.size(), 3);
		QCOMPARE(getChatMessage(msgs.at(0)), QString("test1"));
		QCOMPARE(lastIdx, 2);

		std::tie(msgs, lastIdx) = fh->getBatch(0);

		QCOMPARE(msgs.size(), 2);
		QCOMPARE(getChatMessage(msgs.at(0)), QString("test2"));
		QCOMPARE(lastIdx, 2);

		std::tie(msgs, lastIdx) = fh->getBatch(1);

		QCOMPARE(msgs.size(), 1);
		QCOMPARE(getChatMessage(msgs.at(0)), QString("test3"));
		QCOMPARE(lastIdx, 2);

		std::tie(msgs, lastIdx) = fh->getBatch(2);

		QCOMPARE(msgs.size(), 0);
		QCOMPARE(lastIdx, 2);
	}

	// Make sure messages are added correctly to the recording
	// after it has been cached
	void testLoadAppend()
	{
		QString file = makeTestRecording();
		std::unique_ptr<FiledHistory> fh{
			FiledHistory::load(m_dir.absoluteFilePath(file))};

		// Read the whole recording to load it into the cache
		fh->getBatch(-1);

		// Add something to it. This should go to the cache as well
		auto testMsg =
			net::makeChatMessage(1, 0, 0, QStringLiteral("appended"));

		fh->addMessage(testMsg);

		// The recording should now have a length of 4 and the last message
		// should be there
		net::MessageList msgs;
		int lastIdx;
		std::tie(msgs, lastIdx) = fh->getBatch(-1);

		QCOMPARE(msgs.size(), 4);
		QCOMPARE(lastIdx, 3);
		QVERIFY(msgs.last().equals(testMsg));
	}

	// Tolerate truncated messages
	void testTruncation()
	{
		QString file = makeTestRecording();

		// Cut off the end of the recording
		QString recfile = m_dir.absoluteFilePath(file);
		recfile.replace(".session", ".dprec");
		QFile rf(recfile);
		QVERIFY(rf.resize(rf.size() - 3));

		// The first two messages should still be readable
		std::unique_ptr<FiledHistory> fh{
			FiledHistory::load(m_dir.absoluteFilePath(file))};

		net::MessageList msgs;
		int lastIdx;

		std::tie(msgs, lastIdx) = fh->getBatch(-1);

		QCOMPARE(msgs.size(), 2);
		QCOMPARE(getChatMessage(msgs.at(0)), QString("test1"));
		QCOMPARE(getChatMessage(msgs.at(1)), QString("test2"));
		QCOMPARE(lastIdx, 1);
	}

	// Check if history reset is handled correctly
	void testReset()
	{
		QString file = makeTestRecording();
		std::unique_ptr<FiledHistory> fh{
			FiledHistory::load(m_dir.absoluteFilePath(file))};

		auto testMsg = net::makeChatMessage(1, 0, 0, QStringLiteral("test0"));

		fh->addMessage(testMsg);
		fh->addMessage(testMsg);
		fh->addMessage(testMsg);

		QCOMPARE(fh->lastIndex(), 5);
		QCOMPARE(fh->sizeInBytes(), uint(testMsg.length() * 6));

		net::MessageList msgs;
		int lastIdx;
		std::tie(msgs, lastIdx) = fh->getBatch(-1);
		QCOMPARE(msgs.size(), 6);
		QCOMPARE(lastIdx, 5);
		QVERIFY(msgs.at(3).equals(testMsg));

		net::MessageList newContent;
		newContent << testMsg;
		newContent << testMsg;

		fh->reset(newContent);

		QCOMPARE(fh->lastIndex(), 7);
		QCOMPARE(fh->sizeInBytes(), uint(testMsg.length() * 2));

		// any index below firstIndex() should work the same
		std::tie(msgs, lastIdx) = fh->getBatch(1);
		QCOMPARE(msgs.size(), 2);
		QCOMPARE(lastIdx, 7);
		QVERIFY(msgs.at(0).equals(testMsg));
	}

	void testBlockEnd()
	{
		QString file = makeTestRecording();
		std::unique_ptr<FiledHistory> fh{
			FiledHistory::load(m_dir.absoluteFilePath(file))};

		QCOMPARE(fh->lastIndex(), 2);

		fh->closeBlock();

		auto testMsg = net::makeChatMessage(1, 0, 0, QByteArray("test0"));

		fh->addMessage(testMsg);
		fh->addMessage(testMsg);

		QCOMPARE(fh->lastIndex(), 4);

		// First batch should contain the first block
		net::MessageList msgs;
		int lastIdx;
		std::tie(msgs, lastIdx) = fh->getBatch(-1);
		QCOMPARE(msgs.size(), 3);
		QCOMPARE(lastIdx, 2);
		QCOMPARE(getChatMessage(msgs.last()), QString("test3"));

		// Second batch
		std::tie(msgs, lastIdx) = fh->getBatch(lastIdx);
		QCOMPARE(msgs.size(), 2);
		QCOMPARE(lastIdx, 4);
		QCOMPARE(getChatMessage(msgs.first()), QString("test0"));

		// There is no third batch
		std::tie(msgs, lastIdx) = fh->getBatch(lastIdx);
		QCOMPARE(msgs.size(), 0);
		QCOMPARE(lastIdx, 4);

		// Until now
		fh->closeBlock();
		fh->closeBlock(); // closeBlock should be idempotent
		fh->addMessage(testMsg);

		std::tie(msgs, lastIdx) = fh->getBatch(lastIdx);
		QCOMPARE(msgs.size(), 1);
		QCOMPARE(lastIdx, 5);

		// Having an empty block at the end shouldn't have any effect
		fh->closeBlock();
		std::tie(msgs, lastIdx) = fh->getBatch(lastIdx - 1);
		QCOMPARE(msgs.size(), 1);
		QCOMPARE(lastIdx, 5);
	}

	void testUserLeave()
	{
		auto id = Ulid::make().toString();
		{
			std::unique_ptr<FiledHistory> fh{FiledHistory::startNew(
				m_dir, id, QString(), protocol::ProtocolVersion::current(),
				"test")};

			fh->addMessage(
				net::makeJoinMessage(1, 0, QStringLiteral("u1"), QByteArray()));
			fh->addMessage(
				net::makeJoinMessage(2, 0, QStringLiteral("u2"), QByteArray()));
			fh->addMessage(
				net::makeChatMessage(1, 0, 0, QStringLiteral("test1")));
			fh->addMessage(net::makeLeaveMessage(2));
		}
		{
			std::unique_ptr<FiledHistory> fh{FiledHistory::load(
				m_dir.absoluteFilePath(FiledHistory::journalFilename(id)))};
			QVERIFY(fh.get());

			net::MessageList msgs;
			int lastIdx;
			std::tie(msgs, lastIdx) = fh->getBatch(-1);
			QCOMPARE(msgs.size(), 5);
			QCOMPARE(msgs.last().type(), DP_MSG_LEAVE);
			QCOMPARE(msgs.last().contextId(), uint8_t(1));

			// Id Queue should have been updated by the leave events
			QVERIFY(fh->idQueue().nextId() > 2);
		}
	}

private:
	// Generate a test recording containing three messages.
	QString makeTestRecording()
	{
		auto id = Ulid::make().toString();
		std::unique_ptr<FiledHistory> fh{FiledHistory::startNew(
			m_dir, id, QString(), protocol::ProtocolVersion::current(),
			"test")};

		fh->addMessage(net::makeChatMessage(1, 0, 0, QStringLiteral("test1")));
		fh->addMessage(net::makeChatMessage(1, 0, 0, QStringLiteral("test2")));
		fh->addMessage(net::makeChatMessage(1, 0, 0, QStringLiteral("test3")));

		return FiledHistory::journalFilename(id);
	}

private:
	QTemporaryDir m_tempdir;
	QDir m_dir;
};


QTEST_MAIN(TestFiledHistory)
#include "filedhistory.moc"
