// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
}
#include "cmake-config/config.h"
#include "libclient/utils/logging.h"
#include "libshared/util/paths.h"
#include <QDateTime>
#include <QFile>
#include <QMessageLogContext>

namespace utils {

static DP_Mutex *const logmutex = DP_mutex_new();
static QFile *logfile;
static QtMessageHandler defaultLogger;

void logToFile(
	QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
	QString label;
	switch(type) {
	case QtDebugMsg:
		label = QStringLiteral("DEBUG");
		break;
	case QtInfoMsg:
		label = QStringLiteral("INFO");
		break;
	case QtWarningMsg:
		label = QStringLiteral("WARNING");
		break;
	case QtCriticalMsg:
		label = QStringLiteral("CRITICAL");
		break;
	case QtFatalMsg:
		label = QStringLiteral("FATAL");
		break;
	default:
		label = QStringLiteral("UNKNOWN");
		break;
	}

	QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
	QByteArray line =
		QStringLiteral("[%1 %2] %3\n").arg(ts, label, msg).toUtf8();

	DP_MUTEX_MUST_LOCK(logmutex);
	if(logfile) {
		logfile->write(line);
		logfile->flush();
	}
	DP_MUTEX_MUST_UNLOCK(logmutex);

	defaultLogger(type, ctx, msg);
}

QString logFilePath()
{
	return utils::paths::writablePath(
		QStandardPaths::AppLocalDataLocation, "logs/",
		QStringLiteral("drawpile-%1-%2.log")
			.arg(cmake_config::version())
			.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd")));
}

void enableLogFile(bool enable)
{
	if(enable && !logfile) {
		QString logpath = logFilePath();
		qInfo("Opening log file: %s", qUtf8Printable(logpath));
		QFile *f = new QFile(logpath);
		if(f->open(QIODevice::WriteOnly | QIODevice::Append)) {
			DP_MUTEX_MUST_LOCK(logmutex);
			logfile = f;
			DP_MUTEX_MUST_UNLOCK(logmutex);
			defaultLogger = qInstallMessageHandler(logToFile);
			qInfo("File logging started.");
		} else {
			qWarning("Unable to open logfile");
			delete f;
		}
	} else if(!enable && logfile) {
		qInfo("File logging stopped.");
		qInstallMessageHandler(defaultLogger);
		QFile *f = logfile;
		DP_MUTEX_MUST_LOCK(logmutex);
		logfile = nullptr;
		DP_MUTEX_MUST_UNLOCK(logmutex);
		delete f;
	}
}

}
