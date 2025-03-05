// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/project.h>
#include <dpengine/project_worker.h>
}
#include "libclient/canvas/projectworker.h"
#include "libshared/util/paths.h"
#include "libshared/util/ulid.h"
#include <QDir>
#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDpProjectWorker, "net.drawpile.projectworker", QtDebugMsg)

namespace canvas {

ProjectWorker *ProjectWorker::init(QObject *parent)
{
	ProjectWorker *worker = new ProjectWorker(parent);
	if(!worker->m_pw) {
		delete worker;
		return nullptr;
	}

	worker->openProject();
	return worker;
}

ProjectWorker::~ProjectWorker()
{
	bool havePath = !m_path.isEmpty();
	if(!havePath) {
		DP_project_worker_project_close(m_pw);
	}
	DP_project_worker_free_join(m_pw);
	if(havePath) {
		QFile file(m_path);
		if(!file.remove()) {
			qCWarning(
				lcDpProjectWorker, "Failed to remove project file '%s'",
				qUtf8Printable(m_path));
		}
	}
}

void ProjectWorker::openSession(
	int type, const QString &param, const QString &protocol)
{
	DP_project_worker_session_open(
		m_pw, type, param.toUtf8().constData(), protocol.toUtf8().constData(),
		0u);
}

ProjectWorker::ProjectWorker(QObject *parent)
	: QObject(parent)
	, m_pw(DP_project_worker_new(ProjectWorker::handleEventCallback, this))
{
}

void ProjectWorker::openProject()
{
	QDir dir(utils::paths::writablePath(QStringLiteral("project/")));
	if(!dir.makeAbsolute()) {
		qCWarning(
			lcDpProjectWorker, "Failed to make project directory absolute");
	}

	m_path =
		dir.filePath(QStringLiteral("%1.dppr").arg(Ulid::make().toString()));
	DP_project_worker_project_open(m_pw, m_path.toUtf8().constData(), 0u);
}

void ProjectWorker::handleEventCallback(void *user, DP_ProjectWorkerEvent *pwe)
{
	static_cast<ProjectWorker *>(user)->handleEvent(pwe);
}

void ProjectWorker::handleEvent(DP_ProjectWorkerEvent *pwe)
{
	switch(pwe->type) {
	case DP_PROJECT_WORKER_EVENT_PROJECT_OPEN:
		handleProjectOpenEvent(
			pwe->event.project_open.error, pwe->event.project_open.sql_result);
		return;
	case DP_PROJECT_WORKER_EVENT_PROJECT_CLOSE:
		handleProjectCloseEvent(pwe->event.project_close.error);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_OPEN:
		handleSessionOpenEvent(pwe->event.session_open.error);
		return;
	case DP_PROJECT_WORKER_EVENT_SESSION_CLOSE:
		handleSessionCloseEvent(pwe->event.session_close.error);
		return;
	case DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR:
		handleMessageRecordErrorEvent(pwe->event.message_record_error.error);
		return;
	}
	qWarning("Unhandled project worker event %d", int(pwe->type));
}

void ProjectWorker::handleProjectOpenEvent(int error, int sql_result)
{
	qCDebug(
		lcDpProjectWorker, "Project open event error=%d sql_result=%d", error,
		sql_result);
	if(error != 0) {
		qCWarning(
			lcDpProjectWorker, "Error %d opening project (SQL result %d): %s",
			error, sql_result, DP_error());
	}
}

void ProjectWorker::handleProjectCloseEvent(int error)
{
	qCDebug(lcDpProjectWorker, "Project close event error=%d", error);
	if(error != 0) {
		qCWarning(
			lcDpProjectWorker, "Error %d closing project: %s", error,
			DP_error());
	}
}

void ProjectWorker::handleSessionOpenEvent(int error)
{
	qCDebug(lcDpProjectWorker, "Session open event error=%d", error);
	if(error != 0) {
		qCWarning(
			lcDpProjectWorker, "Error %d opening session: %s", error,
			DP_error());
	}
}

void ProjectWorker::handleSessionCloseEvent(int error)
{
	qCDebug(lcDpProjectWorker, "Session close event error=%d", error);
	if(error != 0) {
		qCWarning(
			lcDpProjectWorker, "Error %d closing session: %s", error,
			DP_error());
	}
}

void ProjectWorker::handleMessageRecordErrorEvent(int error)
{
	qCWarning(
		lcDpProjectWorker, "Error %d recording message: %s", error, DP_error());
}

}
