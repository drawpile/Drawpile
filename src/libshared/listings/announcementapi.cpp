/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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
#include "listserverfinder.h"
#include "../util/networkaccess.h"
#include "config.h" // for DRAWPILE_VERSION

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

namespace sessionlisting {

static const char *USER_AGENT = "DrawpileListingClient/" DRAWPILE_VERSION;

static QString slashcat(QString s, const QString &s2)
{
	if(!s.endsWith('/'))
		s.append('/');
	s.append(s2);
	return s;
}

typedef QPair<QJsonDocument,QString> ApiReply;
static inline ApiReply ApiSuccess(const QJsonDocument &doc) { return ApiReply(doc, QString()); }
static inline QJsonDocument ApiSuccess(const ApiReply &r) { return r.first; }
static inline ApiReply ApiError(const QString &message) { return ApiReply(QJsonDocument(), message); }
static inline ApiReply ApiGone() { return ApiReply(QJsonDocument(), QStringLiteral("ERROR 410 GONE")); }
static inline bool isGoneMessage(const QString &message) { return message == "ERROR 410 GONE"; }
static inline QString ApiError(const ApiReply &r) { return r.second; }
static inline bool IsApiError(const ApiReply &r) { return !ApiError(r).isEmpty(); }

/**
 * Read response body and handle standard errors
 */
static ApiReply readReply(QNetworkReply *reply)
{
	Q_ASSERT(reply);

	if(reply->error() != QNetworkReply::NoError) {
		const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if(statusCode == 400 || statusCode == 422 || statusCode == 409) {
			// Server says that the problem is at our end
			QJsonParseError error;
			QByteArray body = reply->readAll();
			QJsonDocument doc = QJsonDocument::fromJson(body, &error);
			if(error.error != QJsonParseError::NoError)
				return ApiError(QStringLiteral("Http error %1 but response body was unparseable: %2").arg(statusCode).arg(error.errorString()));

			const QString msg = doc.object()["message"].toString();
			if(msg.isEmpty()) {
				return ApiError(QStringLiteral("Http error %d (no explanation given)").arg(statusCode));
			} else {
				return ApiError(msg);
			}
		} else if(statusCode == 410) {
			// Server has been shut down permanently
			return ApiGone();

		} else {
			// Other errors
			return ApiError(QStringLiteral("Network error: ") + reply->errorString());
		}
	}

	// No error, parse response body
	QJsonParseError error;
	const QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &error);
	if(error.error != QJsonParseError::NoError) {
		return ApiError(QStringLiteral("Unparseable response: ") + error.errorString());
	}

	return ApiSuccess(doc);
}

void AnnouncementApiResponse::setResult(const QVariant &result, const QString &message)
{
	m_result = result;
	m_message = message;
	emit finished(m_result, m_message, QString());
}

void AnnouncementApiResponse::setError(const QString &error)
{
	m_error = error;
	m_gone = isGoneMessage(error);
	if(m_gone)
		emit serverGone();
	emit finished(QVariant(), QString(), error);
}

static void readApiInfoReply(QNetworkReply *reply, AnnouncementApiResponse *res)
{
	auto r = readReply(reply);
	if(IsApiError(r)) {
		res->setError(ApiError(r));
		return;
	}
	const auto doc = ApiSuccess(r);

	res->setUrl(reply->url());

	if(!doc.isObject()) {
		res->setError(QStringLiteral("Invalid response: object expected!"));
		return;
	}

	const QJsonObject obj = doc.object();

	QString apiname = obj.value("api_name").toString();
	if(apiname != "drawpile-session-list") {
		res->setError(QStringLiteral("This is not a Drawpile listing server!"));
		return;
	}

	ListServerInfo info {
		obj.value("version").toString(),
		obj.value("name").toString().trimmed(),
		obj.value("description").toString().trimmed(),
		obj.value("favicon").toString(),
		obj.value("read_only").toBool(),
		obj.value("public").toBool(true),
		// backward compatibility: private defaults to true only
		// if the server is not a read only server.
		obj.value("private").toBool(!obj.value("read_only").toBool())
	};

	if(info.version.isEmpty()) {
		res->setError(QStringLiteral("API version not specified!"));
		return;
	}

	if(!info.version.startsWith("1.")) {
		res->setError(QStringLiteral("Unsupported API version!"));
		return;
	}

	if(info.name.isEmpty()) {
		res->setError(QStringLiteral("Server name missing!"));
		return;
	}

	res->setResult(QVariant::fromValue(info));
}

AnnouncementApiResponse *getApiInfo(const QUrl &apiUrl)
{
	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkRequest req(apiUrl);
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
	req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

		// If the URL returns HTML, extract the link to the real API from the meta tags and try again
		if(contentType.startsWith("text/html") || contentType.startsWith("application/xhtml+xml")) {
			if(reply->error() != QNetworkReply::NoError) {
				res->setError(reply->errorString());
				return;
			}

			const auto realApiUrl = findListserverLinkHtml(reply);  
			if(realApiUrl.isEmpty()) {
				res->setError("No listserver link found!");

			} else {
				QNetworkRequest req2(reply->url().resolved(realApiUrl));
				req2.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
				req2.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

				QNetworkReply *reply2 = networkaccess::getInstance()->get(req2);
				reply2->connect(reply2, &QNetworkReply::finished, res, [reply2, res]() {
					readApiInfoReply(reply2, res);
				});
				reply2->connect(reply2, &QNetworkReply::finished, reply2, &QObject::deleteLater);
			}

		} else {
			readApiInfoReply(reply, res);
		}
	});
	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

AnnouncementApiResponse *getSessionList(const QUrl &apiUrl, const QString &protocol, const QString &title, bool nsfm)
{
	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

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
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		auto r = readReply(reply);
		if(IsApiError(r)) {
			res->setError(ApiError(r));
			return;
		}
		const auto doc = ApiSuccess(r);

		if(!doc.isArray()) {
			res->setError(QStringLiteral("Expected array of session descriptions!"));
			qWarning() << "Received" << doc;
			return;
		}

		QVector<Session> sessions;

		for(const QJsonValue jsv : doc.array()) {
			if(!jsv.isObject()) {
				res->setError(QStringLiteral("Expected session description!"));
				return;
			}

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
				PrivacyMode::Public, // a listed session cannot be private by definition
				obj["owner"].toString(),
				started
			};
		}

		res->setResult(QVariant::fromValue(sessions));
	});
	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
	
	return res;
}

AnnouncementApiResponse *announceSession(const QUrl &apiUrl, const Session &session)
{
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
	if(session.isPrivate == PrivacyMode::Private)
		o["private"] = true;

	const QString sessionId = session.id;

	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res, apiUrl, sessionId]() {
		auto r = readReply(reply);
		if(IsApiError(r)) {
			res->setError(ApiError(r));
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();

		const Announcement a {
			apiUrl,
			sessionId,
			obj["key"].toString(),
			obj["roomcode"].toString(),
			obj["id"].toInt(),
			qMax(2, obj["expires"].toInt(6)) - 1,
			obj["private"].toBool()
		};

		res->setResult(QVariant::fromValue(a), doc.object()["message"].toString());
	});
	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

AnnouncementApiResponse *refreshSession(const Announcement &a, const Session &session)
{
	// Construct the announcement
	QJsonObject o;

	o["title"] = session.title;
	o["users"] = session.users;
	o["usernames"] = QJsonArray::fromStringList(session.usernames);
	o["password"] = session.password;
	o["owner"] = session.owner;
	o["nsfm"] = session.nsfm;
	if(session.isPrivate != PrivacyMode::Undefined)
		o["private"] = session.isPrivate == PrivacyMode::Private;

	// Send request
	QUrl url = a.apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	AnnouncementApiResponse *res = new AnnouncementApiResponse(a.apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->put(req, QJsonDocument(o).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res, a]() { 
		auto r = readReply(reply);
		if(IsApiError(r)) {
			res->setError(ApiError(r));
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();

		res->setResult(a.id, obj["message"].toString());
	});

	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

AnnouncementApiResponse *refreshSessions(const QVector<QPair<Announcement, Session>> &listings)
{
	Q_ASSERT(!listings.isEmpty());
	const QUrl apiUrl = listings.first().first.apiUrl;

	// Construct the announcement
	QJsonObject batch;

	for(const auto &listing : listings) {
		Q_ASSERT(listing.first.apiUrl == apiUrl);
		QJsonObject o;

		o["updatekey"] = listing.first.updateKey;
		o["title"] = listing.second.title;
		o["users"] = listing.second.users;
		o["usernames"] = QJsonArray::fromStringList(listing.second.usernames);
		o["password"] = listing.second.password;
		o["owner"] = listing.second.owner;
		o["nsfm"] = listing.second.nsfm;
		if(listing.second.isPrivate != PrivacyMode::Undefined)
			o["private"] = listing.second.isPrivate == PrivacyMode::Private;

		batch[QString::number(listing.first.listingId)] = o;
	}



	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->put(req, QJsonDocument(batch).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		auto r = readReply(reply);
		if(IsApiError(r)) {
			res->setError(ApiError(r));
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();

		res->setResult(
			obj["responses"].toObject().toVariantHash(),
			obj["message"].toString()
			);
	});

	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

AnnouncementApiResponse *unlistSession(const Announcement &a)
{
	QUrl url = a.apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	AnnouncementApiResponse *res = new AnnouncementApiResponse(a.apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->deleteResource(req);
	reply->connect(reply, &QNetworkReply::finished, res, [a, res]() {
		res->setResult(a.id);
	});
	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

AnnouncementApiResponse *queryRoomcode(const QUrl &apiUrl, const QString &roomcode)
{
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("join/") + roomcode));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);

	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		auto r = readReply(reply);
		if(IsApiError(r)) {
			res->setError(ApiError(r));
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();
		const Session session {
			obj["host"].toString(),
			obj["port"].toInt(),
			obj["id"].toString(),
			protocol::ProtocolVersion::current(),
			QString(),
			0,
			QStringList(),
			false,
			false,
			PrivacyMode::Undefined,
			QString(),
			QDateTime()
		};
		res->setResult(QVariant::fromValue(session));
	});

	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

}

