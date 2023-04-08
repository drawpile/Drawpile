// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/webadmin/webadmin.h"
#include "thinsrv/webadmin/qmhttp.h"
#include "thinsrv/multiserver.h"

#include "libserver/jsonapi.h"
#include "libshared/util/qtcompat.h"

#include <QJsonObject>
#include <QMetaObject>
#include <QDir>
#include <QtGlobal>

namespace server {

Webadmin::Webadmin(QObject *parent)
	: QObject(parent), m_server(new MicroHttpd(this)), m_mode(NOTSTARTED)
{
}

void Webadmin::setBasicAuth(const QString &userpass)
{
	int sep = userpass.indexOf(':');
	if(sep<1) {
		qWarning("Invalid user:password parit for web admin: %s", qPrintable(userpass));
	} else {
		QString user = userpass.left(sep);
		QString passwd = userpass.mid(sep+1);
		m_server->setBasicAuth("drawpile-srv", user, passwd);
	}
}

void Webadmin::setSessions(MultiServer *server)
{
	m_server->addRequestHandler("^/api/(.*)", [server](const HttpRequest &req) {
		JsonApiMethod m;
		switch(req.method()) {
		case HttpRequest::HEAD:
		case HttpRequest::GET:
			m = JsonApiMethod::Get;
			break;
		case HttpRequest::POST:
			m = JsonApiMethod::Create;
			break;
		case HttpRequest::PUT:
			m = JsonApiMethod::Update;
			break;
		case HttpRequest::DELETE:
			m = JsonApiMethod::Delete;
			break;
		default:
			return HttpResponse::JsonErrorResponse("Unknown request method", 400);
		}

		const QStringList path = req.pathMatch().captured(1).split('/', compat::SkipEmptyParts);

		QJsonDocument reqBodyDoc;
		if(m == JsonApiMethod::Get) {
			QJsonObject params;
			QHashIterator<QString, QString> i(req.getData());
			while(i.hasNext()) {
				i.next();
				params[i.key()] = i.value();
			}
			reqBodyDoc = QJsonDocument(params);

		} else if(m == JsonApiMethod::Create|| m == JsonApiMethod::Update) {
			if(req.headers()["content-type"] != "application/json") {
				return HttpResponse::JsonErrorResponse("Content-Type should be application/json", 400);
			}

			QJsonParseError parseError;
			reqBodyDoc = QJsonDocument::fromJson(req.body(), &parseError);

			if(parseError.error != QJsonParseError::NoError)
				return HttpResponse::JsonErrorResponse(parseError.errorString(), 400);
		}

		JsonApiResult result;

		// The HTTP server runs in another thread, so we can't
		// call the main server directly
		QMetaObject::invokeMethod(
			server, "callJsonApi", Qt::BlockingQueuedConnection,
			Q_RETURN_ARG(JsonApiResult, result),
			Q_ARG(JsonApiMethod, m),
			Q_ARG(QStringList, path),
			Q_ARG(QJsonObject, reqBodyDoc.object())
			);

		return HttpResponse::JsonResponse(result.body, result.status);
	});
}

void Webadmin::setStaticFileRoot(const QDir &dir)
{
	m_server->addRequestHandler("^/", [dir](const HttpRequest &req) {
		if(req.method() != HttpRequest::HEAD && req.method() != HttpRequest::GET)
			return HttpResponse::MethodNotAllowed(QStringList() << "HEAD" << "GET");

		// Note: microhttpd sanitizes the request path for us.
		auto path = req.path();

		if(path.endsWith('/')) {
			if(path.length() > 1) {
				QFileInfo indexhtml { dir, "." + req.path() + "index.html" };
				if(indexhtml.exists()) {
					return HttpResponse::FileResponse(
						indexhtml.absoluteFilePath(),
						req.method() == HttpRequest::HEAD
						);
				}
			}

			// Fall back to root index HTML
			path = "/index.html";
		}

		QFileInfo f { dir, "." + path };
		if(f.exists())
			return HttpResponse::FileResponse(
				f.absoluteFilePath(),
				req.method() == HttpRequest::HEAD
				);

		return HttpResponse::NotFound();
	});
}

bool Webadmin::setAccessSubnet(const QString &access)
{
	if(access == "all") {
		// special value
		m_server->setAcceptPolicy([](const QHostAddress &) { return true; });
	} else {
		QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(access);
		if(subnet.first.isNull())
			return false;

		m_server->setAcceptPolicy([subnet](const QHostAddress &addr) {
			if(addr.isLoopback())
				return true;

			return addr.isInSubnet(subnet);
		});
	}
	return true;
}

void Webadmin::start(quint16 port)
{
	m_port = port;
	m_mode = PORT;
	m_server->listen(port);
}

void Webadmin::startFd(int fd)
{
	m_port = fd;
	m_mode = FD;
	m_server->listenFd(fd);
}

void Webadmin::restart()
{
	if(m_mode == NOTSTARTED)
		return;

	qInfo("Restarting web-admin server");

	m_server->stop();
	if(m_mode == PORT)
		m_server->listen(m_port);
	else
		m_server->listenFd(m_port);
}

QString Webadmin::version()
{
	return MicroHttpd::version();
}

}

