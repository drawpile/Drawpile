// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/file.h>
#include <dpengine/canvas_state.h>
#include <dpengine/project_worker.h>
#include <dpengine/save_enums.h>
#include <dpmsg/msg_internal.h>
}
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/export/projectsaver.h"
#include <QFile>
#include <QLoggingCategory>
#include <QTemporaryFile>

Q_LOGGING_CATEGORY(
	lcDpProjectSaver, "net.drawpile.export.projectsaver", QtWarningMsg)

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
			pw, fileId, cs, &ProjectSaver::handleSaveStartCallback,
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

int ProjectSaver::handleSaveStart()
{
	m_saveTimer.start();

#ifdef Q_OS_ANDROID
	QTemporaryFile tempFile;
#else
	QTemporaryFile tempFile(m_path + QStringLiteral(".XXXXXX"));
#endif

	if(tempFile.open()) {
		qCDebug(
			lcDpProjectSaver, "Opened temporary %s",
			qUtf8Printable(tempFile.fileName()));
	} else {
		DP_error_set(
			"Failed to open temporary '%s': %s",
			qUtf8Printable(tempFile.fileName()),
			qUtf8Printable(tempFile.errorString()));
		return int(DP_SAVE_RESULT_OPEN_ERROR);
	}

	QFile saveFile(m_path);
#ifdef Q_OS_ANDROID
	// On Android, we have some kind of weird content:// URL here, so checking
	// for existence doesn't make sense.
	bool saveFileExists = true;
#else
	bool saveFileExists = saveFile.exists();
#endif
	int result;
	if(saveFileExists) {
		qCDebug(
			lcDpProjectSaver, "Target file '%s' exists, copying contents",
			qUtf8Printable(tempFile.fileName()));

		if(!saveFile.open(QIODevice::ReadOnly)) {
			DP_error_set(
				"Failed to open '%s': %s", qUtf8Printable(saveFile.fileName()),
				qUtf8Printable(saveFile.errorString()));
			return int(DP_SAVE_RESULT_OPEN_ERROR);
		}

		bool copyOk = CanvasSaverRunnable::copyFileContents(saveFile, tempFile);
		saveFile.close();
		if(copyOk) {
			result = int(DP_SAVE_RESULT_SUCCESS);
		} else {
			result = int(DP_SAVE_RESULT_WRITE_ERROR);
		}
	} else {
		qCDebug(
			lcDpProjectSaver, "Target file '%s' does not exist",
			qUtf8Printable(tempFile.fileName()));
		result = int(DP_SAVE_RESULT_SUCCESS);
	}

	m_tempPath = tempFile.fileName();
	m_tempPathBytes = m_tempPath.toUtf8();
	tempFile.setAutoRemove(false);
	return result;
}

int ProjectSaver::handleSaveStartCallback(void *user, const char **outPath)
{
	ProjectSaver *saver = static_cast<ProjectSaver *>(user);
	int result = saver->handleSaveStart();
	*outPath = saver->m_tempPathBytes.constData();
	return result;
}

int ProjectSaver::handleSaveFinish(int saveResult)
{
	if(saveResult == int(DP_SAVE_RESULT_SUCCESS)) {
		bool writeBackOk = writeBackFromTemporaryFile();
		if(!writeBackOk) {
			saveResult = int(DP_SAVE_RESULT_WRITE_ERROR);
		}
	}
	m_tempPathBytes.clear();

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
	return saveResult;
}

int ProjectSaver::handleSaveFinishCallback(void *user, int saveResult)
{
	return static_cast<ProjectSaver *>(user)->handleSaveFinish(saveResult);
}

bool ProjectSaver::writeBackFromTemporaryFile()
{
	qCDebug(
		lcDpProjectSaver, "Moving temporary '%s' to '%s'",
		qUtf8Printable(m_tempPath), qUtf8Printable(m_path));
#if defined(Q_OS_ANDROID)
	QFile sourceFile(m_tempPath);
	if(!sourceFile.open(QIODevice::ReadOnly)) {
		DP_error_set(
			"Failed to open temporary '%s': %s",
			qUtf8Printable(sourceFile.fileName()),
			qUtf8Printable(sourceFile.errorString()));
		sourceFile.remove();
		return false;
	}

	QFile targetFile(m_path);
	if(!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		DP_error_set(
			"Failed to open '%s': %s", qUtf8Printable(targetFile.fileName()),
			qUtf8Printable(targetFile.errorString()));
		sourceFile.remove();
		return false;
	}

	bool copyOk = CanvasSaverRunnable::copyFileContents(sourceFile, targetFile);
	targetFile.close();
	sourceFile.close();
	return copyOk;
#elif defined(Q_OS_WIN)
	return DP_file_move_win32(
		reinterpret_cast<const wchar_t *>(m_tempPath.utf16()),
		reinterpret_cast<const wchar_t *>(m_path.utf16()));
#else
	return DP_file_move_unix(
		m_tempPathBytes.constData(), m_path.toUtf8().constData());
#endif
}
