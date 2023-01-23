#include "libserver/idqueue.h"

#include <QtTest/QtTest>

using server::IdQueue;

class TestIdQueue: public QObject
{
	Q_OBJECT
private slots:
	void testIdQueue()
	{
		QVERIFY(IdQueue::FIRST_ID == 1);
		IdQueue idq;

		QCOMPARE(idq.nextId(), uint8_t(1));
		QCOMPARE(idq.nextId(), uint8_t(2));
		idq.reserveId(3);
		QCOMPARE(idq.nextId(), uint8_t(4));
		idq.setIdForName(5, "hello");
		QCOMPARE(idq.nextId(), uint8_t(6));
		QCOMPARE(idq.getIdForName("hello"), uint8_t(5));
		QCOMPARE(idq.getIdForName("world"), uint8_t(0));
	}
};


QTEST_MAIN(TestIdQueue)
#include "idqueue.moc"

