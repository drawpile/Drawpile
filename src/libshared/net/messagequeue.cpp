// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/messagequeue.h"
#include "libshared/util/qtcompat.h"
#include <QDateTime>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>
#include <cstring>

namespace net {

MessageQueue::MessageQueue(bool decodeOpaque, QObject *parent)
	: QObject(parent)
	, m_decodeOpaque(decodeOpaque)
	, m_gracefullyDisconnecting(false)
	, m_smoothEnabled(false)
	, m_smoothDrainRate(DEFAULT_SMOOTH_DRAIN_RATE)
	, m_smoothTimer(nullptr)
	, m_smoothMessagesToDrain(INT_MAX)
	, m_contextId(0)
	, m_pingTimer(nullptr)
	, m_idleTimeout(0)
	, m_keepAliveTimeout(0)
	, m_artificialLagMs(0)
	, m_artificialLagTimer(nullptr)
{
	m_idleTimer = new QTimer(this);
	m_idleTimer->setTimerType(Qt::CoarseTimer);
	connect(
		m_idleTimer, &QTimer::timeout, this, &MessageQueue::checkIdleTimeout);
	m_idleTimer->setInterval(1000);
	m_idleTimer->setSingleShot(false);
	resetLastRecvTimer();
	resetKeepAliveTimer();
}

void MessageQueue::setIdleTimeout(qint64 timeout)
{
	m_idleTimeout = timeout;
	resetLastRecvTimer();
	if(timeout > 0) {
		m_idleTimer->start(1000);
	} else {
		m_idleTimer->stop();
	}
}

void MessageQueue::setPingInterval(int msecs)
{
	if(!m_pingTimer) {
		m_pingTimer = new QTimer(this);
		m_pingTimer->setTimerType(Qt::CoarseTimer);
		m_pingTimer->setSingleShot(false);
		connect(m_pingTimer, &QTimer::timeout, this, &MessageQueue::sendPing);
	}
	m_pingTimer->setInterval(msecs);
	m_pingTimer->start(msecs);
}

void MessageQueue::setKeepAliveTimeout(qint64 timeout)
{
	m_keepAliveTimeout = timeout;
	resetKeepAliveTimer();
}

void MessageQueue::setSmoothEnabled(bool enabled)
{
	m_smoothEnabled = enabled;
	updateSmoothing();
}

void MessageQueue::setSmoothDrainRate(int smoothDrainRate)
{
	m_smoothDrainRate = qBound(0, smoothDrainRate, MAX_SMOOTH_DRAIN_RATE);
	updateSmoothing();
}

void MessageQueue::updateSmoothing()
{
	bool enabled = m_smoothEnabled && m_smoothDrainRate > 0;
	if(enabled && !m_smoothTimer) {
		m_smoothTimer = new QTimer{this};
		m_smoothTimer->setTimerType(Qt::PreciseTimer);
		m_smoothTimer->setSingleShot(false);
		m_smoothTimer->setInterval(SMOOTHING_INTERVAL_MSEC);
		connect(
			m_smoothTimer, &QTimer::timeout, this,
			&MessageQueue::receiveSmoothedMessages);
	} else if(!enabled && m_smoothTimer) {
		delete m_smoothTimer;
		m_smoothTimer = nullptr;
		if(!m_smoothBuffer.isEmpty()) {
			m_inbox.append(m_smoothBuffer);
			m_smoothBuffer.clear();
			emit messageAvailable();
		}
	}
}

void MessageQueue::setArtificialLagMs(int msecs)
{
	m_artificialLagMs = qMax(0, msecs);
	if(m_artificialLagMs != 0 && !m_artificialLagTimer) {
		m_artificialLagTimer = new QTimer(this);
		m_artificialLagTimer->setTimerType(Qt::PreciseTimer);
		m_artificialLagTimer->setSingleShot(true);
		connect(
			m_artificialLagTimer, &QTimer::timeout, this,
			&MessageQueue::sendArtificallyLaggedMessages);
	}
}

bool MessageQueue::isPending() const
{
	return !m_inbox.isEmpty();
}

net::Message MessageQueue::shiftPending()
{
	return m_inbox.takeFirst();
}

void MessageQueue::receive(net::MessageList &buffer)
{
	buffer.swap(m_inbox);
}

void MessageQueue::send(const net::Message &msg)
{
	sendMultiple(1, &msg);
}

void MessageQueue::sendMultiple(int count, const net::Message *msgs)
{
	if(m_artificialLagMs == 0) {
		if(!m_gracefullyDisconnecting) {
			resetKeepAliveTimer();
			enqueueMessages(count, msgs);
		}
	} else {
		long long time =
			QDateTime::currentMSecsSinceEpoch() + m_artificialLagMs;
		bool haveKeepAlive = false;
		for(int i = 0; i < count; ++i) {
			const net::Message &msg = msgs[i];
			m_artificialLagTimes.append(time);
			m_artificialLagMessages.append(msg);
			if(msg.type() == DP_MSG_KEEP_ALIVE) {
				haveKeepAlive = true;
			}
		}
		if(haveKeepAlive) {
			resetKeepAliveTimer();
		}
		if(!m_artificialLagTimer->isActive()) {
			m_artificialLagTimer->start(m_artificialLagMs);
		}
	}
}

void MessageQueue::receiveSmoothedMessages()
{
	int count = m_smoothBuffer.size();
	bool smoothTimerActive = m_smoothTimer->isActive();
	if(count == 0) {
		if(smoothTimerActive) {
			m_smoothTimer->stop();
		}
	} else {
		if(m_smoothMessagesToDrain > count) {
			m_inbox.append(m_smoothBuffer);
			m_smoothBuffer.clear();
		} else {
			// We only smoothe strokes, all other messages get flushed along.
			int drainCount = 0;
			int drainBuffer = 0;
			do {
				net::Message msg = m_smoothBuffer.takeFirst();
				if(msg.shouldSmoothe()) {
					drainCount += drainBuffer + 1;
					drainBuffer = 0;
				} else {
					++drainBuffer;
				}
				m_inbox.append(msg);
			} while(drainCount < m_smoothMessagesToDrain &&
					!m_smoothBuffer.isEmpty());
		}

		if(!m_smoothBuffer.isEmpty() && !smoothTimerActive) {
			m_smoothTimer->start();
		}
		emit messageAvailable();
	}
}

void MessageQueue::checkIdleTimeout()
{
	if(getSocketState() == QAbstractSocket::ConnectedState &&
	   m_lastRecvTimer.hasExpired()) {
		emit timedOut(m_idleTimeout);
		abortSocket();
	} else if(m_keepAliveTimer.hasExpired()) {
		send(makeKeepAliveMessage(0));
	}
}

void MessageQueue::sendArtificallyLaggedMessages()
{
	long long now = QDateTime::currentMSecsSinceEpoch();
	int count = m_artificialLagTimes.count();
	int i = 0;
	while(i < count) {
		long long time = m_artificialLagTimes[i];
		if(time <= now) {
			++i;
		} else {
			break;
		}
	}

	if(!m_gracefullyDisconnecting) {
		resetKeepAliveTimer();
		enqueueMessages(i, m_artificialLagMessages.constData());
	}
	m_artificialLagTimes.remove(0, i);
	m_artificialLagMessages.remove(0, i);

	if(i < count) {
		m_artificialLagTimer->start(m_artificialLagTimes.first() - now);
	}
}

void MessageQueue::sendPingMsg(bool pong)
{
	if(m_artificialLagMs == 0) {
		resetKeepAliveTimer();
		enqueuePing(pong);
	} else {
		// Not accurate, but probably good enough for development purposes.
		send(net::makePingMessage(0, pong));
	}
}

void MessageQueue::sendDisconnect(
	GracefulDisconnect reason, const QString &message)
{
	if(m_gracefullyDisconnecting) {
		qWarning("sendDisconnect: already disconnecting.");
	}
	net::Message msg = net::makeDisconnectMessage(0, uint8_t(reason), message);
	send(msg);
	m_gracefullyDisconnecting = true;
	afterDisconnectSent();
	setSmoothEnabled(false);
}

void MessageQueue::sendPing()
{
	if(m_pingSentTimer.isValid()) {
		// This can happen if the other side's upload buffer is too full
		// for the Pong to make it through in time.
		qWarning("sendPing(): reply to previous ping not yet received!");
	} else {
		m_pingSentTimer.start();
	}
	sendPingMsg(false);
}

void MessageQueue::resetLastRecvTimer()
{
	resetDeadlineTimer(m_lastRecvTimer, m_idleTimeout);
}


void MessageQueue::resetKeepAliveTimer()
{
	resetDeadlineTimer(m_keepAliveTimer, m_keepAliveTimeout);
}

void MessageQueue::handlePing(bool isPong)
{
	if(isPong) {
		// We got a Pong back: measure latency
		if(m_pingSentTimer.isValid()) {
			qint64 roundtrip = m_pingSentTimer.elapsed();
			m_pingSentTimer.invalidate();
			emit pingPong(roundtrip);
		} else {
			// Lots of pings can have been queued up
			qDebug("Received Pong, but no Ping was sent!");
		}
	} else {
		// Reply to a Ping with a Pong
		sendPingMsg(true);
	}
}

}
