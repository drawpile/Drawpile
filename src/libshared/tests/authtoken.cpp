#include "libshared/util/authtoken.h"

#include <QtTest/QtTest>

static const char *SAT_SAMPLE =
	"1.eyJ1c2VybmFtZSI6ICJib2IiLCAiZmxhZ3MiOiBbIm1vZCIsICJob3N0Il0sICJub25jZSI6ICIwMTAyMDMwNDA1MDYwNzA4In0=.700X+2Jy0Cor6M08Qd+ZGXcWdfjo1ZHzhldlhGbtJOwv7ZSHy7ykmtr2BiOOt+j4XFGRTT9ebA7F5/+h4osrCg";
static const char *PUBKEY =
	"PUi/Xp04e2C7bk0BNFyAp0IPAiHG8nEkMKEcXo44qnY=";

class TestAuthToken : public QObject
{
	Q_OBJECT
private slots:
	void testPasswordChecking_data()
	{
		QTest::addColumn<QByteArray>("token");
		QTest::addColumn<QByteArray>("pubkey");
		QTest::addColumn<bool>("valid");
		QTest::addColumn<bool>("sigmatch");
		QTest::addColumn<quint64>("nonce");
		QTest::addColumn<QString>("username");
		QTest::addColumn<bool>("validPayload");

		QByteArray pubkey = QByteArray::fromBase64(PUBKEY);
		QTest::newRow("blank") << QByteArray() << QByteArray() << false << false << quint64(0) << QString() << false;
		QTest::newRow("blank") << QByteArray() << pubkey << false << false << quint64(0) << QString() << false;
		QTest::newRow("blankKey") << QByteArray(SAT_SAMPLE) << QByteArray() << true << false << quint64(0) << QString() << false;
		QTest::newRow("sigmatch") << QByteArray(SAT_SAMPLE) << pubkey << true << true << quint64(0) << "bob" << false;
		QTest::newRow("valid") << QByteArray(SAT_SAMPLE) << pubkey << true << true << quint64(0x0102030405060708) << "bob" << true;
	}

	void testPasswordChecking()
	{
		QFETCH(QByteArray, token);
		QFETCH(QByteArray, pubkey);
		QFETCH(bool, valid);
		QFETCH(bool, sigmatch);
		QFETCH(quint64, nonce);
		QFETCH(QString, username);
		QFETCH(bool, validPayload);

		server::AuthToken sat(token);

		QCOMPARE(sat.isValid(), valid);
		QCOMPARE(sat.checkSignature(pubkey), sigmatch);
		if(sigmatch) {
			QCOMPARE(sat.validatePayload(QString(), nonce), validPayload);
			QCOMPARE(sat.payload()["username"].toString(), username);
		}
	}
};


QTEST_MAIN(TestAuthToken)
#include "authtoken.moc"

