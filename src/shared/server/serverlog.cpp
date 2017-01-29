#include "serverlog.h"

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
	if(!m_user.isEmpty())
		msg += m_user;
	if(!m_user.isEmpty() && !m_session.isNull())
		msg += '@';
	if(!m_session.isNull())
		msg += m_session.toString();

	if(!m_user.isEmpty() || !m_session.isNull())
	msg += QStringLiteral(": ");

	msg += m_message;

	return msg;
}

void ServerLog::logMessage(const Log &entry)
{
	if(!m_silent) {
		QMessageLogger logger;
		switch(entry.level()) {
		case Log::Level::Error: logger.critical(qPrintable(entry.toString())); break;
		case Log::Level::Warn: logger.warning(qPrintable(entry.toString())); break;
		case Log::Level::Info: logger.info(qPrintable(entry.toString())); break;
		case Log::Level::Debug: logger.debug(qPrintable(entry.toString())); break;
		}
	}
	storeMessage(entry);
}

void InMemoryLog::setHistoryLimit(int limit)
{
	m_limit = limit;
	if(limit>0 && limit<m_history.size())
		m_history.erase(m_history.begin(), m_history.begin() + (m_history.size()-limit));
}

void InMemoryLog::storeMessage(const Log &entry)
{
	if(m_limit>0 && m_history.size()+1 >= m_limit)
		m_history.pop_front();
	m_history << entry;
}

QList<Log> InMemoryLog::getLogEntries(const QUuid &session, int offset, int limit) const
{
	QList<Log> filtered;
	const int end = limit>0 ? qMin(m_history.size(), offset+limit) : m_history.size();

	for(int i=offset;i<end;++i) {
		const Log &l = m_history.at(i);
		if(session.isNull() || session == l.session())
			filtered << l;
	}

	return filtered;
}

}
