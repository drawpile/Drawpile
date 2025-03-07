// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/listings/announcementapi.h"
#include "libshared/listings/listserverfinder.h"
#include "libshared/util/networkaccess.h"
#include "cmake-config/config.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>
#include <QTimeZone>

namespace sessionlisting {

static void setCommonRequestParameters(QNetworkRequest &req) {
#ifndef __EMSCRIPTEN__
	static const QString USER_AGENT = QStringLiteral("DrawpileListingClient/%1").arg(cmake_config::version());
	req.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
#endif
	req.setMaximumRedirectsAllowed(2);
}

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
static ApiReply readReply(QNetworkReply *reply, bool cancelled)
{
	Q_ASSERT(reply);
	if(cancelled) {
		return ApiError(AnnouncementApiResponse::tr("Cancelled."));
	}

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

void AnnouncementApiResponse::setError(const QString &error, QNetworkReply::NetworkError networkError)
{
	m_error = error.isEmpty() ? tr("Unknown error") : error;
	m_networkError = networkError;
	m_gone = isGoneMessage(error);
	if(m_gone)
		emit serverGone();
	emit finished(QVariant(), QString(), error);
}

void AnnouncementApiResponse::updateRequestUrl(const QUrl &url)
{
	emit requestUrlChanged(url);
}

void AnnouncementApiResponse::cancel()
{
	m_cancelled = true;
	emit cancelRequested();
}

static void readApiInfoReply(QNetworkReply *reply, AnnouncementApiResponse *res)
{
	auto r = readReply(reply, res->isCancelled());
	if(IsApiError(r)) {
		res->setError(ApiError(r), reply->error());
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
	setCommonRequestParameters(req);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		if(res->isCancelled()) {
			res->setError(AnnouncementApiResponse::tr("Cancelled."));
			return;
		}

		const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

		// If the URL returns HTML, extract the link to the real API from the meta tags and try again
		if(contentType.startsWith("text/html") || contentType.startsWith("application/xhtml+xml")) {
			if(reply->error() != QNetworkReply::NoError) {
				res->setError(reply->errorString(), reply->error());
				return;
			}

			const auto realApiUrl = findListserverLinkHtml(reply);
			if(realApiUrl.isEmpty()) {
				res->setError(AnnouncementApiResponse::tr("No listserver link found!"));

			} else {
				QUrl apiUrl2 = reply->url().resolved(realApiUrl);
				res->updateRequestUrl(apiUrl2);

				QNetworkRequest req2(apiUrl2);
				setCommonRequestParameters(req2);

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
	res->connect(res, &AnnouncementApiResponse::cancelRequested, reply, &QNetworkReply::abort);

	return res;
}

AnnouncementApiResponse *getSessionList(const QUrl &apiUrl)
{
	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));

	QUrlQuery query;
	query.addQueryItem("nsfm", "true");
	url.setQuery(query);

	QNetworkRequest req(url);
	setCommonRequestParameters(req);

	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		auto r = readReply(reply, res->isCancelled());
		if(IsApiError(r)) {
			res->setError(ApiError(r), reply->error());
			return;
		}
		const auto doc = ApiSuccess(r);

		if(!doc.isArray()) {
			res->setError(QStringLiteral("Expected array of session descriptions!"));
			qWarning() << "Received" << doc;
			return;
		}

		QVector<Session> sessions;
#if QT_CONFIG(timezone)
		QTimeZone utc = QTimeZone::utc();
#else
		QTimeZone utc = QTimeZone(QTimeZone::UTC);
#endif
		for(const QJsonValue jsv : doc.array()) {
			if(!jsv.isObject()) {
				res->setError(QStringLiteral("Expected session description!"));
				return;
			}

			const QJsonObject obj = jsv.toObject();

			QDateTime started = QDateTime::fromString(obj["started"].toString(), Qt::ISODate);
			started.setTimeZone(utc);

			sessions << Session {
				obj["host"].toString(),
				obj["port"].toInt(),
				obj["id"].toString(),
				protocol::ProtocolVersion::fromString(obj["protocol"].toString()),
				obj["title"].toString(),
				obj["users"].toInt(),
				obj["password"].toBool(),
				obj["nsfm"].toBool(),
				obj["owner"].toString(),
				started,
				obj["maxusers"].toInt(),
				obj["closed"].toBool(),
				obj["activedrawingusers"].toInt(-1),
				obj["allowweb"].toBool(false),
				obj["preferwebsockets"].toBool(),
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
	if(!session.host.isEmpty()) {
		o["host"] = session.host;
	}
	if(session.port > 0) {
		o["port"] = session.port;
	}
	o["id"] = session.id;
	o["protocol"] = session.protocol.asString();
	o["title"] = session.title;
	o["users"] = session.users;
	o["activedrawingusers"] = session.activeDrawingUsers;
	o["password"] = session.password;
	o["owner"] = session.owner;
	o["nsfm"] = session.nsfm;
	if(session.maxUsers > 0) {
		o["maxusers"] = session.maxUsers;
	}
	if(session.closed) {
		o["closed"] = session.closed;
	}
	if(session.allowWeb) {
		o["allowweb"] = true;
	}
	if(session.preferWebSockets) {
		o.insert(QStringLiteral("preferwebsockets"), true);
	}

	const QString sessionId = session.id;

	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	setCommonRequestParameters(req);

	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res, apiUrl, sessionId]() {
		auto r = readReply(reply, res->isCancelled());
		if(IsApiError(r)) {
			res->setError(ApiError(r), reply->error());
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();

		const Announcement a {
			apiUrl,
			sessionId,
			obj["key"].toString(),
			obj["id"].toInt(),
			qMax(2, obj["expires"].toInt(6)) - 1,
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
	o["activedrawingusers"] = session.activeDrawingUsers;
	o["password"] = session.password;
	o["owner"] = session.owner;
	o["nsfm"] = session.nsfm;
	o["maxusers"] = session.maxUsers;
	o["closed"] = session.closed;
	o["allowweb"] = session.allowWeb;
	o["preferwebsockets"] = session.preferWebSockets;

	// Send request
	QUrl url = a.apiUrl;
	url.setPath(slashcat(url.path(), QStringLiteral("sessions/%1").arg(a.listingId)));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	setCommonRequestParameters(req);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	AnnouncementApiResponse *res = new AnnouncementApiResponse(a.apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->put(req, QJsonDocument(o).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res, a]() {
		auto r = readReply(reply, res->isCancelled());
		if(IsApiError(r)) {
			res->setError(ApiError(r), reply->error());
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
		o["activedrawingusers"] = listing.second.activeDrawingUsers;
		o["password"] = listing.second.password;
		o["owner"] = listing.second.owner;
		o["nsfm"] = listing.second.nsfm;
		o["maxusers"] = listing.second.maxUsers;
		o["closed"] = listing.second.closed;
		o["allowweb"] = listing.second.allowWeb;
		o["preferwebsockets"] = listing.second.preferWebSockets;

		batch[QString::number(listing.first.listingId)] = o;
	}



	// Send request
	QUrl url = apiUrl;
	url.setPath(slashcat(url.path(), "sessions/"));

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	setCommonRequestParameters(req);

	AnnouncementApiResponse *res = new AnnouncementApiResponse(apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->put(req, QJsonDocument(batch).toJson());
	reply->connect(reply, &QNetworkReply::finished, res, [reply, res]() {
		auto r = readReply(reply, res->isCancelled());
		if(IsApiError(r)) {
			res->setError(ApiError(r), reply->error());
			return;
		}
		const auto doc = ApiSuccess(r);
		const QJsonObject obj = doc.object();

		res->setResult(
			QVariantHash{
				{"responses", obj["responses"].toObject().toVariantHash()},
				{"errors", obj["errors"].toObject().toVariantHash()},
			},
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
	setCommonRequestParameters(req);
	req.setRawHeader("X-Update-Key", a.updateKey.toUtf8());

	AnnouncementApiResponse *res = new AnnouncementApiResponse(a.apiUrl);

	QNetworkReply *reply = networkaccess::getInstance()->deleteResource(req);
	reply->connect(reply, &QNetworkReply::finished, res, [a, res]() {
		res->setResult(a.id);
	});
	reply->connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

	return res;
}

}

