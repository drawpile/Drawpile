// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/canvas_state.h>
#include <dpengine/project_worker.h>
#include <dpengine/save_enums.h>
#include <dpmsg/msg_internal.h>
}
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/export/projectsaver.h"

ProjectSaver::ProjectSaver(const QString &path, QObject *parent)
	: QObject(parent)
	, m_path(path)
{
}

net::Message ProjectSaver::getProjectSaveRequestMessage()
{
	return net::Message::noinc(DP_msg_internal_project_save_request_new(
		0u, &ProjectSaver::handleSaveRequestCallback, this));
}

void ProjectSaver::handleSaveRequest(
	DP_CanvasState *cs, DP_ProjectWorker *pw, unsigned int fileId)
{
	if(pw && cs) {
		QByteArray pathBytes = m_path.toUtf8();
		DP_project_worker_save_noinc(
			pw, fileId, pathBytes.constData(), cs,
			&ProjectSaver::handleSaveStartCallback,
			&ProjectSaver::handleSaveFinishCallback, this);
	} else {
		DP_canvas_state_decref_nullable(cs);
		Q_EMIT saveFailed(tr("Autosave cancelled during save"));
		deleteLater();
	}
}

void ProjectSaver::handleSaveRequestCallback(
	void *user, DP_CanvasState *cs, DP_ProjectWorker *pw, unsigned int fileId)
{
	static_cast<ProjectSaver *>(user)->handleSaveRequest(cs, pw, fileId);
}

void ProjectSaver::handleSaveStart()
{
	m_saveTimer.start();
}

void ProjectSaver::handleSaveStartCallback(void *user)
{
	static_cast<ProjectSaver *>(user)->handleSaveStart();
}

void ProjectSaver::handleSaveFinish(int saveResult)
{
	switch(saveResult) {
	case int(DP_SAVE_RESULT_SUCCESS):
		Q_EMIT saveSucceeded(m_saveTimer.elapsed());
		break;
	case int(DP_SAVE_RESULT_CANCEL):
		Q_EMIT saveCancelled();
		break;
	default:
		Q_EMIT saveFailed(
			CanvasSaverRunnable::saveResultToErrorString(
				DP_SaveResult(saveResult), DP_SAVE_IMAGE_PROJECT));
	}
	m_saveTimer.invalidate();
	deleteLater();
}

void ProjectSaver::handleSaveFinishCallback(void *user, int saveResult)
{
	static_cast<ProjectSaver *>(user)->handleSaveFinish(saveResult);
}
