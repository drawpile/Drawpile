// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_PROJECT_H
#define LIBCLIENT_DRAWDANCE_PROJECT_H
extern "C" {
#include <dpengine/project.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QByteArray>
#include <functional>

class QString;

namespace net {
class Message;
}

namespace drawdance {

class Project final {
public:
	using CopyCallbackFn =
		std::function<int(const DP_ProjectCopyCallbackParams &)>;
	using InfoFn = std::function<void(const DP_ProjectInfo &)>;

	Project() = default;
	~Project();

	Project(const Project &) = delete;
	Project(Project &&) = delete;
	Project &operator=(const Project &) = delete;
	Project &operator=(Project &&) = delete;

	DP_Project *get() { return m_data; }

	bool open(
		const QString &path, unsigned int flags, int *outError = nullptr,
		int *outSqlResult = nullptr);

	bool close();

	int openSession(
		int sourceType, const char *sourceParam, const char *protocol,
		unsigned int flags)
	{
		return DP_project_session_open(
			m_data, sourceType, sourceParam, protocol, flags);
	}

	// Generates a UUID for the sourceParam and the current protocol version.
	int openNewSession(int sourceType, unsigned int flags);

	int closeSession(unsigned int flagsToSet = 0u)
	{
		return DP_project_session_close(m_data, flagsToSet);
	}

	long long sessionId() const { return DP_project_session_id(m_data); }

	int setSessionThumbnail(const CanvasState &canvasState, DP_DrawContext *dc);

	long long openSnapshot(unsigned int flags)
	{
		return DP_project_snapshot_open(m_data, flags);
	}

	int finishSnapshot(long long snapshotId)
	{
		return DP_project_snapshot_finish(m_data, snapshotId);
	}

	int saveSession(const CanvasState &canvasState);

	int saveSessionAt(
		long long sessionId, long long sequenceId,
		const CanvasState &canvasState);

	// Generates a UUID for the new source param.
	int copySession(
		const QString &path, long long sessionId,
		const CopyCallbackFn &callback);


	int copySessionFixReplaceSnapshot(
		long long snapshotId, long long sequenceId,
		const drawdance::CanvasState &canvasState)
	{
		return DP_project_session_copy_fix_replace_snapshot(
			m_data, snapshotId, sequenceId, canvasState.get());
	}

	int copySessionFixSnapshotContinuedSessionId(
		long long snapshotId, long long newContinuedSessionId)
	{
		return DP_project_session_copy_fix_snapshot_continued_session_id(
			m_data, snapshotId, newContinuedSessionId);
	}

	int copySessionFixLastSessionId(long long &outSessionId) const
	{
		return DP_project_session_copy_fix_last_session_id(
			m_data, &outSessionId);
	}

	int copySessionFixOrphanedContinuations()
	{
		return DP_project_session_copy_fix_orphaned_continuations(m_data);
	}

	int info(unsigned int flags, const InfoFn &fn);

	static DP_ProjectCheckResult checkPath(const QString &path);

	static QByteArray generateSourceParam();

private:
	void closeWarn();

	static int
	copyCallback(void *user, const DP_ProjectCopyCallbackParams *params);

	static void infoCallback(void *user, const DP_ProjectInfo *info);

	DP_Project *m_data = nullptr;
};

class ProjectMessageCompressor final {
public:
	ProjectMessageCompressor() = default;
	~ProjectMessageCompressor();

	ProjectMessageCompressor(const ProjectMessageCompressor &) = delete;
	ProjectMessageCompressor(ProjectMessageCompressor &&) = delete;
	ProjectMessageCompressor &
	operator=(const ProjectMessageCompressor &) = delete;
	ProjectMessageCompressor &operator=(ProjectMessageCompressor &&) = delete;

	DP_ProjectMessageCompressor *get() { return m_data; }

	bool open(Project &project);
	void close();

	bool setSessionId(long long sessionId)
	{
		return DP_project_message_compressor_session_id_set(m_data, sessionId);
	}

	int recordMessage(
		double recordedAt, const net::Message &msg, unsigned int flags);

	bool flush() { return DP_project_message_compressor_flush(m_data); }

private:
	DP_ProjectMessageCompressor *m_data = nullptr;
};

class ProjectPlayer final {
public:
	using ControlCallbackFn = std::function<int(
		DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params,
		int type)>;

	ProjectPlayer() = default;
	~ProjectPlayer();

	ProjectPlayer(const ProjectPlayer &) = delete;
	ProjectPlayer(ProjectPlayer &&) = delete;
	ProjectPlayer &operator=(const ProjectPlayer &) = delete;
	ProjectPlayer &operator=(ProjectPlayer &&) = delete;

	DP_ProjectPlayer *get() { return m_data; }

	bool open(Project &project);
	int close();

	int prepare(double maxDeltaSeconds, long long snapshotInterval)
	{
		return DP_project_player_prepare(
			m_data, maxDeltaSeconds, snapshotInterval);
	}

	double totalPlaybackSeconds() const
	{
		return DP_project_player_total_playback_seconds(m_data);
	}

	double currentPlaybackSeconds() const
	{
		return DP_project_player_current_playback_seconds(m_data);
	}

	long long currentSessionId() const
	{
		return DP_project_player_current_session_id(m_data);
	}

	long long currentSequenceId() const
	{
		return DP_project_player_current_sequence_id(m_data);
	}

	drawdance::CanvasState currentLocalCanvasState() const
	{
		return drawdance::CanvasState::noinc(
			DP_project_player_current_local_canvas_inc(m_data));
	}

	int controlFastForward(unsigned int controlId, const ControlCallbackFn &fn);

	int controlSeekIds(
		unsigned int controlId, long long sessionId, long long sequenceId,
		const ControlCallbackFn &fn);

private:
	void dispose();

	int control(
		DP_ProjectPlayerControlType type, unsigned int controlId,
		const ControlCallbackFn &fn,
		const std::function<void(DP_ProjectPlayerControlParams &)> &block);

	static int controlCallback(
		void *user, DP_ProjectPlayer *pp,
		const DP_ProjectPlayerControlParams *params, int type);

	DP_ProjectPlayer *m_data = nullptr;
};

}

#endif
