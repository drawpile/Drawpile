// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/sessionban.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QtTest>

using server::SessionBanList;

class TestSessionBan final : public QObject {
	Q_OBJECT
private slots:
	void testAddRemove()
	{
		SessionBanList bans;
		const int id1 = bans.addBan(
			"User1", QHostAddress("192.168.0.100"), QString(), QString(),
			"op1");
		QVERIFY(id1 > 0);
		const int id2 = bans.addBan(
			"User2", QHostAddress("192.168.0.101"), QString(), QString(),
			"op2");
		QVERIFY(id2 > 0);

		QJsonArray js = bans.toJson(false);
		QCOMPARE(js.size(), 2);
		QCOMPARE(js[0].toObject()["username"].toString(), QString("User1"));
		QCOMPARE(js[0].toObject()["bannedBy"].toString(), QString("op1"));
		QCOMPARE(js[1].toObject()["username"].toString(), QString("User2"));
		QCOMPARE(js[1].toObject()["bannedBy"].toString(), QString("op2"));

		bans.removeBan(id2);

		js = bans.toJson(false);
		QCOMPARE(js.size(), 1);
		QCOMPARE(js[0].toObject()["username"].toString(), QString("User1"));

		// Can't add an identical ban.
		QCOMPARE(
			bans.addBan(
				"User1", QHostAddress("192.168.0.100"), QString(), QString(),
				"op1"),
			0);
		// Can't add null ban.
		QCOMPARE(
			bans.addBan(QString(), QHostAddress(), QString(), QString(), "op1"),
			0);
		// Localhost can't be banned, so this is a null ban too.
		QCOMPARE(
			bans.addBan(
				QString(), QHostAddress("::1"), QString(), QString(), "op1"),
			0);
		// No further bans added.
		js = bans.toJson(false);
		QCOMPARE(js.size(), 1);
		QCOMPARE(js[0].toObject()["username"].toString(), QString("User1"));
	}

	void testBanLimit()
	{
		SessionBanList bans;
		for(int i = 1; i <= 1000; ++i) {
			QCOMPARE(
				bans.addBan(
					QString::number(i), QHostAddress(), QString(), QString(),
					QStringLiteral("op")),
				i);
		}
		QCOMPARE(
			bans.addBan(
				QString::number(1001), QHostAddress(), QString(), QString(),
				QStringLiteral("op")),
			0);
	}

	void testCustomid()
	{
		SessionBanList bans;
		// auto ID
		const int id1 = bans.addBan(
			"1", QHostAddress("192.168.0.100"), QString(), QString(), "op");
		// specified ID
		const int id2 = bans.addBan(
			"2", QHostAddress("192.168.0.101"), QString(), QString(), "op", 2);
		// next auto ID must be larger than the largest ID used so far
		const int id3 = bans.addBan(
			"3", QHostAddress("192.168.0.102"), QString(), QString(), "op");
		// existing ID overwrite not allowed. Returns zero
		const int id4 = bans.addBan(
			"4", QHostAddress("192.168.0.103"), QString(), QString(), "op", 2);

		QVERIFY(id1 > 0);
		QCOMPARE(id2, 2);
		QVERIFY(id3 > 2);
		QCOMPARE(id4, 0);
	}

	void testIpMatching()
	{
		SessionBanList bans;
		bans.addBan(
			"User", QHostAddress("192.168.0.100"), QString(), "op", QString());

		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.100"), QString(), QString()),
			1);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("::ffff:192.168.0.100"), QString(),
				QString()),
			1);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.101"), QString(), QString()),
			0);
		QCOMPARE(
			bans.isBanned(QString(), QHostAddress("::1"), QString(), QString()),
			0);
	}

	void testExtAuthIds()
	{
		SessionBanList bans;
		bans.addBan(
			"User", QHostAddress("192.168.0.101"), "1", QString(), "op");

		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.100"), QString(), QString()),
			0);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.100"), "000", QString()),
			0);
		// Banned by ip address.
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.101"), QString(), QString()),
			1);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.101"), "000", QString()),
			1);
		// Banned by auth id.
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.101"), "1", QString()),
			1);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.103"), "1", QString()),
			1);
	}

	void testBanMatching()
	{
		SessionBanList bans;
		bans.addBan("a", QHostAddress("192.168.0.1"), "ext:1", "def1", "op", 1);
		bans.addBan("b", QHostAddress("192.168.0.2"), "ext:2", "def2", "op", 2);
		bans.addBan("c", QHostAddress(), QString(), QString(), "op", 3);
		bans.addBan(
			QString(), QHostAddress("192.168.0.4"), QString(), QString(), "op",
			4);
		bans.addBan(QString(), QHostAddress(), "ext:5", QString(), "op", 5);
		bans.addBan(QString(), QHostAddress(), QString(), "def6", "op", 6);

		QCOMPARE(bans.isBanned("a", QHostAddress(), QString(), QString()), 1);
		QCOMPARE(
			bans.isBanned("b", QHostAddress("192.168.0.2"), "ext:2", "def2"),
			2);
		QCOMPARE(bans.isBanned("c", QHostAddress(), QString(), QString()), 3);
		QCOMPARE(
			bans.isBanned(
				QString(), QHostAddress("192.168.0.4"), QString(), QString()),
			4);
		QCOMPARE(
			bans.isBanned(QString(), QHostAddress(), "ext:5", QString()), 5);
		QCOMPARE(
			bans.isBanned(QString(), QHostAddress(), QString(), "def6"), 6);
		QCOMPARE(
			bans.isBanned("g", QHostAddress("192.168.0.7"), "ext:7", "def7"),
			0);
	}

	void testPreventSelfBans()
	{
		SessionBanList bans;
		server::SessionBanner banner = {
			"name",
			"ext:1",
			QHostAddress("192.168.0.1"),
			"def0",
		};

		// Adding identical ban causes it to be null and not added.
		QCOMPARE(
			bans.addBan(
				banner.username, banner.address, banner.authId, banner.sid,
				banner.username, 0, &banner),
			0);
		QCOMPARE(bans.toJson(false).size(), 0);
		QCOMPARE(
			bans.isBanned(
				banner.username, banner.address, banner.authId, banner.sid),
			0);

		// Adding partially equal parameters leads to those parts being added.
		QCOMPARE(
			bans.addBan(
				"fred", banner.address, banner.authId, banner.sid,
				banner.username, 0, &banner),
			1);
		QCOMPARE(
			bans.addBan(
				banner.username, QHostAddress("192.168.0.2"), "ext:2", "def1",
				banner.username, 0, &banner),
			2);

		QCOMPARE(bans.toJson(false).size(), 2);
		QCOMPARE(
			bans.isBanned(
				banner.username, banner.address, banner.authId, banner.sid),
			0);
		QCOMPARE(
			bans.isBanned("fred", QHostAddress("192.168.0.2"), "ext:2", "def1"),
			1);
		QCOMPARE(
			bans.isBanned(
				banner.username, QHostAddress("192.168.0.2"), "ext:2", "def1"),
			2);
	}
};


QTEST_MAIN(TestSessionBan)
#include "sessionban.moc"
