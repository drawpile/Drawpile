/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <iostream>

#include "logger.h"

namespace logger {

namespace {
	static LogLevel loglevel = LOG_INFO;

	static const char *LOG_PREFIX[] = {
		"",
		"ERROR",
		"WARNING",
		"INFO"
	};

	void printLog(LogLevel level, const QString &msg) {
		std::cerr << LOG_PREFIX[level] << ": " << msg.toLocal8Bit().constData() << std::endl;
	}
}

void setLogLevel(LogLevel level) { loglevel = level; }

Logger::Logger(LogLevel level)
{
	Q_ASSERT(level >= LOG_ERROR && level <= LOG_INFO);
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

}

