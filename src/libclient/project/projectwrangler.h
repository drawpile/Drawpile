// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#define LIBCLIENT_PROJECT_PROJECTWRANGLER_H
#include <QAtomicInt>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QVector>
#include <functional>

class QTemporaryFile;
struct DP_Mutex;
struct DP_ProjectInfo;
struct DP_ProjectInfoOverview;
struct DP_ProjectInfoWorkTimes;
struct DP_ProjectWorker;
struct DP_ProjectWorkerEvent;
struct DP_ProjectWorkerEventError;

namespace project {

struct OverviewEntry {
	long long sessionId = 0LL;
	long long ownWorkMinutes = -1LL;
	QPixmap thumbnail;
	QDateTime openedAt;
	QDateTime closedAt;
	QString protocol;
};

class ProjectWrangler final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectWrangler)
public:
	explicit ProjectWrangler(QObject *parent = nullptr);
	~ProjectWrangler() override;

	void openProject(const QString &path);

	void generateOverview();

	void requestCancel(unsigned int syncId);

	void withOverviewEntries(
		const std::function<void(const QVector<OverviewEntry> &)> &fn);

Q_SIGNALS:
	// The following signals are emitted from the worker thread, use
	// Qt::QueuedConnection to attach to them.
	void openErrorOccurred(const QString &errorMessage);
	void overviewErrorOccurred(const QString &errorMessage);
	void unhandledErrorOccurred(const QString &errorMessage);
	void syncReceived(unsigned int syncId);
	void openSucceeded();
	void overviewGenerated();

private:
	struct OpenParams {
		ProjectWrangler *pw;
		QString path;
	};

	struct SyncParams {
		ProjectWrangler *pw;
		unsigned int syncId;
	};

	bool initWorker();

	void handleEvent(const DP_ProjectWorkerEvent *event);

	static void
	handleEventCallback(void *user, const DP_ProjectWorkerEvent *event);

	void handleOpenSync(const QString &path);

	static void handleOpenSyncCallback(void *user);

	void handleInfoSync();

	static void handleInfoSyncCallback(void *user);

	void handleInfo(const DP_ProjectInfo &info);
	void handleInfoOverview(const DP_ProjectInfoOverview &info);
	void handleInfoWorkTimes(const DP_ProjectInfoWorkTimes &info);
	OverviewEntry &getOrCreateOverviewEntry(long long sessionId);

	static void handleInfoCallback(void *user, const DP_ProjectInfo *info);

	void handleSync(unsigned int syncId);

	static void handleSyncCallback(void *user);

	void deleteTemporaryFile(unsigned int fileId);

	static bool
	overviewEntryLessThan(const OverviewEntry &a, const OverviewEntry &b);

	DP_ProjectWorker *m_pw = nullptr;
	DP_Mutex *m_mutex = nullptr;
	QHash<unsigned int, QTemporaryFile *> m_temporaryFiles;
	QVector<OverviewEntry> m_overviewEntries;
	unsigned int m_fileId = 0;
};

}

#endif
