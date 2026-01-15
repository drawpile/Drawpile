// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#define LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#include <QAtomicInt>
#include <QHash>
#include <QObject>
#include <libshared/util/database.h>

class QTemporaryFile;
struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;
struct DP_ProjectWorkerEventError;

namespace project {

class ProjectWrangler final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectWrangler)
public:
	explicit ProjectWrangler(QObject *parent = nullptr);
	~ProjectWrangler() override;

	void requestCancel(unsigned int syncId);

	// To avoid compounding errors, the emitting of error signals is disabled
	// when one occurs. Call this to re-enable error emitting.
	void unblockErrors();

Q_SIGNALS:
	// The following signals are emitted from the worker thread, use
	// Qt::QueuedConnection to attach to them.
	void errorOccurred(const QString &message);
	void syncReceived(unsigned int syncId);

private:
	void handleEvent(const DP_ProjectWorkerEvent *event);

	static void
	handleEventCallback(void *user, const DP_ProjectWorkerEvent *event);

	void
	emitError(const char *messageTemplate, const DP_ProjectWorkerEventError &e);

	bool shouldEmitErrorBlock();

	DP_ProjectWorker *m_pw = nullptr;
	QHash<unsigned int, QTemporaryFile *> m_temporaryFiles;
	QAtomicInt m_errorsBlocked = 0;
	unsigned int m_fileId = 0;
};

}

#endif
