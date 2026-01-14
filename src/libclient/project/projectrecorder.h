// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTRECORDER_H
#define LIBCLIENT_PROJECT_PROJECTRECORDER_H
#include <QAtomicInt>
#include <QObject>
#include <libshared/util/database.h>

class QTimer;
struct DP_Image;
struct DP_Output;
struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;
struct DP_ProjectWorkerEventError;

namespace canvas {
class PaintEngine;
}

namespace config {
class Config;
}

namespace project {

class ProjectRecorder final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectRecorder)
public:
	explicit ProjectRecorder(config::Config *cfg, QObject *parent = nullptr);
	~ProjectRecorder() override;

	const QString &path() const { return m_path; }

	bool startProjectRecording(
		canvas::PaintEngine *paintEngine, int sourceType,
		const QString &protocol, QString *outError = nullptr);

	bool stopProjectRecording(canvas::PaintEngine *paintEngine, bool remove);

	// To avoid compounding errors, the emitting of error signals is disabled
	// when one occurs. Call this to re-enable error emitting.
	void unblockErrors();

	void startMetadataTimer();
	void stopMetadataTimer();
	bool isMetadataTimerReady() const;

	void startSnapshotTimer();
	void stopSnapshotTimer();
	bool isSnapshotTimerReady() const;

	void
	setMetadatum(const QString &name, const drawdance::Query::Param &value);

	void addMetadataSource(int sourceType, const QString &sourceParam);

Q_SIGNALS:
	void metadataRequested();
	void snapshotRequested();
	// The following signals are emitted from the worker thread, use
	// Qt::QueuedConnection to attach to them.
	void errorOccurred(const QString &message);

private:
	static constexpr int TIMER_INTERVAL_MINUTES_MIN = 1;
	static constexpr int TIMER_INTERVAL_MINUTES_MAX = 1440;

	void setMetadataTimerIntervalMinutes(int metadataTimerIntervalMinutes);
	void setSnapshotTimerIntervalMinutes(int snapshotTimerIntervalMinutes);

	static void
	setTimerIntervalMinutes(int newMinutes, int &inOutMinutes, QTimer *timer);

	static constexpr int minutesToMsecs(int minutes) { return minutes * 60000; }

	bool initMetaDb();

	void handleEvent(const DP_ProjectWorkerEvent *event);

	static void
	handleEventCallback(void *user, const DP_ProjectWorkerEvent *event);

	bool writeThumbnail(DP_Image *thumb, DP_Output *outputOrNull);

	static bool writeThumbnailCallback(
		void *user, DP_Image *thumb, DP_Output *outputOrNull);

	void
	emitError(const char *messageTemplate, const DP_ProjectWorkerEventError &e);

	bool shouldEmitErrorBlock();

	static QString searchAvailableProjectPath();

	DP_ProjectWorker *m_pw = nullptr;
	int m_metadataTimerIntervalMinutes = 0;
	int m_snapshotTimerIntervalMinutes = 0;
	QTimer *m_metadataTimer = nullptr;
	QTimer *m_snapshotTimer = nullptr;
	QString m_path;
	QString m_metadataPath;
	QString m_thumbnailPath;
	drawdance::Database m_metaDb;
	QAtomicInt m_errorsBlocked = 0;
	unsigned int m_fileId = 0;
};

}

#endif
