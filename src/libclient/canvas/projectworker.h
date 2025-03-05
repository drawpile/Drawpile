// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECTWORKER_H
#define LIBCLIENT_PROJECTWORKER_H
#include <QObject>
#include <QString>

struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;

namespace canvas {

class ProjectWorker : public QObject {
	Q_OBJECT
public:
	static ProjectWorker *init(QObject *parent = nullptr);
	~ProjectWorker();

	ProjectWorker(const ProjectWorker &) = delete;
	ProjectWorker(ProjectWorker &&) = delete;
	ProjectWorker &operator=(const ProjectWorker &) = delete;
	ProjectWorker &operator=(ProjectWorker &&) = delete;

	void openSession(int type, const QString &param, const QString &protocol);

private:
	explicit ProjectWorker(QObject *parent);

	void openProject();

	static void handleEventCallback(void *user, DP_ProjectWorkerEvent *event);
	void handleEvent(DP_ProjectWorkerEvent *pwe);
	void handleProjectOpenEvent(int error, int sql_result);
	void handleProjectCloseEvent(int error);
	void handleSessionOpenEvent(int error);
	void handleSessionCloseEvent(int error);
	void handleMessageRecordErrorEvent(int error);

	QString m_path;
	DP_ProjectWorker *m_pw;
};

}

#endif
