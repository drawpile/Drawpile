// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_PROJECTSAVER_H
#define LIBCLIENT_EXPORT_PROJECTSAVER_H
#include "libclient/drawdance/canvasstate.h"
#include "libshared/net/message.h"
#include <QElapsedTimer>
#include <QObject>
#include <QRunnable>

struct DP_CanvasState;
struct DP_ProjectWorker;

// The project saver can save the project in two ways: either through the paint
// engine, which integrates with auto-recording and also saves messages, or as a
// runnable that just saves the canvas state to a project file without
// additional messages.
class ProjectSaver final : public QObject, public QRunnable {
	Q_OBJECT
public:
	explicit ProjectSaver(const QString &path, QObject *parent = nullptr);

	net::Message getProjectSaveRequestMessage();

	void setCanvasState(const drawdance::CanvasState &canvasState)
	{
		m_canvasState = canvasState;
	}

	void run() override;

Q_SIGNALS:
	void saveSucceeded(qint64 elapsedMsec);
	void saveCancelled();
	void saveFailed(const QString &errorMessage);

private:
	void handleSaveRequest(
		DP_CanvasState *cs, DP_ProjectWorker *pw, unsigned int fileId);

	static void handleSaveRequestCallback(
		void *user, DP_CanvasState *cs, DP_ProjectWorker *pw,
		unsigned int fileId);

	int handleSaveStart();

	static int handleSaveStartCallback(void *user, const char **outPath);

	int handleSaveFinish(int saveResult);

	static int handleSaveFinishCallback(void *user, int saveResult);

	bool writeBackFromTemporaryFile();

	const QString m_path;
	drawdance::CanvasState m_canvasState;
	QElapsedTimer m_saveTimer;
	QString m_tempPath;
	QByteArray m_tempPathBytes;
};

#endif
