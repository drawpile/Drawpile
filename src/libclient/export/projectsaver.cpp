// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/file.h>
#include <dpengine/canvas_state.h>
#include <dpengine/project.h>
#include <dpengine/project_worker.h>
#include <dpengine/save_enums.h>
#include <dpimpex/image_impex.h>
#include <dpmsg/msg_internal.h>
}
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/export/projectsaver.h"
#include <QFile>
#include <QLoggingCategory>
#include <QTemporaryFile>

Q_LOGGING_CATEGORY(
	lcDpProjectSaver, "net.drawpile.export.projectsaver", QtWarningMsg)

ProjectSaver::ProjectSaver(
	bool append, bool saveToTemporaryFile, const QString &path, QObject *parent)
	: QObject(parent)
	, m_path(path)
	, m_append(append)
	, m_saveToTemporaryFile(saveToTemporaryFile)
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
		DP_project_worker_save_noinc(
			pw, fileId, cs, &ProjectSaver::handleSaveStartCallback,
			&ProjectSaver::handleSaveFinishCallback, this);
	} else {
		DP_canvas_state_decref_nullable(cs);
		Q_EMIT saveFailed(tr("Autosave cancelled during save"));
		deleteLater();
	}
}

void ProjectSaver::run()
{
	Q_ASSERT(!m_canvasState.isNull() || m_saveToTemporaryFile);

	int saveResult = handleSaveStart();
	if(saveResult == DP_SAVE_RESULT_SUCCESS && !m_saveToTemporaryFile) {
		int result = DP_project_save_state(
			m_canvasState.get(), m_tempPathBytes.constData(),
			DP_image_write_project_thumbnail, nullptr);
		saveResult = DP_project_save_error_to_save_result(result);
	}

	handleSaveFinish(saveResult);
}

void ProjectSaver::handleSaveRequestCallback(
	void *user, DP_CanvasState *cs, DP_ProjectWorker *pw, unsigned int fileId)
{
	static_cast<ProjectSaver *>(user)->handleSaveRequest(cs, pw, fileId);
}

int ProjectSaver::handleSaveStart()
{
	m_saveTimer.start();

	QTemporaryFile tempFile;
#ifndef Q_OS_ANDROID
	if(!m_saveToTemporaryFile) {
		tempFile.setFileTemplate(m_path + QStringLiteral(".XXXXXX"));
	}
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
	int result;
	if(shouldAppend(saveFile)) {
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
	if(saveResult != int(DP_SAVE_RESULT_SUCCESS)) {
		removeAndClearTemporaryFile();
	} else if(!m_saveToTemporaryFile) {
		bool writeBackOk = writeBackFromTemporaryFile();
		if(!writeBackOk) {
			saveResult = int(DP_SAVE_RESULT_WRITE_ERROR);
		}
	}

	switch(saveResult) {
	case int(DP_SAVE_RESULT_SUCCESS):
		Q_EMIT saveSucceeded(m_saveTimer.elapsed(), m_tempPath);
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
	return saveResult;
}

int ProjectSaver::handleSaveFinishCallback(void *user, int saveResult)
{
	ProjectSaver *saver = static_cast<ProjectSaver *>(user);
	int result = saver->handleSaveFinish(saveResult);
	saver->deleteLater();
	return result;
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
		removeAndClearTemporaryFile();
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
#elif defined(Q_OS_WIN)
	bool copyOk = DP_file_move_win32(
		reinterpret_cast<const wchar_t *>(m_tempPath.utf16()),
		reinterpret_cast<const wchar_t *>(m_path.utf16()));
#else
	bool copyOk = DP_file_move_unix(
		m_tempPathBytes.constData(), m_path.toUtf8().constData());
#endif
	removeAndClearTemporaryFile();
	return copyOk;
}

void ProjectSaver::removeAndClearTemporaryFile()
{
	if(!m_tempPath.isEmpty()) {
		QFile::remove(m_tempPath);
		m_tempPath.clear();
	}
}

bool ProjectSaver::shouldAppend(QFile &saveFile) const
{
	if(m_append) {
#ifdef Q_OS_ANDROID
		// On Android, we have some kind of weird content:// URL here, so
		// checking for existence doesn't make sense.
		Q_UNUSED(saveFile);
		return true;
#else
		return saveFile.exists();
#endif
	} else {
		return false;
	}
}
