#include "thinsrv/extbans.h"
#include "cmake-config/config.h"
#include "libserver/serverlog.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/qtcompat.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVector>

namespace server {

ExtBans::ExtBans(ServerConfig *config, QObject *parent)
	: QObject(parent)
	, m_config(config)
	, m_refreshTimer(new QTimer(this))
{
	m_refreshTimer->setSingleShot(true);
	m_refreshTimer->setTimerType(Qt::VeryCoarseTimer);
	connect(m_refreshTimer, &QTimer::timeout, this, &ExtBans::refresh);
	connect(
		m_config, &ServerConfig::configValueChanged, this,
		&ExtBans::handleConfigValueChange);
}

QJsonObject ExtBans::status() const
{
	return {
		{QStringLiteral("url"), m_url.toDisplayString()},
		{QStringLiteral("intervalMsecs"), m_intervalMsecs},
		{QStringLiteral("started"), m_started},
		{QStringLiteral("active"), m_active},
		{QStringLiteral("msecsUntilNextCheck"),
		 m_refreshTimer->remainingTime()}};
}

ExtBans::RefreshResult ExtBans::refreshNow()
{
	if(m_active) {
		if(m_refreshTimer->isActive()) {
			m_refreshTimer->stop();
			refresh();
			return RefreshResult::Ok;
		} else {
			return RefreshResult::AlreadyInProgress;
		}
	} else {
		return RefreshResult::NotActive;
	}
}

void ExtBans::loadFromCache()
{
	QString response = m_config->getConfigString(config::ExtBansCacheResponse);
	if(!response.isEmpty()) {
		m_config->setExternalBans(
			handleBans(QJsonDocument::fromJson(response.toUtf8()).object()));
	}
}

void ExtBans::start()
{
	setUrl(m_config->getConfigString(config::ExtBansUrl));
	setIntervalSecs(m_config->getConfigTime(config::ExtBansCheckInterval));
	m_started = true;
	update();
}

void ExtBans::stop()
{
	m_started = false;
	update();
}

void ExtBans::handleConfigValueChange(const ConfigKey &key)
{
	if(m_started) {
		if(key.index == config::ExtBansUrl.index) {
			setUrl(m_config->getConfigString(config::ExtBansUrl));
		} else if(key.index == config::ExtBansCheckInterval.index) {
			setIntervalSecs(
				m_config->getConfigTime(config::ExtBansCheckInterval));
		}
	}
}

void ExtBans::refresh()
{
	if(m_active) {
		QNetworkRequest req(m_url);
		req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());

		QString cacheUrl = m_url.toString(QUrl::FullyEncoded);
		QString cacheKey;
		if(cacheUrl == m_config->getConfigString(config::ExtBansCacheUrl)) {
			cacheKey = m_config->getConfigString(config::ExtBansCacheKey);
			if(!cacheKey.isEmpty()) {
				req.setHeader(
					QNetworkRequest::IfNoneMatchHeader, cacheKey.toUtf8());
			}
		}

		QNetworkReply *reply = networkaccess::getInstance()->get(req);
		connect(this, &ExtBans::deactivated, reply, &QNetworkReply::abort);
		connect(reply, &QNetworkReply::finished, this, [=]() {
			handleReply(reply, cacheUrl, cacheKey);
			reply->deleteLater();
			if(m_active) {
				m_refreshTimer->start(m_intervalMsecs);
			}
		});
	}
}

const QString &ExtBans::userAgent()
{
	static const QString USER_AGENT =
		QStringLiteral("DrawpileExtBansClient/%1").arg(cmake_config::version());
	return USER_AGENT;
}

void ExtBans::handleReply(
	QNetworkReply *reply, const QString &cacheUrl, const QString &cacheKey)
{
	if(!m_active) {
		return; // Stopped while the request was in progress.
	}

	if(reply->error() != QNetworkReply::NoError) {
		logger()->logMessage(Log()
								 .about(Log::Level::Error, Log::Topic::ExtBan)
								 .message(QStringLiteral("Network error: %1")
											  .arg(reply->errorString())));
		return;
	}

	int statusCode =
		reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString etag = QString::fromUtf8(reply->rawHeader("ETag"));
	if(statusCode == 304) {
		if(etag != cacheKey) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(QStringLiteral("Response ETag '%1' does not match "
											"given If-None-Match '%2'")
								 .arg(etag, cacheKey)));
		}
		return;
	}

	if(statusCode != 200) {
		logger()->logMessage(
			Log()
				.about(Log::Level::Error, Log::Topic::ExtBan)
				.message(QStringLiteral("Unexpected status code %1")
							 .arg(statusCode)));
		return;
	}

	QJsonParseError err;
	QByteArray body = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if(err.error != QJsonParseError::NoError) {
		logger()->logMessage(
			Log()
				.about(Log::Level::Error, Log::Topic::ExtBan)
				.message(QStringLiteral("Error parsing response: %1")
							 .arg(err.errorString())));
		return;
	}

	if(!doc.isObject()) {
		logger()->logMessage(
			Log()
				.about(Log::Level::Error, Log::Topic::ExtBan)
				.message(QStringLiteral("Top-level element is not an object")));
		return;
	}

	QJsonObject object = doc.object();
	m_config->setExternalBans(handleBans(object));
	m_config->setConfigString(config::ExtBansCacheUrl, cacheUrl);
	m_config->setConfigString(config::ExtBansCacheKey, etag);
	m_config->setConfigString(
		config::ExtBansCacheResponse, QString::fromUtf8(body));
}

QVector<ExtBan> ExtBans::handleBans(const QJsonObject &object)
{
	QVector<ExtBan> bans;
	for(QJsonValue value : object[QStringLiteral("bans")].toArray()) {
		QJsonValue rawId = value[QStringLiteral("id")];
		int id = rawId.toInt();
		if(id <= 0) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(QStringLiteral("Invalid ban id '%1'")
								 .arg(compat::debug(rawId))));
			continue;
		}

		QJsonValue rawIps = value[QStringLiteral("ips")];
		QVector<BanIpRange> ips;
		if(!handleIps(rawIps.toArray(), false, ips)) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(
						QStringLiteral("Invalid ip ban in id '%1'").arg(id)));
			continue;
		}

		QJsonValue rawIpsExcluded = value[QStringLiteral("ipsexcluded")];
		QVector<BanIpRange> ipsExcluded;
		if(!handleIps(rawIpsExcluded.toArray(), true, ipsExcluded)) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(QStringLiteral("Invalid ip exclude in id '%1'")
								 .arg(id)));
			continue;
		}

		QJsonValue rawSystem = value[QStringLiteral("system")];
		QVector<BanSystemIdentifier> system = handleSystem(rawSystem.toArray());

		QJsonValue rawUsers = value[QStringLiteral("users")];
		QVector<BanUser> users = handleUsers(rawUsers.toArray());

		if(ips.isEmpty() && system.isEmpty() && users.isEmpty()) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(QStringLiteral("Empty ban with id '%1'").arg(id)));
			continue;
		}

		QJsonValue rawExpires = value[QStringLiteral("expires")];
		QDateTime expires = ServerConfig::parseDateTime(rawExpires.toString());
		if(expires.isNull()) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Warn, Log::Topic::ExtBan)
					.message(QStringLiteral("Invalid expires '%1' in id '%2'")
								 .arg(compat::debug(rawExpires))
								 .arg(id)));
			continue;
		}

		bans.append({
			id,
			ips,
			ipsExcluded,
			system,
			users,
			expires,
			value[QStringLiteral("comment")].toString(),
			value[QStringLiteral("reason")].toString(),
		});
	}
	return bans;
}

bool ExtBans::handleIps(
	const QJsonArray &array, bool exclude, QVector<BanIpRange> &outRanges)
{
	for(QJsonValue value : array) {
		if(!value.isObject()) {
			return false;
		}

		QJsonObject object = value.toObject();
		QHostAddress from(object[QStringLiteral("from")].toString());
		if(from.isNull()) {
			return false;
		}

		QHostAddress to(object[QStringLiteral("to")].toString());
		if(to.isNull()) {
			return false;
		}

		BanReaction reaction =
			exclude ? BanReaction::NotBanned
					: ServerConfig::parseReaction(
						  object[QStringLiteral("reaction")].toString());
		outRanges.append({from, to, reaction});
	}
	return true;
}

QVector<BanSystemIdentifier> ExtBans::handleSystem(const QJsonArray &array)
{
	QVector<BanSystemIdentifier> system;
	for(QJsonValue value : array) {
		const QJsonObject object = value.toObject();
		QSet<QString> sids;
		for(const QJsonValue &rawSid :
			object[QStringLiteral("sids")].toArray()) {
			QString sid = rawSid.toString();
			if(!sid.isEmpty()) {
				sids.insert(sid);
			}
		}

		if(!sids.isEmpty()) {
			BanReaction reaction = ServerConfig::parseReaction(
				object[QStringLiteral("reaction")].toString());
			system.append({sids, reaction});
		}
	}
	return system;
}

QVector<BanUser> ExtBans::handleUsers(const QJsonArray &array)
{
	QVector<BanUser> users;
	for(QJsonValue value : array) {
		const QJsonObject object = value.toObject();
		QSet<long long> ids;
		for(const QJsonValue &rawId : object[QStringLiteral("ids")].toArray()) {
			long long id = rawId.toDouble(-1.0);
			if(id >= 0) {
				ids.insert(id);
			}
		}

		if(!ids.isEmpty()) {
			BanReaction reaction = ServerConfig::parseReaction(
				object[QStringLiteral("reaction")].toString());
			users.append({ids, reaction});
		}
	}
	return users;
}

ServerLog *ExtBans::logger()
{
	return m_config->logger();
}

void ExtBans::setUrl(const QString &url)
{
	if(url.isEmpty()) {
		m_url.clear();
	} else {
		m_url = QUrl::fromUserInput(url);
		if(m_url.isEmpty()) {
			logger()->logMessage(
				Log()
					.about(Log::Level::Error, Log::Topic::ExtBan)
					.message(QStringLiteral("Invalid URL '%1'").arg(url)));
		}
	}
	update();
}

void ExtBans::setIntervalSecs(int intervalSecs)
{
	int clamped = qBound(MIN_INTERVAL, intervalSecs, MAX_INTERVAL);
	if(intervalSecs != clamped) {
		logger()->logMessage(
			Log()
				.about(Log::Level::Warn, Log::Topic::ExtBan)
				.message(QStringLiteral("Given ExtBans interval %1 seconds "
										"clamped to %2 seconds")
							 .arg(intervalSecs)
							 .arg(clamped)));
	}
	m_intervalMsecs = clamped * 1000;
	update();
}

void ExtBans::update()
{
	if(!m_started || m_url.isEmpty() || m_intervalMsecs < MIN_INTERVAL * 1000) {
		m_refreshTimer->stop();
		if(m_active) {
			m_active = false;
			emit deactivated();
		}
	} else if(!m_active) {
		m_active = true;
		refresh();
	}
}
}
