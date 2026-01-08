// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_PROJECTSAVER_H
#define LIBCLIENT_EXPORT_PROJECTSAVER_H
#include "libshared/net/message.h"
#include <QElapsedTimer>
#include <QObject>

struct DP_CanvasState;
struct DP_ProjectWorker;

class ProjectSaver final : public QObject {
	Q_OBJECT
public:
	explicit ProjectSaver(const QString &path, QObject *parent = nullptr);

	net::Message getProjectSaveRequestMessage();

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

	void handleSaveStart();

	static void handleSaveStartCallback(void *user);

	void handleSaveFinish(int saveResult);

	static void handleSaveFinishCallback(void *user, int saveResult);

	const QString m_path;
	QElapsedTimer m_saveTimer;
};

#endif
