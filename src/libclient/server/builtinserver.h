// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_SERVER_BUILTINSERVER_H
#define LIBCLIENT_SERVER_BUILTINSERVER_H
#include "libserver/sessions.h"
#include <QObject>
#include <QVector>

class QTcpServer;

namespace canvas {
class PaintEngine;
}

namespace drawdance {
class CanvasState;
}

namespace sessionlisting {
class Announcements;
}

namespace server {

class BuiltinClient;
class BuiltinSession;
class ServerConfig;

class BuiltinServer final : public QObject, public Sessions {
	Q_OBJECT
public:
	explicit BuiltinServer(
		canvas::PaintEngine *paintEngine, QObject *parent = nullptr);

	BuiltinServer(const BuiltinServer &) = delete;
	BuiltinServer(BuiltinServer &&) = delete;
	BuiltinServer &operator=(const BuiltinServer &) = delete;
	BuiltinServer &operator=(BuiltinServer &&) = delete;

	ServerConfig *config() { return m_config; }

	quint16 port() const;

	QJsonArray sessionDescriptions(bool includeUnlisted) const override;

	Session *getSessionById(const QString &id, bool loadTemplate) override;

	JoinResult checkSessionJoin(
		Client *client, const QString &idOrAlias,
		const QString &inviteSecret) override;

	std::tuple<Session *, QString> createSession(
		const QString &id, const QString &alias,
		const protocol::ProtocolVersion &protocolVersion,
		const QString &founder) override;

	bool start(
		quint16 preferredPort, int clientTimeout, int proxyMode,
		QString *outErrorMessage = nullptr);

public slots:
	void stop();
	void doInternalReset(const drawdance::CanvasState &canvasState);

private slots:
	void newClient();
	void removeClient(BuiltinClient *client);

private:
	ServerConfig *initConfig();
	bool matchesSession(const QString &idOrAlias) const;

	canvas::PaintEngine *m_paintEngine;
	ServerConfig *m_config;
	sessionlisting::Announcements *m_announcements;
	QTcpServer *m_server = nullptr;
	BuiltinSession *m_session = nullptr;
	QVector<BuiltinClient *> m_clients;
};

}

#endif
