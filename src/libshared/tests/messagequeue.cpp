#include "libshared/net/messagequeue.h"
#include "libshared/net/meta.h"

#include <QtTest/QtTest>
#include <QTcpSocket>
#include <memory>

#include <QMutex>
#include <QThread>
#include <QTcpServer>
#include <QDebug>

using namespace protocol;

// A simple TCP server that echoes back whatever is written to it.
// Used by the actual test case.
class EchoServer : public QObject
{
	Q_OBJECT
public:
	EchoServer() :
		m_thread(nullptr), m_server(nullptr), m_port(0)
	{
	}

	int port() const { return m_port; }

	bool listen()
	{
		m_server = new QTcpServer(this);
		
		if(!m_server->listen(QHostAddress::LocalHost)) {
			qWarning() << "Couldn't start echo server:" << m_server->errorString();
			return false;
		}
		connect(m_server, &QTcpServer::newConnection, this, &EchoServer::acceptNew);
		m_port = m_server->serverPort();
		return true;
	}

	static EchoServer *startInAnotherThread()
	{
		QThread *thread = new QThread;
		EchoServer *server;

		QMutex mutex;

		mutex.lock();

		connect(thread, &QThread::started, [&mutex, &server, thread]() {
			server = new EchoServer;
			server->m_thread = thread;
			if(!server->listen()) {
				server = nullptr;
			} else {
				connect(thread, &QThread::finished, server, &EchoServer::deleteLater);
			}

			mutex.unlock();
		});

		thread->start();

		mutex.lock();
		mutex.unlock();

		return server;
	}

	void stopInAnotherThread()
	{
		m_thread->quit();
		m_thread->wait();
	}

private slots:
	void acceptNew()
	{
		QTcpSocket *s;
		while((s=m_server->nextPendingConnection())) {
			connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
			connect(s, &QTcpSocket::readyRead, [s]() {
				char buf[128];
				qint64 readbytes;
				while((readbytes=s->read(buf, sizeof(buf)))>0) {
					s->write(buf, readbytes);
				}
			});
		}
	}

private:
	QThread *m_thread;
	QTcpServer *m_server;
	int m_port;
};


// The actual test case
class TestMessageQueue : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_server = EchoServer::startInAnotherThread();
		QVERIFY(m_server);
	}

	void cleanupTestCase()
	{
		m_server->stopInAnotherThread();
	}

	void selftest() {
		auto s = getConnection();
		s->write("Hello");
		s->waitForReadyRead();
		QByteArray read = s->readAll();
		QCOMPARE(read, QByteArray("Hello"));
	}

	void testSend()
	{
		auto mq = getMsgQueue();

		MessagePtr msg(new Chat(0, 0, 0, QByteArray("Hello world!")));

		bool messageReceived = false;

		connect(mq.get(), &MessageQueue::messageAvailable, [&mq, msg, &messageReceived]() {
			MessagePtr got = mq->getPending();
			messageReceived = true;
			QVERIFY(got.equals(msg));
		});
		mq->send(msg);
		loopUntil(messageReceived);
	}

	void testMultiSend()
	{
		auto mq = getMsgQueue();

		const int sendCount = 100;

		int countReceived = 0;
		bool allReceived = false;

		connect(mq.get(), &MessageQueue::messageAvailable, [&mq, sendCount, &countReceived, &allReceived]() {
			while(mq->isPending()) {
				MessagePtr got = mq->getPending();
				// Expect to receive the messages in the same order as we sent them
				QCOMPARE(got->type(), MSG_CHAT);
				QCOMPARE(got.cast<Chat>().message(), QString::number(countReceived));
				if(++countReceived == sendCount)
					allReceived = true;
				QVERIFY(countReceived <= sendCount);
			}
		});

		int totalSendLen = 0;
		for(int i=0;i<sendCount;++i) {
			MessagePtr msg(new Chat(0, 0, 0, QByteArray::number(i)));
			totalSendLen += msg->length();
			mq->send(msg);
		}

		QVERIFY(mq->isUploading());
		QCOMPARE(mq->uploadQueueBytes(), totalSendLen);

		loopUntil(allReceived);
	}

	void testSendDisconnect()
	{
		auto s = getConnection();
		MessageQueue mq(s.get());
		
		bool disconnected = false;

		connect(s.get(), &QTcpSocket::disconnected, [&disconnected]() {
			disconnected = true;
		});

		// Note: because the connection is cut just after the disconnect message
		// is sent, we can't test the reception of the message with an echo server.
		mq.sendDisconnect(0, "test");

		loopUntil(disconnected);
	}

private:
	std::unique_ptr<QTcpSocket> getConnection()
	{
		std::unique_ptr<QTcpSocket> s { new QTcpSocket };
		s->connectToHost(QHostAddress::LocalHost, m_server->port());
		return s;
	}

	std::unique_ptr<MessageQueue> getMsgQueue()
	{
		auto s = getConnection();
		std::unique_ptr<MessageQueue> q { new MessageQueue(s.get()) };
		s->setParent(q.get());
		s.release();
		return q;
	}

	void loopUntil(bool &condition) {
		const int timeout = 3000;
		QElapsedTimer t;
		t.start();
		while(!condition && t.elapsed() < timeout) {
			QCoreApplication::processEvents();
		}
		QVERIFY(t.elapsed() < timeout);
	}

	EchoServer *m_server;
};


QTEST_MAIN(TestMessageQueue)
#include "messagequeue.moc"

