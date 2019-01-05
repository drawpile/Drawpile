#include "../net/sessionlistingmodel.h"
#include "../net/loginsessions.h"
#include "../utils/sessionfilterproxymodel.h"

#include <QtTest/QtTest>

class TestListingFiltering : public QObject
{
	Q_OBJECT
private slots:
	void testSessionListing()
	{
		auto testlist = QList<sessionlisting::Session> {
			listsession("test1", false, false),
			listsession("test2", false, false),
			listsession("test3", true, false),
			listsession("test4", false, false),
			listsession("test5", false, true),
			listsession("test6", false, false),
		};

		SessionListingModel listmodel;
		listmodel.setList(testlist);

		SessionFilterProxyModel filtered;
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
			QDateTime()
		};
	}

	static net::LoginSession loginsession(const QString &id, bool password, bool nsfm)
	{
		return net::LoginSession {
			id,
			QString(),
			"Title: " + id,
			QString(),
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

