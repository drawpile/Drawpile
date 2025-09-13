// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_UTIL_HISTORYINDEX_H
#define LIBSHARED_UTIL_HISTORYINDEX_H
#include <QString>

class HistoryIndex {
public:
	HistoryIndex();
	HistoryIndex(
		const QString &sessionId, long long startId, long long historyPos);

	static HistoryIndex fromString(const QString &s);
	bool setFromString(const QString &s);
	QString toString() const;

	bool isValid() const;
	void clear();

	const QString &sessionId() const { return m_sessionId; }

	long long startId() const { return m_startId; }

	long long historyPos() const { return m_historyPos; }

	void incrementHistoryPos() { ++m_historyPos; }

private:
	QString m_sessionId;
	long long m_startId;
	long long m_historyPos;
};

#endif
