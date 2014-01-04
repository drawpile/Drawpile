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

Logger::Logger()
	: _level(LOG_DEBUG)
{
}

void Logger::setLogLevel(LogLevel level)
{
	_level = level;
}

void Logger::logError(const QString &msg)
{
	if(_level >= LOG_ERROR)
		printMessage(LOG_ERROR, "ERROR: " + msg);
}

void Logger::logWarning(const QString &msg)
{
	if(_level >= LOG_WARNING)
		printMessage(LOG_WARNING, "WARNING: " + msg);
}

void Logger::logDebug(const QString &msg)
{
	if(_level >= LOG_DEBUG)
		printMessage(LOG_DEBUG, "DEBUG: " + msg);
}

void ConsoleLogger::printMessage(LogLevel level, const QString &message)
{
	if(level <= LOG_DEBUG)
		std::cerr << message.toLocal8Bit().constData() << std::endl;
	else
		std::cout << message.toLocal8Bit().constData() << std::endl;
}
