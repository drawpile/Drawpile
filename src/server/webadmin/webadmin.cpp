/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "webadmin.h"
#include "qmhttp.h"
#include "multiserver.h"

#include "../shared/server/jsonapi.h"

#include <QJsonObject>
#include <QMetaObject>

namespace server {

Webadmin::Webadmin(QObject *parent)
	: QObject(parent), m_server(new MicroHttpd(this))
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
	m_server->addRequestHandler(".*", [server](const HttpRequest &req) {
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
		}

		const QStringList path = req.path().split('/', QString::SkipEmptyParts);

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
			if(req.headers()["Content-Type"] != "application/json") {
				return HttpResponse::JsonErrorResponse("Content type should be application/json", 400);
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
	m_server->listen(port);
	// TODO listenFd
}

}
