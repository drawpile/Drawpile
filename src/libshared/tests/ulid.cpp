#include "libshared/util/ulid.h"

#include <QtTest/QtTest>
#include <QDateTime>

class TestUlid final : public QObject
{
	Q_OBJECT
private slots:
	void testUlid()
	{
		const auto before = QDateTime::currentDateTime().addMSecs(-1);
		const auto now = QDateTime::currentDateTime();
		const auto after = QDateTime::currentDateTime().addMSecs(1);

		QVERIFY(Ulid().isNull());
		QVERIFY(Ulid("invalid").isNull());

		auto test = Ulid::make(now);
		QVERIFY(!test.isNull());
		QCOMPARE(test.timestamp(), now);

		QVERIFY(Ulid::make(now) != Ulid::make(now));

		QVERIFY(test == Ulid(test.toString().toUpper()));
		QVERIFY(test == Ulid(test.toString().toLower()));
		QVERIFY(Ulid::make(before) < test);
		QVERIFY(Ulid::make(after) > test);
	}
};


QTEST_MAIN(TestUlid)
#include "ulid.moc"

