/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#include "logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

#if (QT_VERSION < QT_VERSION_CHECK(5, 2, 0))
// A horrible kludge to work around missing QJsonValue::toInt in Qt 5.0 and 5.1
#define toInt toString().toInt
#endif

namespace sessionlisting {

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
	_net = new QNetworkAccessManager(this);
	connect(_net, &QNetworkAccessManager::finished, this, &AnnouncementApi::handleResponse);
}

bool AnnouncementApi::isWhitelisted(const QUrl &url) const
{
	if(_whitelist) {
		if(!_whitelist(url)) {
			logger::warning() << "Announcement API not whitelisted:" << url.toString();
			return false;
		}
	}
	return true;
}

void AnnouncementApi::getApiInfo(const QUrl &apiUrl)
{
	if(isWhitelisted(apiUrl)) {
		logger::debug() << "getting API info from" << apiUrl.toString();

		QNetworkRequest req(apiUrl);

		QNetworkReply *reply = _net->get(req);
		reply->setProperty("QUERY_TYPE", "getinfo");
	}
}

void AnnouncementApi::getSessionList(const QUrl &apiUrl, const QString &protocol, const QString &title, bool nsfm)
{
	if(isWhitelisted(apiUrl)) {
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

		QNetworkReply *reply = _net->get(req);
		reply->setProperty("QUERY_TYPE", "getlist");
	}
}

void AnnouncementApi::announceSession(const QUrl &apiUrl, const Session &session)
{
	if(isWhitelisted(apiUrl)) {
		logger::debug() << "announcing" << session.id << "at" << apiUrl.toString();

		// Construct the announcement
		QJsonObject o;
		if(!session.host.isEmpty())
			o["host"] = session.host;
		else if(!_localAddress.isEmpty())
			o["host"] = _localAddress;
		if(session.port>0)
			o["port"] = session.port;
		o["id"] = session.id;
		o["protocol"] = session.protocol;
		o["title"] = session.title;
		o["users"] = session.users;
		o["usernames"] = QJsonArray::fromStringList(session.usernames);
		o["password"] = session.password;
		o["owner"] = session.owner;
		// TODO: explicit NSFM tag

		// Send request
		QUrl url = apiUrl;
		url.setPath(slashcat(url.path(), "sessions/"));
		QNetworkRequest req(url);
		req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

		QNetworkReply *reply = _net->post(req, QJsonDocument(o).toJson());
		reply->setProperty("QUERY_TYPE", "announce");
		reply->setProperty("API_URL", apiUrl);
		reply->setProperty("SESSION_ID", session.id);
	}
}

void AnnouncementApi::refreshSession(const Announcement &a, const Session &session)
{
	if(isWhitelisted(a.apiUrl)) {
		logger::debug() << "refreshing" << a.listingId << "at" << a.apiUrl.toString();

		// Construct the announcement
		QJsonObject o;

		o["title"] = session.title;
		o["users"] = session.users;
		o["usernames"] = QJsonArray::fromStringList(session.usernames);
		o["password"] = session.password;
		o["owner"] = session.owner;

		// Send request
		QUrl url = a.apiUrl;
		url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

		QNetworkRequest req(url);
		req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

		QNetworkReply *reply = _net->put(req, QJsonDocument(o).toJson());
		reply->setProperty("QUERY_TYPE", "refresh");
	}
}

void AnnouncementApi::unlistSession(const Announcement &a)
{
	if(isWhitelisted(a.apiUrl)) {
		logger::debug() << "unlisting" << a.listingId << "at" << a.apiUrl.toString();

		QUrl url = a.apiUrl;
		url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

		QNetworkRequest req(url);
		req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

		QNetworkReply *reply = _net->deleteResource(req);
		reply->setProperty("QUERY_TYPE", "unlist");
		reply->setProperty("SESSION_ID", a.id);
	}
}

void AnnouncementApi::handleResponse(QNetworkReply *reply)
{
	try {
		if(reply->error() != QNetworkReply::NoError)
			throw ResponseError(reply->errorString());

		// TODO handle redirects

		const QString querytype = reply->property("QUERY_TYPE").toString();
		if(querytype == "announce")
			handleAnnounceResponse(reply);
		else if(querytype == "unlist")
			handleUnlistResponse(reply);
		else if(querytype == "refresh")
			handleRefreshResponse(reply);
		else if(querytype == "getlist")
			handleListingResponse(reply);
		else if(querytype == "getinfo")
			handleServerInfoResponse(reply);
		else // shouldn't happen
			qFatal("Unhandled Announcement API response query type");

	} catch(const ResponseError &e) {
		logger::error() << "Announce API error:" << e.error;
		emit error("Session announcement: " + e.error);
#ifndef NDEBUG
		logger::error() << "Announcement API error:" << QString::fromUtf8(reply->readAll());
#endif
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
	a.apiUrl = reply->property("API_URL").toUrl();
	a.id = reply->property("SESSION_ID").toString();
	a.updateKey = doc.object()["key"].toString();
	a.listingId = doc.object()["id"].toInt();

	logger::debug() << "Announced session. Got listing ID" << a.listingId;

	emit sessionAnnounced(a);

	QString welcome = doc.object()["message"].toString();
	if(!welcome.isEmpty())
		emit messageReceived(doc.object()["message"].toString());
}

void AnnouncementApi::handleUnlistResponse(QNetworkReply *reply)
{
	emit unlisted(reply->property("SESSION_ID").toString());
}

void AnnouncementApi::handleRefreshResponse(QNetworkReply *reply)
{
	Q_UNUSED(reply);
	// nothing interesting in this reply (except error code)
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
			obj["protocol"].toString(),
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
