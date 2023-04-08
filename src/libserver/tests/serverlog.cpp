// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/serverlog.h"

#include <QtTest/QtTest>

using server::Log;
using server::ServerLog;
using server::ServerLogQuery;
using server::InMemoryLog;

class TestServerLog final : public QObject
{
	Q_OBJECT
private slots:
	void testUserSessionLogEntry()
	{
		Log e = Log()
			.about(Log::Level::Info, Log::Topic::Join)
			.session("12345678-1234-1234-1234-123456789abc")
			.user(1,QHostAddress("192.168.1.1"), "My Test Username")
			.message("Test message!")
				;

		QString estr = e.toString(true);
		QCOMPARE(estr, QString("Join 1;192.168.1.1;My Test Username@12345678-1234-1234-1234-123456789abc: Test message!"));
	}

	void testSessionLogEntry()
	{
		Log e = Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.session("{12345678-1234-1234-1234-123456789abc}")
			.message("Test message!")
				;

		QString estr = e.toString(true);
		QCOMPARE(estr, QString("Status {12345678-1234-1234-1234-123456789abc}: Test message!"));
	}

	void testUserLogEntry()
	{
		Log e = Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.user(1,QHostAddress("192.168.1.1"), "My Test Username")
			.message("Test message!")
				;

		QString estr = e.toString(true);
		QCOMPARE(estr, QString("Status 1;192.168.1.1;My Test Username: Test message!"));
	}

	void testInMemoryHistory() {
		InMemoryLog srvlog;
		srvlog.setSilent(true);

		auto session = Ulid::make().toString();

		srvlog.logMessage(Log().message("Hello 1"));
		srvlog.logMessage(Log().session(session).message("Hello 2"));
		srvlog.logMessage(Log().session(session).user(1, QHostAddress("192.168.1.1"), "test").message("Hello 3"));
		srvlog.logMessage(Log().message("Hello 4"));

		QCOMPARE(srvlog.query().get().size(), 4);

		srvlog.setHistoryLimit(3);

		QCOMPARE(srvlog.query().get().size(), 3);
		QCOMPARE(srvlog.query().session(session).get().size(), 2);
	}
};


QTEST_MAIN(TestServerLog)
#include "serverlog.moc"

