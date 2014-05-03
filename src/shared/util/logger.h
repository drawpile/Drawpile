/*
   DrawPile - a collaborative drawing program.

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
#ifndef DP_SHARED_LOGGING_H
#define DP_SHARED_LOGGING_H

#include <QDebug>
#include <functional>

class QHostAddress;

namespace logger {

enum LogLevel {LOG_NONE, LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG};
void setLogLevel(LogLevel level);

typedef std::function<void(LogLevel,const QString&)> LogFunction;
void setLogPrinter(LogFunction fn);

/**
 * @brief A helper struct for logging the IDs of things like sessions and clients in consistent format
 */
struct LogId {
	const char *typestr; // type of the object (e.g. Session)
	int id; // the ID number
	QString name; // the name or title of the object

	LogId() : typestr("?"), id(0), name(QString()) { }
	LogId(const char *typestr_, int id_, const QString &name_=QString()) : typestr(typestr_), id(id_), name(name_) { }
};

/**
 * \brief Stream oriented log printing class
 *
 * Based on QDebug
 */
class Logger {
private:
	struct Stream {
		Stream(LogLevel lev) : ts(&str, QIODevice::WriteOnly), ref(1), level(lev), space(true) { }

		QTextStream ts;
		QString str;
		int ref;
		LogLevel level;
		bool space;
	} *stream;
public:
	Logger(LogLevel level);
	Logger(const Logger &logger);
	Logger &operator=(const Logger &logger);
	~Logger();

    inline Logger &space() { if(stream) { stream->space = true; stream->ts << ' '; } return *this; }
    inline Logger &nospace() { if(stream) { stream->space = false; } return *this; }
    inline Logger &maybeSpace() { if (stream && stream->space) stream->ts << ' '; return *this; }

    Logger &operator<<(bool t) { if(stream) { stream->ts << (t ? "true" : "false"); } return maybeSpace(); }
    Logger &operator<<(int t) { if(stream) { stream->ts << t; } return maybeSpace(); }
    Logger &operator<<(double t) { if(stream) { stream->ts << t; } return maybeSpace(); }
	Logger &operator<<(const char* t) { if(stream) { stream->ts << QString::fromLocal8Bit(t); } return maybeSpace(); }
    Logger &operator<<(const QString & t) { if(stream) { stream->ts << '\"' << t  << '\"'; } return maybeSpace(); }
	Logger &operator<<(const QHostAddress &a);
	Logger &operator<<(const LogId &id);
};

inline Logger error() { return Logger(LOG_ERROR); }
inline Logger warning() { return Logger(LOG_WARNING); }
inline Logger info() { return Logger(LOG_INFO); }
inline Logger debug() { return Logger(LOG_DEBUG); }

}

#endif

