// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#define LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#include "libclient/drawdance/canvasstate.h"
#include "libclient/net/message.h"
#include <QAtomicInt>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QVector>
#include <functional>

class QElapsedTimer;
class QTemporaryFile;
struct DP_DrawContext;
struct DP_Mutex;
struct DP_ProjectInfo;
struct DP_ProjectInfoOverview;
struct DP_ProjectInfoWorkTimes;
struct DP_ProjectPlayer;
struct DP_ProjectPlayerControlParams;
struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;
struct DP_ProjectWorkerEventError;

namespace project {

struct OverviewEntry {
	long long sessionId = 0LL;
	long long ownWorkMinutes = -1LL;
	QPixmap thumbnail;
	QDateTime openedAt;
	QDateTime closedAt;
	QString protocol;
};

class ProjectWrangler final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectWrangler)
public:
	enum class Error {
		Unhandled,
		Open,
		Overview,
		PreparePlayer,
		ControlPlayer,
	};

	enum class PlayerState { Ok, End, Error };

	explicit ProjectWrangler(QObject *parent = nullptr);
	~ProjectWrangler() override;

	void openProject(
		const QString &path, bool readOnly = true, bool copyToTemporary = true);

	void generateOverview();

	void preparePlayer(double maxDeltaSeconds, long long snapshotInterval);

	unsigned int rewindPlayer();
	unsigned int fastForwardPlayer();
	unsigned int seekPlayer(double seconds);
	unsigned int stepPlayerMessages(int messageCount);
	unsigned int stepPlayerUndoPoints(int undoPointCount);
	unsigned int skipPlayerSessions(int sessionDelta);
	unsigned int startPlayer();
	void pausePlayer();
	void cancelPlayer();

	// A value of 0 or less means uncapped speed.
	void setPlaybackSpeed(double percent);

	void requestCancel(unsigned int syncId);

	void withOverviewEntries(
		const std::function<void(const QVector<OverviewEntry> &)> &fn);

Q_SIGNALS:
	// The following signals are emitted from the worker thread, use
	// Qt::QueuedConnection to attach to them.
	void errorOccurred(int type, const QString &errorMessage);
	void syncReceived(unsigned int syncId);
	void openSucceeded();
	void overviewGenerated();
	void playerPrepared(double totalPlaybackSeconds);
	void playerProgressed(
		unsigned int controlId, int playerState, double playbackSeconds,
		long long sessionId, long long sequenceId);
	void playerUpdated(
		unsigned int controlId, int playerState,
		const drawdance::CanvasState &canvasState, double playbackSeconds,
		long long sessionId, long long sequenceId, bool localStateChanged,
		const net::MessageList &localStateMsgs);
	void playerControlCompleted(int controlId);

private:
	static constexpr int FPS = 30;
	static constexpr qint64 FRAME_TIME_MSEC = 33;

	struct OpenParams {
		ProjectWrangler *pw;
		QString path;
		bool readOnly;
		bool copyToTemporary;
	};

	struct SyncParams {
		ProjectWrangler *pw;
		unsigned int syncId;
	};

	bool initWorker();

	unsigned int controlPlayer(
		const std::function<void(DP_ProjectPlayerControlParams &)> &fn);

	void handleEvent(const DP_ProjectWorkerEvent *event);

	static void
	handleEventCallback(void *user, const DP_ProjectWorkerEvent *event);

	void
	handleOpenSync(const QString &path, bool readOnly, bool copyToTemporary);

	static void handleOpenSyncCallback(void *user);

	int handlePlayerProgress(
		DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params);

	int handlePlayerUpdate(
		DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params);

	int updatePlayback(DP_ProjectPlayer *pp);
	int updatePlaybackUncapped();

	int handlePlayerPlay(
		DP_ProjectPlayer *pp, const DP_ProjectPlayerControlParams &params);

	static int handlePlayerControlCallback(
		void *user, DP_ProjectPlayer *pp,
		const DP_ProjectPlayerControlParams *params, int type);

	static PlayerState getPlayerState(DP_ProjectPlayer *pp);

	void handleInfoSync();

	static void handleInfoSyncCallback(void *user);

	void handleInfo(const DP_ProjectInfo &info);
	void handleInfoOverview(const DP_ProjectInfoOverview &info);
	void handleInfoWorkTimes(const DP_ProjectInfoWorkTimes &info);
	OverviewEntry &getOrCreateOverviewEntry(long long sessionId);

	static void handleInfoCallback(void *user, const DP_ProjectInfo *info);

	void handleSync(unsigned int syncId);

	static void handleSyncCallback(void *user);

	void deleteTemporaryFile(unsigned int fileId);

	static bool
	overviewEntryLessThan(const OverviewEntry &a, const OverviewEntry &b);

	static bool acceptLocalStateMessage(void *user, DP_Message *msg);

	DP_ProjectWorker *m_pw = nullptr;
	DP_DrawContext *m_dc = nullptr;
	DP_Mutex *m_mutex = nullptr;
	QHash<unsigned int, QTemporaryFile *> m_temporaryFiles;
	QVector<OverviewEntry> m_overviewEntries;
	QElapsedTimer m_frameTimer;
	double m_playbackSeconds = -1.0;
	double m_playbackMsecMultiplier = 1.0 / 1000.0;
	qint64 m_nextFrameTimeMsec = 0LL;
	QAtomicInt m_activeControlId;
	unsigned int m_fileId = 0u;
	unsigned int m_controlId = 0u;
	bool m_pauseRequested = false;
};

}

#endif
