/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "announcementapi.h"
#include "networkaccess.h"
#include "../server/serverlog.h"
#include "config.h" // for DRAWPILE_VERSION

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

namespace sessionlisting {

using server::Log;

static const char *PROP_APIURL = "APIURL";        // The base API URL of the request
static const char *PROP_SESSION_ID = "SESSIONID"; // The ID of the session being announced or unlisted
static const char *USER_AGENT = "DrawpileListingClient/" DRAWPILE_VERSION;

class ResponseError {
public:
	ResponseError(const QString &e) : error(e) { }

	QString error;
};

static QString slashcat(QString s, const QString &s2)
{
	if(!s.endsWith('/'))
		s.append('/');
	s.append(s2);
	return s;
}

AnnouncementApi::AnnouncementApi(QObject *parent)
	: QObject(parent)
{
}

void AnnouncementApi::getApiInfo(const QUrl &apiUrl)
{
	emit logMessage(Log().about(Log::Level::Debug, Log::Topic::PubList).message("Getting API info from " + apiUrl.toString()));

	QNetworkRequest req(apiUrl);
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->setProperty(PROP_APIURL, apiUrl);
	connect(reply, &QNetworkReply::finished, this, [reply, this]() { handleResponse(reply, &AnnouncementApi::handleServerInfoResponse);} );
}

void AnnouncementApi::getSessionList(const QUrl &apiUrl, const QString &protocol, const QString &title, bool nsfm)
{
	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));

	QUrlQuery query;
	if(!protocol.isEmpty())
		query.addQueryItem("protocol", protocol);
	if(!title.isEmpty())
		query.addQueryItem("title", title);
	if(nsfm)
		query.addQueryItem("nsfm", "true");
	url.setQuery(query);

	QNetworkRequest req(url);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->setProperty(PROP_APIURL, apiUrl);
	connect(reply, &QNetworkReply::finished, this, [reply, this]() { handleResponse(reply, &AnnouncementApi::handleListingResponse);} );
}

void AnnouncementApi::announceSession(const QUrl &apiUrl, const Session &session)
{
	emit logMessage(Log().about(Log::Level::Info, Log::Topic::PubList).message("Announcing " + session.id + " at " + apiUrl.toString()));

	// Construct the announcement
	QJsonObject o;
	if(!session.host.isEmpty())
		o["host"] = session.host;
	if(session.port>0)
		o["port"] = session.port;
	o["id"] = session.id;
	o["protocol"] = session.protocol.asString();
	o["title"] = session.title;
	o["users"] = session.users;
	o["usernames"] = QJsonArray::fromStringList(session.usernames);
	o["password"] = session.password;
	o["owner"] = session.owner;
	o["nsfm"] = session.nsfm;

	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	QNetworkReply *reply = networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	reply->setProperty(PROP_APIURL, apiUrl);
	reply->setProperty(PROP_SESSION_ID, session.id);
	connect(reply, &QNetworkReply::finished, this, [reply, this]() { handleResponse(reply, &AnnouncementApi::handleAnnounceResponse);} );
}

void AnnouncementApi::refreshSession(const Announcement &a, const Session &session)
{
	emit logMessage(Log().about(Log::Level::Debug, Log::Topic::PubList).message(QString("Refreshing listing %1 at %2").arg(a.listingId).arg(a.apiUrl.toString())));

	// Construct the announcement
	QJsonObject o;

	o["title"] = session.title;
	o["users"] = session.users;
	o["usernames"] = QJsonArray::fromStringList(session.usernames);
	o["password"] = session.password;
	o["owner"] = session.owner;
	o["nsfm"] = session.nsfm;

	// Send request
	QUrl url = a.apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	QNetworkReply *reply = networkaccess::getInstance()->put(req, QJsonDocument(o).toJson());
	reply->setProperty(PROP_APIURL, a.apiUrl);
	connect(reply, &QNetworkReply::finished, this, [reply, this]() { handleResponse(reply, &AnnouncementApi::handleRefreshResponse);} );
}

void AnnouncementApi::unlistSession(const Announcement &a)
{
	emit logMessage(Log().about(Log::Level::Info, Log::Topic::PubList).message(QString("Unlisting announcement %1 at %2").arg(a.listingId).arg(a.apiUrl.toString())));

	QUrl url = a.apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	QNetworkReply *reply = networkaccess::getInstance()->deleteResource(req);
	reply->setProperty(PROP_APIURL, a.apiUrl);
	reply->setProperty(PROP_SESSION_ID, a.id);
	connect(reply, &QNetworkReply::finished, this, [reply, this]() { handleResponse(reply, &AnnouncementApi::handleUnlistResponse);} );
}

void AnnouncementApi::handleResponse(QNetworkReply *reply, AnnouncementApi::HandlerFunc handlerFunc)
{
	Q_ASSERT(reply);
	Q_ASSERT(handlerFunc);

	try {
		if(reply->error() != QNetworkReply::NoError) {
			const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if(statusCode == 422 || statusCode == 409) {
				// Server says that the problem is at our end
				QJsonParseError error;
				QByteArray body = reply->readAll();
				QJsonDocument doc = QJsonDocument::fromJson(body, &error);
				if(error.error != QJsonParseError::NoError)
					throw ResponseError(QStringLiteral("Http error 422 but response body was unparseable: %1").arg(error.errorString()));

				const QString msg = doc.object()["message"].toString();
				if(msg.isEmpty()) {
					throw ResponseError(QStringLiteral("Http error 422 (no explanation given)"));
				} else {
					throw ResponseError(msg);
				}

			} else {
				// Other errors
				throw ResponseError(QStringLiteral("Network error: ") + reply->errorString());
			}
		}

		// Server did not return an error: handle the response
		((this)->*(handlerFunc))(reply);

	} catch(const ResponseError &e) {
		emit logMessage(Log().about(Log::Level::Error, Log::Topic::PubList).message("Announcement API error: " + e.error));
		emit error(reply->property(PROP_APIURL).toString(), "Session announcement: " + e.error);
	}

	reply->deleteLater();
}

void AnnouncementApi::handleAnnounceResponse(QNetworkReply *reply)
{
	QJsonParseError error;
	QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &error);
	if(error.error != QJsonParseError::NoError)
		throw ResponseError(QStringLiteral("Error parsing announcement response: %1").arg(error.errorString()));

	Announcement a;
	a.apiUrl = reply->property(PROP_APIURL).toUrl();
	a.id = reply->property(PROP_SESSION_ID).toString();
	a.updateKey = doc.object()["key"].toString();
	a.listingId = doc.object()["id"].toInt();
	a.refreshInterval = qMax(2, doc.object()["expires"].toInt(6)) - 1;

	qDebug("Announcement server %s refresh interval is %d minutes.", qPrintable(a.apiUrl.toString()), a.refreshInterval);

	emit logMessage(Log().about(Log::Level::Debug, Log::Topic::PubList).message(QString("Announced session %2. Got listing ID %1").arg(a.listingId).arg(a.id)));

	emit sessionAnnounced(a);

	QString welcome = doc.object()["message"].toString();
	if(!welcome.isEmpty())
		emit messageReceived(doc.object()["message"].toString());
}

void AnnouncementApi::handleUnlistResponse(QNetworkReply *reply)
{
	emit unlisted(
				reply->property(PROP_APIURL).toString(),
				reply->property(PROP_SESSION_ID).toString()
				);
}

void AnnouncementApi::handleRefreshResponse(QNetworkReply *reply)
{
	QJsonParseError error;
	QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &error);
	if(error.error != QJsonParseError::NoError)
		throw ResponseError(QStringLiteral("Error parsing refresh response: %1").arg(error.errorString()));

	QString msg = doc.object()["message"].toString();
	if(!msg.isEmpty())
		emit messageReceived(msg);
}

void AnnouncementApi::handleListingResponse(QNetworkReply *reply)
{
	QJsonParseError error;
	QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &error);
	if(error.error != QJsonParseError::NoError)
		throw ResponseError(QStringLiteral("Error parsing announcement response: %1").arg(error.errorString()));

	QList<Session> sessions;

	if(!doc.isArray())
		throw ResponseError(QStringLiteral("Expected array of session descriptions!"));

	for(const QJsonValue &jsv : doc.array()) {
		if(!jsv.isObject())
			throw ResponseError(QStringLiteral("Expected session description!"));
		const QJsonObject obj = jsv.toObject();

		QDateTime started = QDateTime::fromString(obj["started"].toString(), Qt::ISODate);
		started.setTimeSpec(Qt::UTC);

		sessions << Session {
			obj["host"].toString(),
			obj["port"].toInt(),
			obj["id"].toString(),
			protocol::ProtocolVersion::fromString(obj["protocol"].toString()),
			obj["title"].toString(),
			obj["users"].toInt(),
			obj["usernames"].toVariant().toStringList(),
			obj["password"].toBool(),
			obj["nsfm"].toBool(),
			obj["owner"].toString(),
			started
		};
	}

	emit sessionListReceived(sessions);
}

void AnnouncementApi::handleServerInfoResponse(QNetworkReply *reply)
{
	QJsonParseError error;
	QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &error);
	if(error.error != QJsonParseError::NoError)
		throw ResponseError(QStringLiteral("Error parsing API response: %1").arg(error.errorString()));

	if(!doc.isObject())
		throw ResponseError(QStringLiteral("Expected object!"));

	QJsonObject obj = doc.object();

	QString apiname = obj.value("api_name").toString();
	if(apiname != "drawpile-session-list")
		throw ResponseError(QStringLiteral("This is not a Drawpile listing server!"));

	ListServerInfo info {
		obj.value("version").toString(),
		obj.value("name").toString().trimmed(),
		obj.value("description").toString().trimmed(),
		obj.value("favicon").toString()
	};

	if(info.version.isEmpty())
		throw ResponseError(QStringLiteral("API version not specified!"));

	if(!info.version.startsWith("1."))
		throw ResponseError(QStringLiteral("Unsupported API version!"));

	if(info.name.isEmpty())
		throw ResponseError(QStringLiteral("Server name missing!"));

	emit serverInfo(info);
}

}
