// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTHANDLER_H
#define LIBCLIENT_PROJECT_PROJECTHANDLER_H
#include <QAtomicInt>
#include <QObject>
#include <libshared/util/database.h>

class QTimer;
struct DP_Image;
struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;
struct DP_ProjectWorkerEventError;

namespace canvas {
class PaintEngine;
}

namespace project {

class ProjectHandler final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectHandler)
public:
	explicit ProjectHandler(QObject *parent = nullptr);
	~ProjectHandler() override;

	const QString &path() const { return m_path; }

	bool startProjectRecording(
		canvas::PaintEngine *paintEngine, int sourceType,
		const QString &protocol, QString *outError = nullptr);

	bool stopProjectRecording(canvas::PaintEngine *paintEngine, bool remove);

	// To avoid compounding errors, the emitting of error signals is disabled
	// when one occurs. Call this to re-enable error emitting.
	void unblockErrors();

	void startThumbnailTimer();
	void stopThumbnailTimer();
	bool isThumbnailTimerReady() const;

	void saveToProjectFile(const QString &path);

	void
	setMetadatum(const QString &name, const drawdance::Query::Param &value);

	void addMetadataSource(int sourceType, const QString &sourceParam);

Q_SIGNALS:
	void thumbnailRequested();
	// The following signals are emitted from the worker thread, use
	// Qt::QueuedConnection to attach to them.
	void errorOccurred(const QString &message);

private:
	bool initMetaDb();

	void handleEvent(const DP_ProjectWorkerEvent *event);

	static void
	handleEventCallback(void *user, const DP_ProjectWorkerEvent *event);

	bool writeThumbnail(DP_Image *thumb);

	static bool writeThumbnailCallback(void *user, DP_Image *thumb);

	void
	emitError(const char *messageTemplate, const DP_ProjectWorkerEventError &e);

	bool shouldEmitErrorBlock();

	static QString searchAvailableProjectPath();

	DP_ProjectWorker *m_pw = nullptr;
	QTimer *m_thumbnailTimer = nullptr;
	QString m_path;
	QString m_metadataPath;
	QString m_thumbnailPath;
	drawdance::Database m_metaDb;
	QAtomicInt m_errorsBlocked = 0;
	unsigned int m_fileId = 0;
};

}

#endif
