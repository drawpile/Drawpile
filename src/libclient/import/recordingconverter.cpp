// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/input.h>
#include <dpcommon/timing.h>
#include <dpengine/player.h>
#include <dpengine/project.h>
#include <dpmsg/message.h>
}
#include "libclient/import/loadresult.h"
#include "libclient/import/recordingconverter.h"
#include "libclient/net/message.h"
#include <QUuid>
#include <memory>

namespace {

struct PlayerCleanup {
	void operator()(DP_Player *player) const { DP_player_free(player); }
};

struct ProjectCleanup {
	void operator()(DP_Project *project) const { DP_project_close(project); }
};

}

namespace impex {

RecordingConverter::RecordingConverter(
	const QString &path, const QSharedPointer<utils::TempFile> &tempFile,
	QObject *parent)
	: QObject(parent)
	, m_path(path)
	, m_tempFile(tempFile)
{
}

void RecordingConverter::run()
{
	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	QByteArray pathBytes = m_path.toUtf8();
	DP_Input *input = DP_file_input_new_from_path(pathBytes.constData());
	if(!input) {
		Q_EMIT conversionFailed(
			tr("Failed to open input file."), QString::fromUtf8(DP_error()));
		return;
	}

	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	DP_LoadResult loadResult;
	std::unique_ptr<DP_Player, PlayerCleanup> player(DP_player_new(
		DP_PLAYER_TYPE_GUESS, pathBytes.constData(), input, &loadResult));
	if(!player) {
		Q_EMIT conversionFailed(
			tr("Failed to open recording: %1")
				.arg(getLoadResultMessage(loadResult)),
			QString::fromUtf8(DP_error()));
		return;
	}

	if(!DP_player_compatible(player.get())) {
		Q_EMIT conversionFailed(tr("Incompatible recording."));
		return;
	}

	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	QString outputPath = m_tempFile->path();
	QByteArray outputPathBytes = outputPath.toUtf8();
	DP_ProjectOpenResult projectOpenResult =
		DP_project_open(outputPathBytes.constData(), DP_PROJECT_OPEN_TRUNCATE);
	if(!projectOpenResult.project) {
		Q_EMIT conversionFailed(
			tr("Error %1 opening project file %2.")
				.arg(QString::number(projectOpenResult.error), outputPath),
			QString::fromUtf8(DP_error()));
		return;
	}
	std::unique_ptr<DP_Project, ProjectCleanup> project(
		projectOpenResult.project);

	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	int result = DP_project_session_open(
		project.get(), DP_PROJECT_SOURCE_FILE,
		QUuid::createUuid().toByteArray(QUuid::Id128).constData(),
		DP_PROTOCOL_VERSION, DP_PROJECT_SESSION_FLAG_CONVERTED);
	if(result != 0) {
		Q_EMIT conversionFailed(
			tr("Error %1 opening project session.").arg(result),
			QString::fromUtf8(DP_error()));
		return;
	}

	long long snapshotId = DP_project_snapshot_open(
		project.get(), DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT |
						   DP_PROJECT_SNAPSHOT_FLAG_CONVERTED);
	if(snapshotId <= 0LL) {
		Q_EMIT conversionFailed(
			tr("Error %1 opening project snapshot.").arg(snapshotId),
			QString::fromUtf8(DP_error()));
		return;
	}

	result = DP_project_snapshot_finish(project.get(), snapshotId);
	if(result != 0) {
		Q_EMIT conversionFailed(
			tr("Error %1 finishing project snapshot.").arg(result),
			QString::fromUtf8(DP_error()));
		return;
	}

	// The timing information is all wrong, but the dprec format doesn't contain
	// much in the way of that. We just start at the current time and add
	// guessed timing and intervals on top of that.
	long long timeMsec = DP_time_unix_msec();
	bool nextHasTime = true;
	int lastPercent = 0;
	while(true) {
		if(isCancelled()) {
			Q_EMIT conversionCancelled();
			return;
		}

		net::Message msg;
		DP_PlayerResult playerResult;
		{
			DP_Message *rawMsg = nullptr;
			playerResult = DP_player_step(player.get(), true, &rawMsg);
			if(rawMsg) {
				msg = net::Message::noinc(rawMsg);
			}
		}

		if(playerResult == DP_PLAYER_SUCCESS) {
			if(nextHasTime) {
				timeMsec += DP_message_guess_msecs(msg.get(), &nextHasTime);
			} else {
				nextHasTime = true;
			}

			if(shouldRecordMessage(msg)) {
				result = DP_project_message_record(
					project.get(), double(timeMsec) / 1000.0, msg.get(),
					DP_PROJECT_MESSAGE_FLAG_CONVERTED, nullptr);
				if(result != 0) {
					Q_EMIT conversionFailed(
						tr("Error %1 converting recording"),
						QString::fromUtf8(DP_error()));
					return;
				}
			}

		} else if(playerResult == DP_PLAYER_RECORDING_END) {
			break;

		} else if(playerResult == DP_PLAYER_ERROR_PARSE) {
			qWarning("Error parsing recording message: %s", DP_error());

		} else {
			Q_EMIT conversionFailed(
				tr("Error %1 reading recording.").arg(int(playerResult)),
				QString::fromUtf8(DP_error()));
			return;
		}

		int percent =
			qBound(0, int(DP_player_progress(player.get()) * 100.0 + 0.5), 100);
		if(percent != lastPercent) {
			lastPercent = percent;
			Q_EMIT conversionProgress(percent);
		}
	}

	if(lastPercent != 100) {
		Q_EMIT conversionProgress(100);
	}

	result = DP_project_session_close(project.get(), 0u);
	if(result != 0) {
		Q_EMIT conversionFailed(
			tr("Error %1 closing project session.").arg(result),
			QString::fromUtf8(DP_error()));
		return;
	}

	if(!DP_project_close(project.release())) {
		Q_EMIT conversionFailed(
			tr("Error closing project."), QString::fromUtf8(DP_error()));
		return;
	}

	Q_EMIT conversionSucceeded();
}

void RecordingConverter::cancel()
{
	m_cancelled.storeRelaxed(1);
}

bool RecordingConverter::shouldRecordMessage(const net::Message &msg)
{
	if(msg.isInControlRange()) {
		return false;
	}

	switch(msg.type()) {
	case DP_MSG_INTERVAL:
		return false;
	default:
		return true;
	}
}

}
