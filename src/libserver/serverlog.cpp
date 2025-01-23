// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/serverlog.h"
#include <QJsonObject>
#include <QMetaEnum>

namespace server {

QString Log::toString(bool abridged) const
{
	// Full form: timestamp level/topic user@{session}: message
	QString msg;
	if(!abridged) {
		msg += m_timestamp.toString(Qt::ISODate);
		msg += ' ';
		msg += QMetaEnum::fromType<Log::Level>().valueToKey(int(m_level));
		msg += '/';
	}

	msg += QMetaEnum::fromType<Log::Topic>().valueToKey(int(m_topic));
	msg += ' ';
	if(!m_user.isEmpty()) {
		msg += m_user;
	}
	if(!m_user.isEmpty() && !m_session.isEmpty()) {
		msg += '@';
	}
	if(!m_session.isEmpty()) {
		msg += m_session;
	}
	if(!m_user.isEmpty() || !m_session.isEmpty()) {
		msg += QStringLiteral(": ");
	}

	msg += m_message;

	return msg;
}

QJsonObject Log::toJson(JsonOptions options) const
{
	QJsonObject o;
	o[QStringLiteral("timestamp")] = m_timestamp.toString(Qt::ISODate);
	o[QStringLiteral("level")] =
		QMetaEnum::fromType<Log::Level>().valueToKey(int(m_level));
	o[QStringLiteral("topic")] =
		QMetaEnum::fromType<Log::Topic>().valueToKey(int(m_topic));
	if(!options.testFlag(NoSession) && !m_session.isEmpty())
		o[QStringLiteral("session")] = m_session;

	if(!m_user.isEmpty()) {
		if(options.testFlag(NoPrivateData)) {
			const int sep1 = m_user.indexOf(';');
			const int sep2 = m_user.indexOf(';', sep1 + 1);
			o[QStringLiteral("user")] = m_user.left(sep1) + m_user.mid(sep2);
		} else {
			o[QStringLiteral("user")] = m_user;
		}
	}

	o[QStringLiteral("message")] = m_message;
	return o;
}

void ServerLog::logMessage(const Log &entry)
{
	if(!m_silent) {
		QMessageLogger logger;
		switch(entry.level()) {
		case Log::Level::Error:
			logger.critical("%s", qUtf8Printable(entry.toString()));
			break;
		case Log::Level::Warn:
			logger.warning("%s", qUtf8Printable(entry.toString()));
			break;
		case Log::Level::Info:
			logger.info("%s", qUtf8Printable(entry.toString()));
			break;
		case Log::Level::Debug:
			logger.debug("%s", qUtf8Printable(entry.toString()));
			break;
		}
	}
	storeMessage(entry);
}

void InMemoryLog::setHistoryLimit(int limit)
{
	m_limit = limit;
	if(limit > 0 && limit < m_history.size()) {
		m_history.erase(m_history.begin() + limit, m_history.end());
	}
}

void InMemoryLog::storeMessage(const Log &entry)
{
	m_history.prepend(entry);
	if(m_limit > 0 && m_history.size() >= m_limit) {
		m_history.pop_back();
	}
}

QList<Log> InMemoryLog::getLogEntries(
	const QString &session, const QDateTime &after, Log::Level atleast,
	bool omitSensitive, bool omitKicksAndBans, int offset, int limit) const
{
	QList<Log> filtered;
	for(const Log &l : m_history) {
		if(after.isValid() && after.msecsTo(l.timestamp()) < 1000) {
			break;
		}

		if(!session.isEmpty() && session != l.session()) {
			continue;
		}

		if(l.level() > atleast) {
			continue;
		}

		if(omitSensitive && l.isSensitive()) {
			continue;
		}

		if(omitKicksAndBans && l.isKickOrBan()) {
			continue;
		}

		if(offset <= 0) {
			if(limit > 0 && filtered.size() >= limit) {
				break;
			}
			filtered << l;
		} else {
			--offset;
		}
	}
	return filtered;
}

}
