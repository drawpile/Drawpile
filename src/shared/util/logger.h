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
#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QSharedPointer>

/**
 * @brief A simple logging interface for the server
 */
class Logger
{
public:
	enum LogLevel {LOG_NONE, LOG_ERROR, LOG_WARNING, LOG_DEBUG};
	Logger();
	virtual ~Logger() = default;

	void setLogLevel(LogLevel level);

	void logError(const QString &msg);
	void logWarning(const QString &msg);
	void logDebug(const QString &msg);

protected:
	virtual void printMessage(LogLevel level, const QString &message) = 0;

private:
	LogLevel _level;
};

typedef QSharedPointer<Logger> SharedLogger;

/**
 * @brief A do-nothing logger
 */
class DummyLogger : public Logger
{
protected:
	void printMessage(LogLevel level, const QString &message) {
		Q_UNUSED(level);
		Q_UNUSED(message);
	}
};

/**
 * @brief A concrete logger that prints to stdout/stderr
 */
class ConsoleLogger : public Logger
{
protected:
	void printMessage(LogLevel level, const QString &message);
};

#endif // LOGGER_H
