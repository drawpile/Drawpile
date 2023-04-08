// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/localserver.h"
#include "thinsrv/multiserver.h"
#include "libserver/sessionserver.h"
#include "libserver/serverconfig.h"
#include "libshared/util/whatismyip.h"

#include <QSettings>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QApplication>

namespace server {
namespace gui {

LocalServer::LocalServer(MultiServer *server, QObject *parent)
	: Server(parent), m_server(server)
{
	Q_ASSERT(server);

	connect(server, &MultiServer::serverStartError, this, &LocalServer::serverError);
	connect(server, &MultiServer::serverStartError, this, &LocalServer::onStartStop);
	connect(server, &MultiServer::serverStarted, this, &LocalServer::onStartStop);
	connect(server, &MultiServer::serverStopped, this, &LocalServer::onStartStop);
	connect(server, &MultiServer::jsonApiResult, this, &LocalServer::onApiResponse);
}

void LocalServer::onStartStop()
{
	emit serverStateChanged(isRunning());
}

QString LocalServer::address() const
{
	QString addr = m_server->sessionServer()->config()->internalConfig().localHostname;
	if(addr.isEmpty())
		addr = WhatIsMyIp::instance()->myAddress();
	return addr;
}

int LocalServer::port() const
{
	int p = m_server->port();
	if(p==0)
		p = QSettings().value("guiserver/port", "27750").toInt();
	return p;
}

bool LocalServer::isRunning() const
{
	bool result;
	QMetaObject::invokeMethod(
		m_server, "isRunning", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(bool, result)
		);
	return result;
}

void LocalServer::startServer()
{
	if(isRunning()) {
		qWarning("Tried to start a server that was already running!");
		return;
	}

	// These settings are safe to set from another thread when the server isn't running
	QSettings cfg;
	cfg.beginGroup("guiserver");

	if(cfg.value("use-ssl", false).toBool())
		m_server->setSslCertFile(cfg.value("sslcert").toString(), cfg.value("sslkey").toString());
	else
		m_server->setSslCertFile(QString(), QString());

	InternalConfig icfg = m_server->config()->internalConfig();
	icfg.localHostname = cfg.value("local-address").toString();
#ifdef HAVE_LIBSODIUM
	icfg.extAuthUrl = cfg.value("extauth").toString();
#endif

	m_server->config()->setInternalConfig(icfg);

	if(cfg.value("session-storage").toString() == "file") {
		QDir sessionDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/sessions";
		if(!sessionDir.mkpath(".")) {
			qWarning("Couldn't create session directory");
		} else {
			m_server->setSessionDirectory(sessionDir);
		}
	}

	// Start the server
	quint16 port = cfg.value("port", 27750).toInt();

	QMetaObject::invokeMethod(
		m_server, "start", Qt::QueuedConnection,
		Q_ARG(quint16, port)
		);
}

void LocalServer::stopServer()
{
	// Calling stop is safe in any state
	QMetaObject::invokeMethod(m_server, "stop", Qt::QueuedConnection);
}

void LocalServer::makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	// Note: we can call the internal server's JSON API even when the server is stopped
	QMetaObject::invokeMethod(
		m_server, "callJsonApiAsync", Qt::QueuedConnection,
		Q_ARG(QString, requestId),
		Q_ARG(JsonApiMethod, method),
		Q_ARG(QStringList, path),
		Q_ARG(QJsonObject, request)
		);
}

void LocalServer::confirmQuit()
{
	QMessageBox box(QMessageBox::Question, tr("Drawpile Server"), tr("The server is still running."));
	QPushButton *quit = box.addButton(QMessageBox::Yes);
	QPushButton *cancel = box.addButton(QMessageBox::Cancel);
	quit->setText(tr("Stop server"));

	box.setDefaultButton(cancel);

	if(box.exec() == QMessageBox::Yes) {
		stopServer();
		qApp->exit();
	}
}

}
}
