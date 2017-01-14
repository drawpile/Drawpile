#include "../server/sessionban.h"

#include <QtTest/QtTest>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>

using server::SessionBanList;

class TestSessionBan: public QObject
{
	Q_OBJECT
private slots:
	void testAddRemove()
	{
		SessionBanList bans;
		const int id1 = bans.addBan("User1", QHostAddress("192.168.0.100"), "op1");
		QVERIFY(id1>0);
		const int id2 = bans.addBan("User2", QHostAddress("192.168.0.101"), "op2");
		QVERIFY(id2>0);

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
	}

	void testCustomid()
	{
		SessionBanList bans;
		// auto ID
		const int id1 = bans.addBan("1", QHostAddress("192.168.0.100"), "op");
		// specified ID
		const int id2 = bans.addBan("2", QHostAddress("192.168.0.101"), "op", 2);
		// next auto ID must be larger than the largest ID used so far
		const int id3 = bans.addBan("3", QHostAddress("192.168.0.102"), "op");
		// existing ID overwrite not allowed. Returns zero
		const int id4 = bans.addBan("4", QHostAddress("192.168.0.103"), "op", 2);

		QVERIFY(id1>0);
		QCOMPARE(id2, 2);
		QVERIFY(id3>2);
		QCOMPARE(id4, 0);
	}

	void testIpMatching()
	{
		SessionBanList bans;
		bans.addBan("User", QHostAddress("192.168.0.100"), "op");

		QCOMPARE(bans.isBanned(QHostAddress("192.168.0.100")), true);
		QCOMPARE(bans.isBanned(QHostAddress("::ffff:192.168.0.100")), true);
		QCOMPARE(bans.isBanned(QHostAddress("192.168.0.101")), false);
	}

};


QTEST_MAIN(TestSessionBan)
#include "sessionban.moc"

