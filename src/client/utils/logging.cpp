/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#include "logging.h"
#include "config.h"

#include <QMessageLogContext>
#include <QDateTime>
#include <QStandardPaths>

#include <cstdio>

namespace utils {

static FILE *logfile;
static QtMessageHandler defaultLogger;

void logToFile(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
	const QByteArray m = msg.toLocal8Bit();
	const QByteArray ts = QDateTime::currentDateTime().toString("hh:mm:ss").toLocal8Bit();
	switch(type) {
		case QtDebugMsg:
			fprintf(logfile, "[%s DEBUG] %s:%u (%s): %s\n", ts.constData(), ctx.file, ctx.line, ctx.function, m.constData());
			break;
		case QtInfoMsg:
			fprintf(logfile, "[%s INFO] %s\n", ts.constData(), m.constData());
			break;
		case QtWarningMsg:
			fprintf(logfile, "[%s WARNING] %s\n", ts.constData(), m.constData());
			break;
		case QtCriticalMsg:
			fprintf(logfile, "[%s CRITICAL] %s\n", ts.constData(), m.constData());
			break;
		case QtFatalMsg:
			fprintf(logfile, "[%s FATAL] %s\n", ts.constData(), m.constData());
			break;
	}
	defaultLogger(type, ctx, msg);
}

void initLogging()
{
	const QByteArray logpath =
		(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
		("/drawpile-" DRAWPILE_VERSION " ") + QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log")
		.toLocal8Bit();

	qInfo("Opening log file: %s", logpath.constData());
	logfile = fopen(logpath.constData(), "a");
	if(!logfile) {
		qWarning("Unable to open logfile");

	} else {
		defaultLogger = qInstallMessageHandler(logToFile);
		qInfo("Drawpile started.");
	}
}

}

