// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_PROJECTREPAIR_H
#define LIBCLIENT_PROJECT_PROJECTREPAIR_H
#include "libclient/io/tempfile.h"
#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <dpdb/sql_qt.h>

struct DP_Mutex;
struct DP_Project;

namespace project {

class ProjectRepair final : public QObject, public QRunnable {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ProjectRepair)
public:
	enum class Status {
		Ok,
		Cancelled,
		ErrorInputPath,
		ErrorInputFile,
		ErrorOutputPath,
		ErrorVerifyOpen,
		ErrorVerifyClose,
		ErrorRepairOpen,
		ErrorRepair,
	};

	enum class FileType {
		Unknown,
		Project,
		Canvas,
	};

	ProjectRepair(const QString &path, QObject *parent = nullptr);

	Status status() const { return m_status; }
	FileType fileType() const { return m_fileType; }
	bool verified() const { return m_verified; }
	const QString &errorMessage() const { return m_errorMessage; }
	io::TempFileHolder &repairedFile() { return m_repairedFile; }

	void run() override;

	void cancel();

Q_SIGNALS:
	void progress(qint64 inputSize, qint64 repairedSize);
	void finished();

private:
	Status runRepair();
	bool verify();

	bool isCancelled() const { return m_cancel.loadRelaxed(); }

	QString m_path;
	QString m_errorMessage;
	drawdance::Database m_db;
	io::TempFileHolder m_repairedFile = nullptr;
	QAtomicInt m_cancel;
	Status m_status = Status::Ok;
	FileType m_fileType = FileType::Unknown;
	bool m_verified = false;
};

}

#endif
