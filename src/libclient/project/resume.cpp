// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/project/resume.h"
#include "libclient/project/metadata.h"
#include "libclient/project/projectrecorder.h"
#include "libclient/project/recoverymodel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace project {

QString getAutoResumePath(const QString &baseDir)
{
	// Autoresume depends on the platform. At the time of writing, it's enabled
	// on Android due to its propensity to terminate the application in the
	// background and users probably rarely exiting the application cleanly.
	if constexpr(ProjectRecorder::isAutoresumeEnabled()) {
		QFileInfoList candidates;
		bool ok = true;

		// The entries don't include files opened by the current process.
		for(const QFileInfo &fi :
			RecoveryModel::entryInfoListForBaseDir(baseDir)) {
			// The file may be locked or unopenable, don't try to resume those.
			RecoveryStatus status =
				RecoveryModel::checkRecoveryStatus(fi.filePath());
			if(status == RecoveryStatus::Available) {
				// Only files that have a resume marker can be opened, which are
				// "regular" autorecovery files. The marker will not exist if
				// the recording has been stopped or a previous autoresume
				// attempt failed, since we remove the markers here.
				QFile resumeFile(fi.filePath() + QStringLiteral(".resume"));
				if(resumeFile.exists()) {
					if(resumeFile.remove()) {
						candidates.append(fi);
					} else {
						ok = false;
						qWarning(
							"Error removing '%s': %s",
							qUtf8Printable(resumeFile.fileName()),
							qUtf8Printable(resumeFile.errorString()));
					}
				}
			}
		}

		if(ok && candidates.size() == 1) {
			return candidates.constFirst().filePath();
		}
	}

	return QString();
}

ResumeMetadata getResumeMetadata(const QString &dpprPath)
{
	ResumeMetadata rm;
	retrieveMetadata(dpprPath, [&rm](drawdance::Query &qry) {
		retrieveMetadataString(qry, "save_path", rm.savePath);
		retrieveMetadataString(qry, "export_path", rm.exportPath);
		retrieveMetadataString(qry, "project_path", rm.projectPath);
		retrieveMetadataString(
			qry, "continue_source_param", rm.continueSourceParam);
		retrieveMetadataLongLong(
			qry, "continue_sequence_id", rm.continueSequenceId);
		retrieveMetadataInt(qry, "save_type", rm.saveType);
		retrieveMetadataInt(qry, "export_type", rm.exportType);
	});
	return rm;
}

}
