// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/output.h>
#include <dpengine/project.h>
#include <dpengine/project_worker.h>
#include <dpimpex/image_impex.h>
}
#include "libclient/canvas/paintengine.h"
#include "libclient/config/config.h"
#include "libclient/project/projecthandler.h"
#include "libclient/project/recoverymodel.h"
#include "libshared/util/paths.h"
#include "libshared/util/ulid.h"
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

Q_LOGGING_CATEGORY(
	lcDpProjectWorker, "net.drawpile.project.projecthandler", QtDebugMsg)

namespace project {

ProjectHandler::ProjectHandler(config::Config *cfg, QObject *parent)
	: QObject(parent)
{
	CFG_BIND_SET(
		cfg, AutoRecordMetadataIntervalMinutes, this,
		ProjectHandler::setMetadataTimerIntervalMinutes);
	CFG_BIND_SET(
		cfg, AutoRecordSnapshotIntervalMinutes, this,
		ProjectHandler::setSnapshotTimerIntervalMinutes);
}

ProjectHandler::~ProjectHandler()
{
	DP_project_worker_free_join(m_pw);
}

bool ProjectHandler::startProjectRecording(
	canvas::PaintEngine *paintEngine, int sourceType, const QString &protocol,
	QString *outError)
{
	if(m_pw) {
		if(outError) {
			*outError = tr("Autosave recording already active");
		}
		return false;
	}

	m_path = searchAvailableProjectPath();
	if(m_path.isEmpty()) {
		if(outError) {
			*outError = tr("Could not find any available autosave path");
		}
		return false;
	}

	m_pw = DP_project_worker_new(
		&ProjectHandler::handleEventCallback,
		&ProjectHandler::writeThumbnailCallback, this);
	if(!m_pw) {
		if(outError) {
			*outError = tr("Could not start autosave recording: %1")
							.arg(QString::fromUtf8(DP_error()));
		}
		return false;
	}

	RecoveryModel::addOpenedAutosavePath(m_path);

	m_metadataPath = m_path + QStringLiteral(".meta");
	m_thumbnailPath = m_path + QStringLiteral(".thumb");
	QFile::remove(m_metadataPath);
	QFile::remove(m_thumbnailPath);

	if(!initMetaDb()) {
		m_metaDb.close();
	}

	m_fileId = DP_project_worker_open(
		m_pw, m_path.toUtf8().constData(), DP_PROJECT_OPEN_TRUNCATE);
	qCDebug(lcDpProjectWorker, "Opened file id %u", m_fileId);

	DP_project_worker_session_open(
		m_pw, m_fileId, sourceType,
		QUuid::createUuid().toByteArray(QUuid::Id128).constData(),
		protocol.toUtf8().constData(), 0u);

	paintEngine->startProjectRecording(m_pw, m_fileId);

	m_metadataTimer = new QTimer(this);
	m_metadataTimer->setTimerType(Qt::VeryCoarseTimer);
	m_metadataTimer->setSingleShot(true);
	connect(
		m_metadataTimer, &QTimer::timeout, this,
		&ProjectHandler::metadataRequested);

	m_snapshotTimer = new QTimer(this);
	m_snapshotTimer->setTimerType(Qt::VeryCoarseTimer);
	m_snapshotTimer->setSingleShot(true);
	connect(
		m_snapshotTimer, &QTimer::timeout, this,
		&ProjectHandler::snapshotRequested);

	return true;
}

bool ProjectHandler::stopProjectRecording(
	canvas::PaintEngine *paintEngine, bool remove)
{
	if(!m_pw) {
		return false;
	}

	qCDebug(lcDpProjectWorker, "Cancel project recording");
	delete m_metadataTimer;
	delete m_snapshotTimer;
	m_metadataTimer = nullptr;
	m_snapshotTimer = nullptr;
	paintEngine->stopProjectRecording();
	DP_project_worker_close(m_pw, m_fileId);
	DP_project_worker_free_join(m_pw);
	m_pw = nullptr;

	m_metaDb.close();

	RecoveryModel::removeOpenedAutosavePath(m_path);

	if(remove && !m_path.isEmpty()) {
		qCDebug(lcDpProjectWorker, "Removing '%s'", qUtf8Printable(m_path));
		QFile file(m_path);
		if(file.exists() && !file.remove()) {
			if(shouldEmitErrorBlock()) {
				Q_EMIT errorOccurred(tr("Error removing project file '%1': %2")
										 .arg(m_path, file.errorString()));
			}
		} else {
			// Just try to remove these and ignore any failures. Leftover
			// files can be cleaned up elsewhere, they're not critical.
			QFile::remove(m_metadataPath);
			QFile::remove(m_thumbnailPath);
		}
	} else {
		qCDebug(lcDpProjectWorker, "Retaining '%s'", qUtf8Printable(m_path));
	}

	return true;
}

void ProjectHandler::startMetadataTimer()
{
	if(isMetadataTimerReady()) {
		qCDebug(
			lcDpProjectWorker, "Starting thumbnail timer with %d minute(s)",
			m_metadataTimerIntervalMinutes);
		m_metadataTimer->start(minutesToMsecs(m_metadataTimerIntervalMinutes));
	}
}

void ProjectHandler::stopMetadataTimer()
{
	if(m_metadataTimer) {
		m_metadataTimer->stop();
	}
}

bool ProjectHandler::isMetadataTimerReady() const
{
	return m_metadataTimer && !m_metadataTimer->isActive();
}

void ProjectHandler::startSnapshotTimer()
{
	if(isSnapshotTimerReady()) {
		qCDebug(
			lcDpProjectWorker, "Starting snapshot timer with %d minute(s)",
			m_snapshotTimerIntervalMinutes);
		m_snapshotTimer->start(minutesToMsecs(m_snapshotTimerIntervalMinutes));
	}
}

void ProjectHandler::stopSnapshotTimer()
{
	if(m_snapshotTimer) {
		m_snapshotTimer->stop();
	}
}

bool ProjectHandler::isSnapshotTimerReady() const
{
	return m_snapshotTimer && !m_snapshotTimer->isActive();
}

void ProjectHandler::unblockErrors()
{
	m_errorsBlocked.storeRelaxed(0);
}

void ProjectHandler::setMetadatum(
	const QString &name, const drawdance::Query::Param &value)
{
	drawdance::DatabaseLocker locker(m_metaDb);
	if(m_metaDb.isOpen()) {
		drawdance::Query qry = m_metaDb.queryWithoutLock();
		qry.exec(
			"insert into metadata (name, value) values (?, ?)\n"
			"    on conflict do update set value = excluded.value",
			{name, value});
	}
}

void ProjectHandler::addMetadataSource(
	int sourceType, const QString &sourceParam)
{
	drawdance::DatabaseLocker locker(m_metaDb);
	if(m_metaDb.isOpen()) {
		drawdance::Query qry = m_metaDb.queryWithoutLock();
		qry.exec(
			"insert into sources (updated_at, source_type, source_param)\n"
			"    values (unixepoch(), ?, ?)\n"
			"    on conflict do update set updated_at = excluded.updated_at",
			{sourceType, sourceParam});
	}
}

void ProjectHandler::setMetadataTimerIntervalMinutes(
	int metadataTimerIntervalMinutes)
{
	setTimerIntervalMinutes(
		metadataTimerIntervalMinutes, m_metadataTimerIntervalMinutes,
		m_metadataTimer);
}

void ProjectHandler::setSnapshotTimerIntervalMinutes(
	int snapshotTimerIntervalMinutes)
{
	setTimerIntervalMinutes(
		snapshotTimerIntervalMinutes, m_snapshotTimerIntervalMinutes,
		m_snapshotTimer);
}

void ProjectHandler::setTimerIntervalMinutes(
	int newMinutes, int &inOutMinutes, QTimer *timer)
{
	if(newMinutes < TIMER_INTERVAL_MINUTES_MIN) {
		newMinutes = TIMER_INTERVAL_MINUTES_MIN;
	} else if(newMinutes > TIMER_INTERVAL_MINUTES_MAX) {
		newMinutes = TIMER_INTERVAL_MINUTES_MAX;
	}

	int oldMinutes = inOutMinutes;
	if(newMinutes != oldMinutes) {
		inOutMinutes = newMinutes;
		if(timer) {
			// Only manipulate the timer if it isn't close to finishing.
			int remainingTimeMsec = timer->remainingTime();
			if(remainingTimeMsec > minutesToMsecs(1)) {
				int newTimeMsec = qMax(
					0, remainingTimeMsec +
						   minutesToMsecs(newMinutes - oldMinutes));
				qCDebug(
					lcDpProjectWorker, "Restarting timer from %d to %d msec(s)",
					remainingTimeMsec, newTimeMsec);
				timer->start(newTimeMsec);
			}
		}
	}
}

bool ProjectHandler::initMetaDb()
{
	if(m_metaDb.open(m_metadataPath, QStringLiteral("autosave metadata"))) {
		drawdance::Query qry = m_metaDb.queryWithoutLock();
		return qry.exec("pragma journal_mode = off") &&
			   qry.exec("pragma synchronous = off") &&
			   qry.exec(
				   "create table if not exists metadata (\n"
				   "    name text primary key not null,\n"
				   "    value any) strict") &&
			   qry.exec(
				   "create table if not exists sources (\n"
				   "    id integer primary key not null,\n"
				   "    updated_at integer not null,\n"
				   "    source_type integer not null,\n"
				   "    source_param text not null,\n"
				   "    unique (source_type, source_param)) strict");
	} else {
		return false;
	}
}

void ProjectHandler::handleEvent(const DP_ProjectWorkerEvent *event)
{
	DP_ProjectWorkerEventType type = event->type;
	switch(type) {
	case DP_PROJECT_WORKER_EVENT_OPEN_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 opening project: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_CLOSE_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 closing project: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_WRITE_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 writing to project: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_OPEN_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 opening session: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_CLOSE_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 closing session: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 recording command: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR:
		emitError(
			//: %1 is an error code, %2 is a more detailed error message.
			QT_TR_NOOP("Error %1 creating snapshot: %2"), event->data.error);
		return;
	case DP_PROJECT_WORKER_EVENT_THUMBNAIL_MAKE_ERROR:
		qCWarning(
			lcDpProjectWorker, "Error %d generating thumbnail: %s",
			int(event->data.error.error), event->data.error.message);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE_ERROR:
		qCWarning(
			lcDpProjectWorker, "Error %d updating session times: %s",
			int(event->data.error.error), event->data.error.message);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE:
		setMetadatum(
			QStringLiteral("own_work_minutes"), event->data.own_work_minutes);
		return;
	}
	qCWarning(lcDpProjectWorker, "Unhandled event type %d", int(type));
}

void ProjectHandler::handleEventCallback(
	void *user, const DP_ProjectWorkerEvent *event)
{
	static_cast<ProjectHandler *>(user)->handleEvent(event);
}

bool ProjectHandler::writeThumbnail(DP_Image *thumb)
{
	QByteArray thumbnailPathBytes = m_thumbnailPath.toUtf8();
	DP_Output *output =
		DP_file_output_save_new_from_path(thumbnailPathBytes.constData());
	if(!output) {
		return false;
	}

	if(!DP_image_write_project_thumbnail(nullptr, thumb, output)) {
		DP_output_free_discard(output);
		return false;
	}

	return DP_output_free(output);
}

bool ProjectHandler::writeThumbnailCallback(void *user, DP_Image *thumb)
{
	return static_cast<ProjectHandler *>(user)->writeThumbnail(thumb);
}

void ProjectHandler::emitError(
	const char *messageTemplate, const DP_ProjectWorkerEventError &e)
{
	if(shouldEmitErrorBlock()) {
		Q_EMIT errorOccurred(
			tr(messageTemplate)
				.arg(QString::number(e.error), QString::fromUtf8(e.message)));
	}
}

bool ProjectHandler::shouldEmitErrorBlock()
{
	return m_errorsBlocked.testAndSetRelaxed(0, 1);
}

QString ProjectHandler::searchAvailableProjectPath()
{
	int attempts = 20;
	for(int i = 0; i < attempts; ++i) {
		QString fileName = Ulid::make().toString() + QStringLiteral(".dppr");
		QString path =
			utils::paths::writablePath(QStringLiteral("autosave"), fileName);
		if(path.isEmpty()) {
			qCWarning(
				lcDpProjectWorker, "Failed to get path for '%s'",
				qUtf8Printable(fileName));
		} else if(QFileInfo::exists(path)) {
			qCWarning(
				lcDpProjectWorker, "Path '%s' already exists",
				qUtf8Printable(path));
		} else {
			qCDebug(lcDpProjectWorker, "Got path '%s'", qUtf8Printable(path));
			return path;
		}
	}
	qCWarning(
		lcDpProjectWorker, "Failed to get path after %d attempts", attempts);
	return QString();
}

}
