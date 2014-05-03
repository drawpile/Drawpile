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

#include "logger.h"

#include <iostream>
#include <QHostAddress>

namespace logger {

namespace {
	LogLevel loglevel = LOG_INFO;

	const char *LOG_PREFIX[] = {
		"",
		"ERROR",
		"WARNING",
		"INFO",
		"DEBUG"
	};

	void defaultPrintLog(LogLevel level, const QString &msg) {
		std::cerr << LOG_PREFIX[level] << ": " << msg.toLocal8Bit().constData() << std::endl;
	}

	LogFunction printLog = defaultPrintLog;
}

void setLogLevel(LogLevel level) { loglevel = level; }
void setLogPrinter(LogFunction fn) { printLog = fn; }

Logger::Logger(LogLevel level)
{
	Q_ASSERT(level >= LOG_ERROR && level <= LOG_DEBUG);
	if(level <= loglevel)
		stream = new Stream(level);
	else
		stream = 0;
}

Logger::Logger(const Logger &other)
	: stream(other.stream)
{
	if(stream)
		++stream->ref;
}

Logger &Logger::operator=(const Logger &other)
{
	if(other.stream != stream) {
		if(stream) {
			Q_ASSERT(stream->ref>0);
			if(--stream->ref==0) {
				printLog(stream->level, stream->str);
				delete stream;
			}
		}
	}
	stream = other.stream;
	++stream->ref;
	return *this;
}

Logger::~Logger()
{
	if(stream) {
		Q_ASSERT(stream->ref>0);
		if(--stream->ref==0) {
			printLog(stream->level, stream->str);
			delete stream;
		}
	}
}

Logger &Logger::operator<<(const QHostAddress &a) {
	if(stream) {
		stream->ts << a.toString();
	}
	return maybeSpace();
}

Logger &Logger::operator <<(const LogId &id)
{
	if(stream) {
		stream->ts << id.typestr << " #" << id.id;
		if(!id.name.isEmpty()) {
			if(id.name.length() > 10)
				stream->ts << " (" << id.name.left(7) << "...)";
			else
				stream->ts << " (" << id.name << ")";
		}
	}
	return maybeSpace();
}


}
