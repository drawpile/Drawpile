// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_RECORDINGCONVERTER_H
#define LIBCLIENT_IMPORT_RECORDINGCONVERTER_H
#include "libclient/drawdance/project.h"
#include "libclient/io/tempfile.h"
#include <QAtomicInt>
#include <QHash>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>

namespace drawdance {
class Project;
class ProjectMessageCompressor;
}

namespace net {
class Message;
}

namespace impex {

class RecordingConverter final : public QObject, public QRunnable {
	Q_OBJECT
public:
	struct Input {
		QString path;
		long long sessionId;
		bool project;
	};

	// Converts a single recording (dprec or dptxt) to a dppr file. Does not
	// save a thumbnail or a final snapshot, since it's intended for playback.
	RecordingConverter(
		const QString &recordingPath,
		const QSharedPointer<io::TempFile> &outputFile,
		QObject *parent = nullptr);

	// Converts multiple recordings (dprec or dptxt) and/or dppr files to a
	// single dppr file. Saves thumbnails and a final snapshot for each session.
	// Recordings are played back to produce those. If sessions from a dppr file
	// that depend on each other are inserted in an incorrect order, those are
	// played back to get the necessary starting snapshots as well.
	RecordingConverter(
		const QVector<Input> &inputs,
		const QSharedPointer<io::TempFile> &outputFile,
		QObject *parent = nullptr);

	void run() override;

	void cancel();

Q_SIGNALS:
	void conversionSucceeded();
	void conversionCancelled();
	void
	conversionFailed(const QString &message, const QString &detail = QString());
	void conversionProgressMessage(const QString &message);
	void conversionProgress(int percent);

private:
	struct ContinuedSession {
		long long targetSnapshotId;
		long long sequenceId;
		long long continueSessionId;
		long long continueSequenceId;
		bool orphaned;
	};

	struct InputProject {
		QTemporaryFile tempFile;
		QHash<long long, long long> sessionIdMapping;
		QHash<long long, long long> snapshotIdMapping;
		QVector<ContinuedSession> continuedSessions;

		void sortContinuedSessions();
	};

	bool isCancelled() { return m_cancelled.loadRelaxed(); }

	void initProgressInputCount();

	void runConversion();

	bool convertInput(
		drawdance::Project &project,
		drawdance::ProjectMessageCompressor &projectMessageCompressor, int i,
		long long &timeMsec, QString &outErrorMessage, QString &outDetail);

	bool convertProjectSession(
		drawdance::Project &project, int i, QString &outErrorMessage,
		QString &outDetail);

	InputProject *
	getInputProject(int i, QString &outErrorMessage, QString &outDetail);

	InputProject *
	openInputProject(int i, QString &outErrorMessage, QString &outDetail);

	bool convertRecording(
		drawdance::Project &project,
		drawdance::ProjectMessageCompressor &projectMessageCompressor, int i,
		long long &timeMsec, QString &outErrorMessage, QString &outDetail);

	bool fixupProject(
		drawdance::Project &project, const QString &path, InputProject *ip,
		int i, QString &outErrorMessage, QString &outDetail);

	bool makeFinalSnapshot(
		drawdance::Project &project, int i, QString &outErrorMessage,
		QString &outDetail);

	void emitConversionProgress(int i, double progress);
	void emitFixupProgress(int i, double progress);

	QString getBasename(const QString &path);

	static bool shouldRecordMessage(const net::Message &msg);

	QVector<Input> m_inputs;
	QHash<QString, InputProject *> m_inputProjects;
	QHash<QString, QString> m_basenameByPath;
	QSharedPointer<io::TempFile> m_tempFile;
	QAtomicInt m_cancelled;
	int m_progressInputCount;
	int m_lastPercent = -1;
	bool m_saveSnapshots;
	bool m_needsFinalReplay = false;
};

}

#endif
