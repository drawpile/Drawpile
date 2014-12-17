/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

/**
 * @brief A wrapper for HTTP API calls
 */
class ApiCall {
	typedef QJsonDocument (*ApiCallFn)(SessionServer *, const HttpRequest &);
public:
	ApiCall(SessionServer *server) : _server(server), _get(nullptr), _post(nullptr), _put(nullptr), _delete(nullptr) { }

	HttpResponse operator()(const HttpRequest &req)
	{
		switch(req.method()) {
		case HttpRequest::HEAD:
		case HttpRequest::GET:
			if(_get)
				return handle(_get, req);
			break;
		case HttpRequest::POST:
			if(_post)
				return handle(_post, req);
			break;
		case HttpRequest::PUT:
			if(_put)
				return handle(_put, req);
			break;
		case HttpRequest::DELETE:
			if(_delete)
				return handle(_delete, req);
			break;
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

	void setGet(ApiCallFn func) { _get = func; }
	void setPost(ApiCallFn func) { _post = func; }
	void setPut(ApiCallFn func) { _put = func; }
	void setDelete(ApiCallFn func) { _delete = func; }

private:
	HttpResponse handle(ApiCallFn handler, const HttpRequest &req)
	{
		QJsonDocument doc = handler(_server, req);
		if(doc.isNull())
			return HttpResponse::NotFound();
		else
			return HttpResponse::JsonResponse(doc);
	}

	SessionServer *_server;
	ApiCallFn _get;
	ApiCallFn _post;
	ApiCallFn _put;
	ApiCallFn _delete;
};

//! Helper function: return {sucess: true} or {success:false, errors: [error list]}
QJsonDocument jsonSuccess(const QStringList &errors) {
	QJsonObject o;
	o["success"] = errors.isEmpty();
	if(!errors.isEmpty()) {
		QJsonArray a;
		for(const QString &s : errors)
			a.append(s);
		o["errors"] = a;
	}
	return QJsonDocument(o);
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
		Q_ARG(int, req.pathMatch().captured(1).toInt()),
		Q_ARG(bool, true),
		Q_ARG(bool, true)
	);
	if(desc.id==0)
		return QJsonDocument();

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
			flags.append(QLatin1Literal("op"));
		if(user.isLocked)
			flags.append(QLatin1Literal("locked"));
		if(user.isSecure)
			flags.append(QLatin1Literal("secure"));
		u["flags"] = flags;

		users.append(u);
	}

	o["users"] = users;

	return QJsonDocument(o);
}

//! Delete the selected session
QJsonDocument killSession(SessionServer *server, const HttpRequest &req)
{
	int sessionId = req.pathMatch().captured(1).toInt();

	bool ok=false;
	QMetaObject::invokeMethod(
		server, "killSession", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(bool, ok),
		Q_ARG(int, sessionId)
	);

	QStringList errors;
	if(!ok)
		errors << QString("couldn't delete ") + sessionId;

	return jsonSuccess(errors);
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
	QStringList errors;
	if(req.postData().contains("title")) {
		QString newtitle = req.postData()["title"];

		if(newtitle.length() > 400) {
			errors.append("title too long");

		} else {
			QMetaObject::invokeMethod(
				server, "setTitle", Qt::BlockingQueuedConnection,
				Q_ARG(QString, newtitle)
				);
		}
	}

	return jsonSuccess(errors);
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
	QString message = req.postData()["message"].trimmed();

	if(message.isEmpty())
		return jsonSuccess(QStringList() << "empty message");

	if(sessionid.isNull()) {
		QMetaObject::invokeMethod(
			server, "wall", Qt::QueuedConnection,
			Q_ARG(QString, message)
			);
	} else {
		QMetaObject::invokeMethod(
			server, "wall", Qt::QueuedConnection,
			Q_ARG(QString, message),
			Q_ARG(int, sessionid.toInt())
			);
	}
	return jsonSuccess(QStringList());
}

} // end anymous namespace

void initWebAdminApi(MicroHttpd *httpServer, SessionServer *s)
{
	{
		ApiCall ac(s);
		ac.setGet(serverStatus);
		ac.setPost(updateServerSettings);
		httpServer->addRequestHandler("^/api/status/?$", ac);
	}

	{
		ApiCall ac(s);
		ac.setGet(sessionList);
		httpServer->addRequestHandler("^/api/sessions/?$", ac);
	}

	{
		ApiCall ac(s);
		ac.setGet(sessionInfo);
		ac.setDelete(killSession);
		httpServer->addRequestHandler("^/api/sessions/([a-zA-Z0-9:-]{1,40})/?$", ac);
	}

	{
		ApiCall ac(s);
		ac.setPost(sendToAll);
		httpServer->addRequestHandler("^/api/sessions/([a-zA-Z0-9:-]{1,40})/wall/?$", ac);
		httpServer->addRequestHandler("^/api/wall/?$", ac);
	}
}

}
