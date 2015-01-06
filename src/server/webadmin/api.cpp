/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "api.h"
#include "qmhttp.h"

#include "../shared/server/sessionserver.h"
#include "../shared/server/sessiondesc.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace server {

namespace {

class ApiCallError {
public:
	enum ErrorType {
		NOTFOUND = 404,
		BADREQUEST = 400,
		UNSUPPORTED_MEDIA_TYPE = 415,
		INTERNAL_ERROR = 500
	};

	ApiCallError(ErrorType etype, const QString &msg) : type(etype), error(msg) { }

	static ApiCallError NotFound() { return ApiCallError(NOTFOUND, QStringLiteral("Resource not found")); }
	static ApiCallError BadRequest(const QString &message) { return ApiCallError(BADREQUEST, message); }

	ErrorType type;
	QString error;
};

/**
 * @brief A wrapper for HTTP API calls
 */
class ApiCall {
	typedef QJsonDocument (*ApiCallFn)(SessionServer *, const HttpRequest &);
public:
	ApiCall(SessionServer *server) : _server(server), _get(nullptr), _post(nullptr), _put(nullptr), _delete(nullptr) { }

	HttpResponse operator()(const HttpRequest &req)
	{
		try {
			switch(req.method()) {
			case HttpRequest::HEAD:
			case HttpRequest::GET:
				if(_get)
					return HttpResponse::JsonResponse(_get(_server, req));
				break;
			case HttpRequest::POST:
				if(_post)
					return HttpResponse::JsonResponse(_post(_server, req));
				break;
			case HttpRequest::PUT:
				if(_put)
					return HttpResponse::JsonResponse(_put(_server, req));
				break;
			case HttpRequest::DELETE:
				if(_delete)
					return HttpResponse::JsonResponse(_delete(_server, req));
				break;
			}
		} catch(const ApiCallError &error) {
			QJsonObject o;
			o["success"] = false;
			o["error"] = error.error;
			return HttpResponse::JsonResponse(QJsonDocument(o), error.type);
		}

		QStringList methods;
		if(_get)
			methods << "GET";
		if(_post)
			methods << "POST";
		if(_put)
			methods << "PUT";
		if(_delete)
			methods << "DELETE";

		return HttpResponse::MethodNotAllowed(methods);
	}

	ApiCall &setGet(ApiCallFn func) { _get = func; return *this; }
	ApiCall &setPost(ApiCallFn func) { _post = func; return *this; }
	ApiCall &setPut(ApiCallFn func) { _put = func; return *this; }
	ApiCall &setDelete(ApiCallFn func) { _delete = func; return *this; }
	ApiCall addTo(MicroHttpd *server, const char *urlMatcher) { server->addRequestHandler(urlMatcher, *this); return *this; }

private:
	SessionServer *_server;
	ApiCallFn _get;
	ApiCallFn _post;
	ApiCallFn _put;
	ApiCallFn _delete;
};

//! Helper function: return {success: true}
QJsonDocument jsonSuccess() {
	QJsonObject o;
	o["success"] = true;
	return QJsonDocument(o);
}

//! Helper function: Get request's JSON body. If content-type is wrong, throws an exception
QJsonDocument getJsonBody(const HttpRequest &req)
{
	if(req.headers()["Content-Type"] != "application/json")
		throw ApiCallError(ApiCallError::UNSUPPORTED_MEDIA_TYPE, QStringLiteral("Body content type must be application/json"));

	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(req.body(), &error);

	if(error.error != QJsonParseError::NoError)
		throw ApiCallError(ApiCallError::BADREQUEST, error.errorString());

	return doc;
}

QJsonObject _sessionBaseDescription(const SessionDescription &sd)
{
	QJsonObject o;

	o["id"] = sd.id.id();
	o["title"] = sd.title;
	o["userCount"] = sd.userCount;
	o["maxUsers"] = sd.maxUsers;
	o["protoVersion"] = sd.protoMinor;

	QJsonArray flags;
	if(sd.closed)
		flags.append(QLatin1Literal("closed"));
	if(sd.persistent)
		flags.append(QLatin1Literal("persistent"));
	if(sd.hibernating)
		flags.append(QLatin1Literal("asleep"));
	if(!sd.passwordHash.isEmpty())
		flags.append(QLatin1Literal("pass"));
	o["flags"] = flags;
	o["startTime"] = sd.startTime.toString(Qt::ISODate);

	return o;
}

//! Get a list of sessions (no details)
QJsonDocument sessionList(SessionServer *server, const HttpRequest &)
{
	// Construct the session list
	QList<SessionDescription> sessions;
	QMetaObject::invokeMethod(
		server, "sessions", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(QList<SessionDescription>, sessions));

	QJsonArray jsSessions;
	for(const SessionDescription &sd : sessions) {
		jsSessions.append(_sessionBaseDescription(sd));
	}

	return QJsonDocument(jsSessions);
}

//! Get detailed information about a session
QJsonDocument sessionInfo(SessionServer *server, const HttpRequest &req)
{
	SessionDescription desc;
	QMetaObject::invokeMethod(
		server, "getSessionDescriptionById", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(SessionDescription, desc),
		Q_ARG(QString, req.pathMatch().captured(1)),
		Q_ARG(bool, true),
		Q_ARG(bool, true)
	);

	if(desc.id.isEmpty())
		throw ApiCallError::NotFound();

	QJsonObject o = _sessionBaseDescription(desc);

	// Add extended session info
	o["historySize"] = desc.historySizeMb;
	o["historyLimit"] = desc.historyLimitMb;
	o["historyStart"] = desc.historyStart;
	o["historyEnd"] = desc.historyEnd;

	// Add user list
	QJsonArray users;
	for(const UserDescription &user : desc.users) {
		QJsonObject u;
		u["id"] = user.id;
		u["name"] = user.name;

		QJsonArray flags;
		if(user.isOp)
			flags.append(QStringLiteral("op"));
		if(user.isMod)
			flags.append(QStringLiteral("mod"));
		if(user.isLocked)
			flags.append(QStringLiteral("locked"));
		if(user.isSecure)
			flags.append(QStringLiteral("secure"));
		u["flags"] = flags;

		users.append(u);
	}

	o["users"] = users;

	return QJsonDocument(o);
}

//! Delete the selected session
QJsonDocument killSession(SessionServer *server, const HttpRequest &req)
{
	QString sessionId = req.pathMatch().captured(1);

	bool ok=false;
	QMetaObject::invokeMethod(
		server, "killSession", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(bool, ok),
		Q_ARG(QString, sessionId)
	);

	// this only fails if session was not found
	if(!ok)
		throw ApiCallError::NotFound();

	return jsonSuccess();
}

//! Get general server status information
QJsonDocument serverStatus(SessionServer *server, const HttpRequest &req)
{
	Q_UNUSED(req);
	ServerStatus s;
	QMetaObject::invokeMethod(
		server, "getServerStatus", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(ServerStatus, s)
	);

	QJsonObject o;
	o["title"] = s.title;
	o["sessionCount"] = s.sessionCount;
	o["totalUsers"] = s.totalUsers;
	o["maxSessions"] = s.maxSessions;

	QJsonArray flags;
	if(s.needHostPassword)
		flags.append(QLatin1Literal("hostPass"));
	if(s.allowPersistentSessions)
		flags.append(QLatin1Literal("persistent"));
	if(s.secureMode)
		flags.append(QLatin1Literal("secure"));
	if(s.hibernation)
		flags.append(QLatin1Literal("hibernation"));
	o["flags"] = flags;

	return QJsonDocument(o);
}

/**
 * @brief Change server wide settings.
 *
 * The settigns that can currently be changed are:
 * - title
 *
 * @param server
 * @param req
 * @return
 */
QJsonDocument updateServerSettings(SessionServer *server, const HttpRequest &req)
{
	QJsonObject body = getJsonBody(req).object();

	if(body.contains("title")) {
		QString newtitle = body.value("title").toString();

		if(newtitle.length() > 400)
			throw ApiCallError::BadRequest(QStringLiteral("Title too long"));

		QMetaObject::invokeMethod(
			server, "setTitle", Qt::BlockingQueuedConnection,
			Q_ARG(QString, newtitle)
			);
	}

	return serverStatus(server, req);
}

/**
 * @brief Send a message to all users (of a session)
 *
 * If a capture group is present in the URL matcher, the target
 * session ID is read from there. Otherwise, the message will be sent to
 * every user of every session.
 *
 * @param server
 * @param req
 * @return
 */
QJsonDocument sendToAll(SessionServer *server, const HttpRequest &req)
{
	QString sessionid = req.pathMatch().captured(1);
	QJsonDocument body = getJsonBody(req);

	QString message = body.object().value("message").toString().trimmed();

	if(message.isEmpty())
		throw ApiCallError::BadRequest("Empty message");

	if(sessionid.isNull()) {
		QMetaObject::invokeMethod(
			server, "wall", Qt::QueuedConnection,
			Q_ARG(QString, message)
			);
	} else {
		QMetaObject::invokeMethod(
			server, "wall", Qt::QueuedConnection,
			Q_ARG(QString, message),
			Q_ARG(QString, sessionid)
			);
	}

	return jsonSuccess();
}

} // end anymous namespace

void initWebAdminApi(MicroHttpd *httpServer, SessionServer *s)
{
	ApiCall(s)
		.setGet(serverStatus)
		.setPut(updateServerSettings)
		.addTo(httpServer, "^/api/status/?$");

	ApiCall(s)
		.setGet(sessionList)
		.addTo(httpServer, "^/api/sessions/?$");

	ApiCall(s)
		.setGet(sessionInfo)
		.setDelete(killSession)
		.addTo(httpServer, "^/api/sessions/([a-zA-Z0-9:-]{1,40})/?$");

	ApiCall(s)
		.setPost(sendToAll)
		.addTo(httpServer, "^/api/sessions/([a-zA-Z0-9:-]{1,40})/wall/?$")
		.addTo(httpServer, "^/api/wall/?$");
}

}
