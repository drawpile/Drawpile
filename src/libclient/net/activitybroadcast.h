// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_NET_ACTIVITYBROADCAST_H
#define LIBCLIENT_NET_ACTIVITYBROADCAST_H
#ifndef DP_HAVE_ACTIVITYBROADCAST
#	error "DP_HAVE_ACTIVITYBROADCAST undefined, should not include this header"
#endif
#include <QColor>
#include <QObject>

class QJsonObject;
class QString;
class QUdpSocket;

namespace brushes {
class ActiveBrush;
}

namespace net {

class ActivityBroadcast final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ActivityBroadcast)
public:
	explicit ActivityBroadcast(QObject *parent = nullptr);

	bool start(int port, QString *outErrorMessage = nullptr);
	bool stop();

	void sendActiveBrush(const brushes::ActiveBrush &brush);
	void sendActiveTool(int type);
	void sendForegroundColor(const QColor &color);
	void sendPenDown(
		const QPointF &viewPos, const QRectF &viewArea, qreal pressure,
		qreal xtilt, qreal ytilt, qreal rotation);
	void sendPenMove(
		const QPointF &viewPos, const QRectF &viewArea, qreal pressure,
		qreal xtilt, qreal ytilt, qreal rotation);
	void sendPenHover(const QPointF &viewPos, const QRectF &viewArea);
	void sendPenUp(const QRectF &viewArea);

Q_SIGNALS:
	void activityBroadcasted(const QByteArray &bytes);

private:
	void sendData(const QString &type, const QJsonObject &json);

	QUdpSocket *m_socket = nullptr;
	int m_port = -1;
};

}

#endif
