// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpengine/project.h>
}
#include "libclient/io/files.h"
#include "libclient/project/projectrepair.h"
#include <QFileInfo>

namespace project {

ProjectRepair::ProjectRepair(const QString &path, QObject *parent)
	: QObject(parent)
	, m_path(path)
{
}

void ProjectRepair::run()
{
	m_status = runRepair();
	Q_EMIT finished();
}

void ProjectRepair::cancel()
{
	m_cancel.storeRelaxed(1);
	m_db.cancel();
}

ProjectRepair::Status ProjectRepair::runRepair()
{
	if(isCancelled()) {
		return Status::Cancelled;
	}

	io::TempFile inputFile;
	if(!inputFile.setTemporaryPath()) {
		m_errorMessage = tr("Failed to set temporary input path.");
		return Status::ErrorInputPath;
	}

	QString inputPath = inputFile.path();
	if(!io::copyFile(m_path, inputPath, m_errorMessage)) {
		return Status::ErrorInputFile;
	}

	switch(DP_project_check_path(inputPath.toUtf8().constData()).result) {
	case DP_PROJECT_CHECK_PROJECT:
		m_fileType = FileType::Project;
		break;
	case DP_PROJECT_CHECK_CANVAS:
		m_fileType = FileType::Canvas;
		break;
	default:
		break;
	}

	if(isCancelled()) {
		return Status::Cancelled;
	}

	if(!m_db.open(inputPath, QStringLiteral("repair"))) {
		m_errorMessage = tr("Failed to open project.");
		return Status::ErrorVerifyOpen;
	}

	if(isCancelled()) {
		return Status::Cancelled;
	}

	m_verified = verify();

	if(isCancelled()) {
		return Status::Cancelled;
	}

	if(!m_db.close()) {
		m_errorMessage = tr("Failed to close project.");
		return Status::ErrorVerifyClose;
	}

	if(isCancelled()) {
		return Status::Cancelled;
	}

	m_repairedFile.setTempFile(new io::TempFile);
	if(!m_repairedFile.setTemporaryPath()) {
		m_errorMessage = tr("Failed to set temporary output path.");
		return Status::ErrorOutputPath;
	}

	if(isCancelled()) {
		return Status::Cancelled;
	}

	QString repairedPath = m_repairedFile.path();
	drawdance::SqlRecover recover;
	if(!recover.open(inputPath, repairedPath)) {
		m_errorMessage =
			tr("Failed to start repair: %1").arg(recover.errorMessage());
		return Status::ErrorRepairOpen;
	}

	qint64 inputSize = QFileInfo(inputPath).size();
	while(recover.step()) {
		if(isCancelled()) {
			return Status::Cancelled;
		}
		Q_EMIT progress(inputSize, QFileInfo(repairedPath).size());
	}

	if(recover.hasError()) {
		m_errorMessage =
			tr("Failed to repair file: %1").arg(recover.errorMessage());
		return Status::ErrorRepair;
	}

	return Status::Ok;
}

bool ProjectRepair::verify()
{
	drawdance::Query qry = m_db.queryWithoutLock();
	return qry.exec("pragma integrity_check(1)") && !isCancelled() &&
		   qry.next() && !isCancelled() &&
		   qry.columnText8(0, true) == QByteArrayLiteral("ok");
}

}
