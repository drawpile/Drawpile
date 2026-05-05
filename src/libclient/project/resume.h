// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_RESUME_H
#define LIBCLIENT_PROJECT_RESUME_H
#include <QString>

namespace project {

struct ResumeMetadata {
	QString savePath;
	QString exportPath;
	QString projectPath;
	QString continueSourceParam;
	long long continueSequenceId = 0LL;
	int saveType = 0;
	int exportType = 0;
};

// Retrieves a path to attempt to resume on startup. This will delete any resume
// marker files, so calling this function multiple times is not idempotent! It
// must be called only once during startup. If autoresume is disabled, there's
// not exactly one resumable files or some other error occurs, this returns an
// empty string. Locked autorecovery files are ignored, they cause no errors.
QString getAutoResumePath(const QString &baseDir);

ResumeMetadata getResumeMetadata(const QString &dpprPath);

}

#endif
