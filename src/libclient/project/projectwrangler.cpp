// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpengine/project.h>
#include <dpengine/project_worker.h>
}
#include "libclient/project/projectwrangler.h"
#include "libshared/util/paths.h"
#include <QFile>
#include <QLoggingCategory>
#include <QTemporaryFile>
#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(
	lcDpProjectWrangler, "net.drawpile.project.projectwrangler", QtWarningMsg)

namespace project {

ProjectWrangler::ProjectWrangler(QObject *parent)
	: QObject(parent)
{
}

ProjectWrangler::~ProjectWrangler()
{
	qCDebug(lcDpProjectWrangler, "Delete wrangler");
	DP_project_worker_free_join(m_pw);
	DP_mutex_free(m_mutex);
	for(QTemporaryFile *tempFile : m_temporaryFiles) {
		qCWarning(
			lcDpProjectWrangler, "Delete lingering temp file '%s'",
			qUtf8Printable(tempFile->fileName()));
		delete tempFile;
	}
}

void ProjectWrangler::openProject(const QString &path)
{
	if(initWorker()) {
		DP_project_worker_sync(
			m_pw, &ProjectWrangler::handleOpenSyncCallback,
			new OpenParams{this, path});
	} else {
		Q_EMIT openErrorOccurred(tr("Initialization failed"));
	}
}

void ProjectWrangler::generateOverview()
{
	if(m_pw) {
		DP_project_worker_sync(
			m_pw, &ProjectWrangler::handleInfoSyncCallback, this);
		DP_project_worker_info(
			m_pw, m_fileId,
			DP_PROJECT_INFO_FLAG_OVERVIEW | DP_PROJECT_INFO_FLAG_WORK_TIMES,
			&ProjectWrangler::handleInfoCallback, this);
	} else {
		Q_EMIT overviewErrorOccurred(tr("Not initialized"));
	}
}

void ProjectWrangler::requestCancel(unsigned int syncId)
{
	if(m_pw) {
		DP_project_worker_cancel(m_pw, m_fileId);
		DP_project_worker_sync(
			m_pw, &ProjectWrangler::handleSyncCallback,
			new SyncParams{this, syncId});
	} else {
		Q_EMIT syncReceived(syncId);
	}
}

void ProjectWrangler::withOverviewEntries(
	const std::function<void(const QVector<OverviewEntry> &)> &fn)
{
	if(m_mutex) {
		DP_MUTEX_MUST_LOCK(m_mutex);
		fn(m_overviewEntries);
		DP_MUTEX_MUST_UNLOCK(m_mutex);
	} else {
		// Shouldn't happen, but let's not crash if it does.
		qCWarning(
			lcDpProjectWrangler, "Reading overview without initializing mutex");
		fn(m_overviewEntries);
	}
}

bool ProjectWrangler::initWorker()
{
	if(!m_mutex) {
		m_mutex = DP_mutex_new();
		if(!m_mutex) {
			qCWarning(
				lcDpProjectWrangler, "Error creating mutex: %s", DP_error());
			return false;
		}
	}

	if(!m_pw) {
		m_pw = DP_project_worker_new(
			&ProjectWrangler::handleEventCallback, nullptr, this);
		if(!m_pw) {
			qCWarning(
				lcDpProjectWrangler, "Error starting project worker: %s",
				DP_error());
			return false;
		}
	}

	return true;
}

void ProjectWrangler::handleEvent(const DP_ProjectWorkerEvent *event)
{
	DP_ProjectWorkerEventType type = event->type;
	switch(type) {
	case DP_PROJECT_WORKER_EVENT_OPEN_ERROR:
		Q_EMIT openErrorOccurred(
			tr("Error %d opening project file: %s")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		deleteTemporaryFile(event->data.file_id);
		return;
	case DP_PROJECT_WORKER_EVENT_INFO_ERROR:
		Q_EMIT overviewErrorOccurred(
			tr("Error %d generating project overview: %s")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		return;
	case DP_PROJECT_WORKER_EVENT_CLOSE_ERROR:
	case DP_PROJECT_WORKER_EVENT_WRITE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_OPEN_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_CLOSE_ERROR:
	case DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR:
	case DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR:
	case DP_PROJECT_WORKER_EVENT_THUMBNAIL_MAKE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SAVE_ERROR:
		Q_EMIT unhandledErrorOccurred(
			tr("Unhandled error %d of type %d: %s")
				.arg(
					QString::number(event->data.error.error),
					QString::number(type),
					QString::fromUtf8(event->data.error.message)));
		return;
	case DP_PROJECT_WORKER_EVENT_OPEN:
		qCDebug(lcDpProjectWrangler, "Project %u opened", event->data.file_id);
		Q_EMIT openSucceeded();
		return;
	case DP_PROJECT_WORKER_EVENT_CLOSE:
		qCDebug(lcDpProjectWrangler, "Project %u closed", event->data.file_id);
		deleteTemporaryFile(event->data.file_id);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE:
		break;
	case DP_PROJECT_WORKER_EVENT_INFO_DONE:
		DP_MUTEX_MUST_LOCK(m_mutex);
		std::sort(
			m_overviewEntries.begin(), m_overviewEntries.end(),
			&ProjectWrangler::overviewEntryLessThan);
		DP_MUTEX_MUST_UNLOCK(m_mutex);
		Q_EMIT overviewGenerated();
		return;
	}
	qCWarning(lcDpProjectWrangler, "Unhandled event %d", int(type));
}

void ProjectWrangler::handleEventCallback(
	void *user, const DP_ProjectWorkerEvent *event)
{
	static_cast<ProjectWrangler *>(user)->handleEvent(event);
}

void ProjectWrangler::handleOpenSync(const QString &path)
{
	QFile sourceFile(path);
	if(!sourceFile.open(QIODevice::ReadOnly)) {
		Q_EMIT openErrorOccurred(
			tr("Failed to open '%1': %2")
				.arg(sourceFile.fileName(), sourceFile.errorString()));
		return;
	}

	QTemporaryFile *tempFile = new QTemporaryFile;
	if(!tempFile->open()) {
		Q_EMIT openErrorOccurred(
			tr("Failed to open temporary '%1': %2")
				.arg(tempFile->fileName(), tempFile->errorString()));
		delete tempFile;
		return;
	}

	QString error;
	if(!utils::paths::copyFileContents(sourceFile, *tempFile, error)) {
		qCWarning(
			lcDpProjectWrangler,
			"Error copying file contents from '%s' to '%s': %s",
			qUtf8Printable(sourceFile.fileName()),
			qUtf8Printable(tempFile->fileName()), qUtf8Printable(error));
		Q_EMIT openErrorOccurred(
			tr("Failed to copy '%1' to temporary file '%2'")
				.arg(sourceFile.fileName(), tempFile->fileName()));
		delete tempFile;
		return;
	}

	tempFile->close();

	DP_MUTEX_MUST_LOCK(m_mutex);
	m_fileId = DP_project_worker_open(
		m_pw, tempFile->fileName().toUtf8().constData(),
		DP_PROJECT_OPEN_READ_ONLY, nullptr, 0LL);
	qCDebug(
		lcDpProjectWrangler, "Adding temporary file '%s' for file id %u",
		qUtf8Printable(tempFile->fileName()), m_fileId);
	m_temporaryFiles.insert(m_fileId, tempFile);
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

void ProjectWrangler::handleOpenSyncCallback(void *user)
{
	OpenParams *params = static_cast<OpenParams *>(user);
	ProjectWrangler *pw = params->pw;
	QString path = params->path;
	delete params;
	pw->handleOpenSync(path);
}

void ProjectWrangler::handleInfoSync()
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	m_overviewEntries.clear();
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

void ProjectWrangler::handleInfoSyncCallback(void *user)
{
	static_cast<ProjectWrangler *>(user)->handleInfoSync();
}

void ProjectWrangler::handleInfo(const DP_ProjectInfo &info)
{
	switch(info.type) {
	case DP_PROJECT_INFO_TYPE_OVERVIEW:
		DP_MUTEX_MUST_LOCK(m_mutex);
		handleInfoOverview(info.overview);
		DP_MUTEX_MUST_UNLOCK(m_mutex);
		break;
	case DP_PROJECT_INFO_TYPE_WORK_TIMES:
		DP_MUTEX_MUST_LOCK(m_mutex);
		handleInfoWorkTimes(info.work_times);
		DP_MUTEX_MUST_UNLOCK(m_mutex);
		break;
	default:
		qCWarning(
			lcDpProjectWrangler, "Unhandled project info type %d",
			int(info.type));
		break;
	}
}

void ProjectWrangler::handleInfoOverview(const DP_ProjectInfoOverview &info)
{
	OverviewEntry &oe = getOrCreateOverviewEntry(info.session_id);

	if(info.thumbnail_data && info.thumbnail_size != 0) {
		if(!oe.thumbnail.loadFromData(
			   info.thumbnail_data, uint(info.thumbnail_size))) {
			qCWarning(
				lcDpProjectWrangler,
				"Failed to load thumbnail for session id %lld",
				info.session_id);
		}
	}

	if(std::isfinite(info.opened_at) && info.opened_at > 0.0) {
		oe.openedAt = QDateTime::fromSecsSinceEpoch(qRound64(info.opened_at));
	}

	if(std::isfinite(info.closed_at) && info.closed_at > 0.0) {
		oe.closedAt = QDateTime::fromSecsSinceEpoch(qRound64(info.closed_at));
	}

	oe.protocol = QString::fromUtf8(info.protocol);
}

void ProjectWrangler::handleInfoWorkTimes(const DP_ProjectInfoWorkTimes &info)
{
	OverviewEntry &oe = getOrCreateOverviewEntry(info.session_id);
	oe.ownWorkMinutes = info.own_work_minutes;
}

OverviewEntry &ProjectWrangler::getOrCreateOverviewEntry(long long sessionId)
{
	for(OverviewEntry &oe : m_overviewEntries) {
		if(oe.sessionId == sessionId) {
			return oe;
		}
	}

	m_overviewEntries.append(OverviewEntry());
	OverviewEntry &oe = m_overviewEntries.last();
	oe.sessionId = sessionId;
	return oe;
}

void ProjectWrangler::handleInfoCallback(void *user, const DP_ProjectInfo *info)
{
	static_cast<ProjectWrangler *>(user)->handleInfo(*info);
}

void ProjectWrangler::handleSync(unsigned int syncId)
{
	Q_EMIT syncReceived(syncId);
}

void ProjectWrangler::handleSyncCallback(void *user)
{
	SyncParams *params = static_cast<SyncParams *>(user);
	ProjectWrangler *pw = params->pw;
	unsigned int syncId = params->syncId;
	delete params;
	pw->handleSync(syncId);
}

void ProjectWrangler::deleteTemporaryFile(unsigned int fileId)
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	QHash<unsigned int, QTemporaryFile *>::iterator it =
		m_temporaryFiles.find(fileId);
	if(it == m_temporaryFiles.end()) {
		qCWarning(
			lcDpProjectWrangler, "No temporary file found for file id %u",
			fileId);
	} else {
		QTemporaryFile *tempFile = *it;
		qCDebug(
			lcDpProjectWrangler, "Removing temporary file '%s' for file id %u",
			qUtf8Printable(tempFile->fileName()), fileId);
		delete tempFile;
		m_temporaryFiles.remove(fileId);
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

bool ProjectWrangler::overviewEntryLessThan(
	const OverviewEntry &a, const OverviewEntry &b)
{
	return a.sessionId < b.sessionId;
}

}
