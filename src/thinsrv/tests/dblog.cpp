// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/database.h"
#include "thinsrv/dblog.h"

#include <QtTest/QtTest>

using server::Database;
using server::DbLog;
using server::Log;

class TestDbLog final : public QObject
{
	Q_OBJECT
private slots:
	void init()
	{
		m_db.reset(new Database);
		QVERIFY(m_db->openFile(":memory:"));

		logger = dynamic_cast<DbLog*>(m_db->logger());
		QVERIFY(logger);
		logger->setSilent(true);
	}

	void testLogEntryPurging()
	{
		const QDateTime now = QDateTime::currentDateTimeUtc();

		logger->logMessage(Log(now.addDays(-1), QString(), "test", Log::Level::Info, Log::Topic::Status, "day 1"));
		logger->logMessage(Log(now.addDays(-2), QString(), "test", Log::Level::Info, Log::Topic::Status, "day 2"));
		logger->logMessage(Log(now.addDays(-2), QString(), "test", Log::Level::Info, Log::Topic::Status, "day 2.2"));
		logger->logMessage(Log(now.addDays(-3), QString(), "test", Log::Level::Info, Log::Topic::Status, "day 3"));

		QCOMPARE(logEntryCount(), 4);

		// Oldest log entry is 3 days old, we try to purge entries older than that
		QCOMPARE(logger->purgeLogs(3), 0);
		QCOMPARE(logEntryCount(), 4);

		// There is one entry older than 2 days
		QCOMPARE(logger->purgeLogs(2), 1);
		QCOMPARE(logEntryCount(), 3);

		// And three entries older than one day (just two after the last purge)
		QCOMPARE(logger->purgeLogs(1), 2);
		QCOMPARE(logEntryCount(), 1);
	}

private:
	int logEntryCount()
	{
		return logger->getLogEntries(QString(), QDateTime(), Log::Level::Debug, 0, 0).size();
	}

	QScopedPointer<Database> m_db;
	DbLog *logger;
};


QTEST_MAIN(TestDbLog)
#include "dblog.moc"

