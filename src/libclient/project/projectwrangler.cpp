// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpengine/project.h>
#include <dpengine/project_worker.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/project/projectwrangler.h"
#include "libshared/util/paths.h"
#include <QFile>
#include <QLoggingCategory>
#include <QTemporaryFile>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <limits>

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
	if(m_dc) {
		drawdance::DrawContextPool::releaseRaw(m_dc);
	}
	for(QTemporaryFile *tempFile : m_temporaryFiles) {
		qCWarning(
			lcDpProjectWrangler, "Delete lingering temp file '%s'",
			qUtf8Printable(tempFile->fileName()));
		delete tempFile;
	}
}

void ProjectWrangler::openProject(
	const QString &path, bool readOnly, bool copyToTemporary)
{
	if(initWorker()) {
		DP_project_worker_sync(
			m_pw, &ProjectWrangler::handleOpenSyncCallback,
			new OpenParams{this, path, readOnly, copyToTemporary});
	} else {
		Q_EMIT errorOccurred(int(Error::Open), tr("Initialization failed"));
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
		Q_EMIT errorOccurred(int(Error::Overview), tr("Not initialized"));
	}
}

void ProjectWrangler::preparePlayer(
	double maxDeltaSeconds, long long snapshotInterval)
{
	if(m_pw) {
		if(!m_dc) {
			m_dc = drawdance::DrawContextPool::acquireRaw();
		}
		DP_project_worker_player_prepare(
			m_pw, m_fileId, m_dc, maxDeltaSeconds, snapshotInterval);
	} else {
		Q_EMIT errorOccurred(int(Error::PreparePlayer), tr("Not initialized"));
	}
}

unsigned int ProjectWrangler::rewindPlayer()
{
	return controlPlayer([](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_REWIND;
	});
}

unsigned int ProjectWrangler::fastForwardPlayer()
{
	return controlPlayer([](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_FAST_FORWARD;
	});
}

unsigned int ProjectWrangler::seekPlayer(double seconds)
{
	return controlPlayer([seconds](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_SEEK;
		params.data.seek_seconds = seconds;
	});
}

unsigned int ProjectWrangler::stepPlayerMessages(int messageCount)
{
	return controlPlayer([messageCount](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_STEP_MESSAGES;
		params.data.message_count = messageCount;
	});
}

unsigned int ProjectWrangler::stepPlayerUndoPoints(int undoPointCount)
{
	return controlPlayer(
		[undoPointCount](DP_ProjectPlayerControlParams &params) {
			params.type = DP_PROJECT_PLAYER_CONTROL_STEP_UNDO_POINTS;
			params.data.undo_point_count = undoPointCount;
		});
}

unsigned int ProjectWrangler::skipPlayerSessions(int sessionDelta)
{
	return controlPlayer([sessionDelta](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_SKIP_SESSIONS;
		params.data.session_delta = sessionDelta;
	});
}

unsigned int ProjectWrangler::startPlayer()
{
	return controlPlayer([](DP_ProjectPlayerControlParams &params) {
		params.type = DP_PROJECT_PLAYER_CONTROL_PLAY;
	});
}

void ProjectWrangler::pausePlayer()
{
	if(m_mutex) {
		DP_MUTEX_MUST_LOCK(m_mutex);
		m_pauseRequested = true;
		DP_MUTEX_MUST_UNLOCK(m_mutex);
	} else {
		// Shouldn't happen, but don't crash if it does.
		qCWarning(
			lcDpProjectWrangler, "Pausing player without initializing mutex");
		m_pauseRequested = true;
	}
}

void ProjectWrangler::cancelPlayer()
{
	m_activeControlId.storeRelaxed(0);
}

void ProjectWrangler::setPlaybackSpeed(double percent)
{
	double playbackMsecMultiplier =
		percent <= 0.0 ? 0.0 : (qMax(1.0, percent) / 100.0) / 1000.0;
	if(m_mutex) {
		DP_MUTEX_MUST_LOCK(m_mutex);
		m_playbackMsecMultiplier = playbackMsecMultiplier;
		DP_MUTEX_MUST_UNLOCK(m_mutex);
	} else {
		m_playbackMsecMultiplier = playbackMsecMultiplier;
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

unsigned int ProjectWrangler::controlPlayer(
	const std::function<void(DP_ProjectPlayerControlParams &)> &fn)
{
	if(m_pw) {
		DP_MUTEX_MUST_LOCK(m_mutex);
		// Unlikely the user will issue billions of playback control actions,
		// but handle hitting the limit anyway.
		if(m_controlId >=
		   static_cast<unsigned int>(std::numeric_limits<int>::max())) {
			m_controlId = 1u;
		} else {
			++m_controlId;
		}
		m_pauseRequested = false;
		m_playbackSeconds = -1.0;
		m_activeControlId.storeRelaxed(int(m_controlId));
		DP_MUTEX_MUST_UNLOCK(m_mutex);

		DP_ProjectPlayerControlParams params;
		params.control_id = m_controlId;
		params.fn = &ProjectWrangler::handlePlayerControlCallback;
		params.user = this;
		fn(params);

		DP_project_worker_player_control(m_pw, m_fileId, &params);
		return m_controlId;

	} else {
		Q_EMIT errorOccurred(int(Error::ControlPlayer), tr("Not initialized"));
		return 0u;
	}
}

void ProjectWrangler::handleEvent(const DP_ProjectWorkerEvent *event)
{
	DP_ProjectWorkerEventType type = event->type;
	switch(type) {
	case DP_PROJECT_WORKER_EVENT_OPEN_ERROR:
		Q_EMIT errorOccurred(
			int(Error::Open),
			tr("Error %1 opening project file: %2")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		deleteTemporaryFile(event->data.file_id);
		return;
	case DP_PROJECT_WORKER_EVENT_INFO_ERROR:
		Q_EMIT errorOccurred(
			int(Error::Overview),
			tr("Error %1 generating project overview: %2")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		return;
	case DP_PROJECT_WORKER_EVENT_PLAYER_PREPARE_ERROR:
		Q_EMIT errorOccurred(
			int(Error::PreparePlayer),
			tr("Error %1 preparing player: %2")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		return;
	case DP_PROJECT_WORKER_EVENT_PLAYER_CONTROL_ERROR:
		Q_EMIT errorOccurred(
			int(Error::ControlPlayer),
			tr("Error %1 in player: %2")
				.arg(
					QString::number(event->data.error.error),
					QString::fromUtf8(event->data.error.message)));
		return;
	case DP_PROJECT_WORKER_EVENT_CLOSE_ERROR:
	case DP_PROJECT_WORKER_EVENT_WRITE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_OPEN_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_RESUME_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_CLOSE_ERROR:
	case DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR:
	case DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR:
	case DP_PROJECT_WORKER_EVENT_THUMBNAIL_MAKE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SAVE_ERROR:
	case DP_PROJECT_WORKER_EVENT_SIZE_REPORT_ERROR:
		Q_EMIT errorOccurred(
			int(Error::Unhandled),
			tr("Unhandled error %1 of type %2: %3")
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
	case DP_PROJECT_WORKER_EVENT_PLAYER_PREPARE_DONE:
		Q_EMIT playerPrepared(event->data.total_playback_seconds);
		return;
	case DP_PROJECT_WORKER_EVENT_PLAYER_CONTROL_DONE:
		Q_EMIT playerControlCompleted(event->data.control_id);
		return;
	case DP_PROJECT_WORKER_EVENT_SIZE_REPORT:
		return; // Don't care.
	}
	qCWarning(lcDpProjectWrangler, "Unhandled event %d", int(type));
}

void ProjectWrangler::handleEventCallback(
	void *user, const DP_ProjectWorkerEvent *event)
{
	static_cast<ProjectWrangler *>(user)->handleEvent(event);
}

void ProjectWrangler::handleOpenSync(
	const QString &path, bool readOnly, bool copyToTemporary)
{
	QTemporaryFile *tempFile;
	QByteArray pathToOpen;
	if(copyToTemporary) {
		QFile sourceFile(path);
		if(!sourceFile.open(QIODevice::ReadOnly)) {
			Q_EMIT errorOccurred(
				int(Error::Open),
				tr("Failed to open '%1': %2")
					.arg(sourceFile.fileName(), sourceFile.errorString()));
			return;
		}

		tempFile = new QTemporaryFile;
		if(!tempFile->open()) {
			Q_EMIT errorOccurred(
				int(Error::Open),
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
			Q_EMIT errorOccurred(
				int(Error::Open),
				tr("Failed to copy '%1' to temporary file '%2'")
					.arg(sourceFile.fileName(), tempFile->fileName()));
			delete tempFile;
			return;
		}

		tempFile->close();
		pathToOpen = tempFile->fileName().toUtf8();
	} else {
		tempFile = nullptr;
		pathToOpen = path.toUtf8();
	}

	DP_MUTEX_MUST_LOCK(m_mutex);

	m_fileId = DP_project_worker_open(
		m_pw, pathToOpen.constData(),
		DP_flag_uint(readOnly, DP_PROJECT_OPEN_READ_ONLY), nullptr, 0LL);
	qCDebug(
		lcDpProjectWrangler, "Adding temporary file for file id %u", m_fileId);
	m_temporaryFiles.insert(m_fileId, tempFile);
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

void ProjectWrangler::handleOpenSyncCallback(void *user)
{
	OpenParams *params = static_cast<OpenParams *>(user);
	ProjectWrangler *pw = params->pw;
	QString path = params->path;
	bool readOnly = params->readOnly;
	bool copyToTemporary = params->copyToTemporary;
	delete params;
	pw->handleOpenSync(path, readOnly, copyToTemporary);
}

int ProjectWrangler::handlePlayerProgress(
	DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params)
{
	Q_UNUSED(pp);
	unsigned int controlId = params.control_id;
	if(m_activeControlId.loadRelaxed() == int(controlId)) {
		Q_EMIT playerProgressed(
			controlId, int(getPlayerState(pp)),
			DP_project_player_current_playback_seconds(pp),
			DP_project_player_current_session_id(pp),
			DP_project_player_current_sequence_id(pp));
		return DP_PROJECT_PLAYER_PROGRESS_CONTINUE;
	} else {
		return DP_PROJECT_PLAYER_PROGRESS_CANCEL;
	}
}

int ProjectWrangler::handlePlayerUpdate(
	DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params)
{
	unsigned int controlId = params.control_id;

	DP_MUTEX_MUST_LOCK(m_mutex);
	bool shouldHandle = controlId == m_controlId;
	DP_MUTEX_MUST_UNLOCK(m_mutex);

	if(shouldHandle) {
		double playbackSeconds = DP_project_player_current_playback_seconds(pp);

		if(params.type == DP_PROJECT_PLAYER_CONTROL_PLAY) {
			m_nextFrameTimeMsec = FRAME_TIME_MSEC;
			if(m_playbackSeconds >= 0.0) {
				m_nextFrameTimeMsec -= m_frameTimer.elapsed();
			}
			m_playbackSeconds = playbackSeconds;
			m_frameTimer.start();
		}

		DP_CanvasState *cs = DP_project_player_current_canvas_noinc(pp);
		if(cs) {
			net::MessageList localStateMsgs;
			bool localStateChanged = DP_project_player_local_state_get_reset(
				pp, &ProjectWrangler::acceptLocalStateMessage, &localStateMsgs);
			Q_EMIT playerUpdated(
				controlId, int(getPlayerState(pp)),
				drawdance::CanvasState::inc(cs), playbackSeconds,
				DP_project_player_current_session_id(pp),
				DP_project_player_current_sequence_id(pp), localStateChanged,
				localStateMsgs);
		} else {
			qCWarning(
				lcDpProjectWrangler,
				"No canvas state in player control callback");
		}
	}

	return 0;
}

ProjectWrangler::PlayerState
ProjectWrangler::getPlayerState(DP_ProjectPlayer *pp)
{
	switch(DP_project_player_state(pp)) {
	case DP_PROJECT_PLAYER_STATE_AT_END:
		return PlayerState::End;
	case DP_PROJECT_PLAYER_STATE_ERROR:
		return PlayerState::Error;
	default:
		return PlayerState::Ok;
	}
}

int ProjectWrangler::updatePlayback(DP_ProjectPlayer *pp)
{
	// Kind of complicated and definitely very wrong timing logic. The results
	// look okay enough though, so I'm not going to mess with it further.
	// Somebody who is better at math can maybe do that at some point.
	//
	// UPDATE means that a frame will be emitted to the canvas and playback will
	// continue. If there are pending dabs to paint in a multidab operation,
	// they will be flushed, which may delay the frame being emitted by a bit.
	//
	// SKIP means that the playback will keep going without emitting a frame.
	//
	// PAUSE means that playback will stop and the current frame will be emitted
	// to the canvas.
	//
	// playbackSeconds is the time that the player is currently at.
	//
	// m_playbackSeconds is the time of the last emitted frame.
	//
	// m_playbackMsecMultiplier is a ratio to go from real time to playback
	// time, taking into account the playback speed. As a special case, a value
	// less than or equal to zero means "uncapped" speed.
	//
	// m_nextFrameTimeMsec is how many milliseconds the current frame is
	// supposed to take. This may be zero.
	//
	// m_frameTimer counts the elapsed time since the last emitted frame.
	//
	double playbackSeconds = DP_project_player_current_playback_seconds(pp);

	// If we haven't emitted a frame yet, our m_playbackSeconds will be -1.0. If
	// we reach the next frame time, we emit a frame. If we somehow time-travel
	// backwards, we also emit a frame.
	bool needsUpdate = m_playbackSeconds < 0.0 ||
					   playbackSeconds < m_playbackSeconds ||
					   m_nextFrameTimeMsec <= 0LL ||
					   m_frameTimer.hasExpired(m_nextFrameTimeMsec);
	if(needsUpdate) {
		return DP_PROJECT_PLAYER_PLAY_UPDATE;
	}

	// If the playback doesn't reach our next frame time yet, don't emit it,
	// just keep playing it back.
	double nextPlaybackSeconds =
		m_playbackSeconds +
		(double(m_nextFrameTimeMsec) * m_playbackMsecMultiplier);
	if(playbackSeconds < nextPlaybackSeconds) {
		return DP_PROJECT_PLAYER_PLAY_SKIP;
	}

	// Otherwise, we have a frame, but aren't supposed to show it yet, so stall.
	while(true) {
		// Sleep for at most one frame to handle pause requests quickly. We
		// should probably also somehow account for the time the frame is
		// actually supposed to be played back and shorten the sleep time to
		// that if it's before the next frame. Unsure how to math that correctly
		// in the face of varying playback speed though.
		qint64 sleepTime =
			qMax(1LL, m_nextFrameTimeMsec - m_frameTimer.elapsed());
		DP_MUTEX_MUST_UNLOCK(m_mutex);
		QThread::msleep(static_cast<unsigned long>(sleepTime));
		DP_MUTEX_MUST_LOCK(m_mutex);

		if(m_pauseRequested) {
			// The user pressed pause, bail out.
			return DP_PROJECT_PLAYER_PLAY_PAUSE;

		} else if(m_playbackMsecMultiplier <= 0.0) {
			// The user changed the playback speed to uncapped.
			return DP_PROJECT_PLAYER_PLAY_UPDATE;

		} else {
			// Emit the frame if we stalled long enough.
			m_playbackSeconds +=
				double(m_frameTimer.elapsed()) * m_playbackMsecMultiplier;
			if(playbackSeconds <= m_playbackSeconds) {
				return DP_PROJECT_PLAYER_PLAY_UPDATE;
			}
		}
	}
}

int ProjectWrangler::updatePlaybackUncapped()
{
	// Uncapped playback speed.
	if(m_nextFrameTimeMsec <= 0LL ||
	   m_frameTimer.hasExpired(m_nextFrameTimeMsec)) {
		return DP_PROJECT_PLAYER_PLAY_UPDATE;
	} else {
		return DP_PROJECT_PLAYER_PLAY_SKIP;
	}
}

int ProjectWrangler::handlePlayerPlay(
	DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params)
{
	unsigned int controlId = params.control_id;

	int result;
	DP_MUTEX_MUST_LOCK(m_mutex);

	if(controlId == m_controlId) {
		if(m_pauseRequested) {
			result = DP_PROJECT_PLAYER_PLAY_PAUSE;
		} else if(m_playbackMsecMultiplier > 0.0) {
			result = updatePlayback(pp);
		} else {
			result = updatePlaybackUncapped();
		}
	} else {
		result = DP_PROJECT_PLAYER_PLAY_SKIP;
	}

	DP_MUTEX_MUST_UNLOCK(m_mutex);
	return result;
}

int ProjectWrangler::handlePlayerControlCallback(
	void *user, DP_ProjectPlayer *pp,
	const DP_ProjectPlayerControlParams *params, int type)
{
	ProjectWrangler *pw = static_cast<ProjectWrangler *>(user);
	switch(type) {
	case DP_PROJECT_PLAYER_CONTROL_CALLBACK_PROGRESS:
		return pw->handlePlayerProgress(pp, *params);
	case DP_PROJECT_PLAYER_CONTROL_CALLBACK_UPDATE:
		return pw->handlePlayerUpdate(pp, *params);
	case DP_PROJECT_PLAYER_CONTROL_CALLBACK_PLAY:
		return pw->handlePlayerPlay(pp, *params);
	}
	qCWarning(
		lcDpProjectWrangler, "Unhandled project player control type %d", type);
	return 0;
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
		if(tempFile) {
			qCDebug(
				lcDpProjectWrangler,
				"Removing temporary file '%s' for file id %u",
				qUtf8Printable(tempFile->fileName()), fileId);
			delete tempFile;
		}
		m_temporaryFiles.remove(fileId);
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

bool ProjectWrangler::overviewEntryLessThan(
	const OverviewEntry &a, const OverviewEntry &b)
{
	return a.sessionId < b.sessionId;
}

bool ProjectWrangler::acceptLocalStateMessage(void *user, DP_Message *msg)
{
	net::MessageList *localStateMsgs = static_cast<net::MessageList *>(user);
	localStateMsgs->append(net::Message::noinc(msg));
	return true;
}

}
