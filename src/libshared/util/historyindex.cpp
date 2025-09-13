// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/util/historyindex.h"
#include <QRegularExpression>
#include <QStringList>


HistoryIndex::HistoryIndex()
	: HistoryIndex(QString(), 0LL, 0LL)
{
}

HistoryIndex::HistoryIndex(
	const QString &sessionId, long long startId, long long historyPos)
	: m_sessionId(sessionId)
	, m_startId(startId)
	, m_historyPos(historyPos)
{
}

HistoryIndex HistoryIndex::fromString(const QString &s)
{
	HistoryIndex hi;
	hi.setFromString(s);
	return hi;
}

bool HistoryIndex::setFromString(const QString &s)
{
	static QRegularExpression re(
		QStringLiteral("\\A([0-9]+):([0-9]+):(.+)\\z"),
		QRegularExpression::DotMatchesEverythingOption);

	QRegularExpressionMatch match = re.match(s);
	if(match.hasMatch()) {
		m_startId = match.captured(1).toLongLong();
		m_historyPos = match.captured(2).toLongLong();
		m_sessionId = match.captured(3);
		return isValid();
	} else {
		clear();
		return false;
	}
}

QString HistoryIndex::toString() const
{
	if(isValid()) {
		return QStringList({QString::number(m_startId),
							QString::number(m_historyPos), m_sessionId})
			.join(':');
	} else {
		return QString();
	}
}

bool HistoryIndex::isValid() const
{
	return !m_sessionId.isEmpty() && m_startId > 0LL && m_historyPos > 0LL;
}

void HistoryIndex::clear()
{
	m_sessionId.clear();
	m_startId = 0LL;
	m_historyPos = 0LL;
}
