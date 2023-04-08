// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/logging.h"
#include "config.h"
#include "libshared/util/paths.h"

#include <QMessageLogContext>
#include <QDateTime>
#include <QSettings>

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
			fprintf(logfile, "[%s CRITICAL] %s:%u %s\n", ts.constData(), ctx.file, ctx.line, m.constData());
			break;
		case QtFatalMsg:
			fprintf(logfile, "[%s FATAL] %s:%u %s\n", ts.constData(), ctx.file, ctx.line, m.constData());
			break;
	}
	fflush(logfile);
	defaultLogger(type, ctx, msg);
}

QByteArray logFilePath()
{
	return utils::paths::writablePath(
				QStandardPaths::AppLocalDataLocation,
				"logs/",
				("drawpile-" DRAWPILE_VERSION "-") + QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log"
			).toLocal8Bit();
}

void logMessage(int level, const char *file, uint32_t line, const char *msg)
{
	QMessageLogger l(file ? file : "", line, "");
	switch(level) {
	case 0: l.debug("%s", msg); break;
	case 1: l.info("%s", msg); break;
	case 2: l.warning("%s", msg); break;
	case 3: l.critical("%s", msg); break;
	}
}

void initLogging()
{
	if(!QSettings().value("settings/logfile", true).toBool()) {
		qInfo("Logfile disabled");
		return;
	}

	const QByteArray logpath = logFilePath();

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

