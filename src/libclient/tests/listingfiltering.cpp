// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/sessionlistingmodel.h"
#include "libclient/net/loginsessions.h"
#include "libclient/utils/sessionfilterproxymodel.h"

#include <QtTest/QtTest>

class TestListingFiltering final : public QObject
{
	Q_OBJECT
private slots:
	void testSessionListing()
	{
		auto testlist = QVector<sessionlisting::Session> {
			listsession("test1", false, false),
			listsession("test2", false, false),
			listsession("test3", true, false),
			listsession("test4", false, false),
			listsession("test5", false, true),
			listsession("test6", false, false),
		};

		SessionListingModel listmodel;
		listmodel.setList("test", "example.com", testlist);

		SessionFilterProxyModel filtered;
		filtered.setSourceModel(&listmodel);
		filtered.setFilterKeyColumn(-1);

		const QModelIndex root = listmodel.index(0, 0);
		const QModelIndex filteredRoot = filtered.index(0, 0);

		QCOMPARE(listmodel.rowCount(root), 6);
		QCOMPARE(filtered.rowCount(filteredRoot), 6);

		filtered.setShowNsfw(false);
		QCOMPARE(filtered.rowCount(filteredRoot), 5);

		filtered.setShowPassworded(false);
		QCOMPARE(filtered.rowCount(filteredRoot), 4);

		// Standard filtering can be combined with the new options too
		filtered.setFilterFixedString("5");
		filtered.setShowNsfw(true);
		QCOMPARE(filtered.rowCount(filteredRoot), 1);
	}

	void testLoginSessions()
	{
		net::LoginSessionModel listmodel;

		listmodel.updateSession(loginsession("test1", false, false));
		listmodel.updateSession(loginsession("test2", false, false));
		listmodel.updateSession(loginsession("test3", true, false));
		listmodel.updateSession(loginsession("test4", false, false));
		listmodel.updateSession(loginsession("test5", false, true));
		listmodel.updateSession(loginsession("test6", false, false));

		SessionFilterProxyModel filtered;
		filtered.setFilterKeyColumn(-1);
		filtered.setSourceModel(&listmodel);

		QCOMPARE(listmodel.rowCount(), 6);
		QCOMPARE(filtered.rowCount(), 6);

		filtered.setShowNsfw(false);
		QCOMPARE(filtered.rowCount(), 5);

		filtered.setShowPassworded(false);
		QCOMPARE(filtered.rowCount(), 4);

		// Standard filtering can be combined with the new options too
		filtered.setFilterFixedString("5");
		filtered.setShowNsfw(true);
		QCOMPARE(filtered.rowCount(), 1);
	}

private:
	static sessionlisting::Session listsession(const QString &title, bool password, bool nsfm)
	{
		return sessionlisting::Session {
			"example.com",
			27750,
			"abc1",
			protocol::ProtocolVersion("dp", 2, 1, 0),
			title,
			1,
			QStringList(),
			password,
			nsfm,
			sessionlisting::PrivacyMode::Public,
			QString(),
			QDateTime(),
			254,
			false,
		};
	}

	static net::LoginSession loginsession(const QString &id, bool password, bool nsfm)
	{
		return net::LoginSession {
			id,
			QString(),
			"Title: " + id,
			QString(),
			QString(),
			false,
			0,
			password,
			false,
			false,
			false,
			nsfm
		};
	}
};

QTEST_MAIN(TestListingFiltering)
#include "listingfiltering.moc"

