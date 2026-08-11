// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_RECORDINGCONVERTER_H
#define LIBCLIENT_IMPORT_RECORDINGCONVERTER_H
#include "libclient/drawdance/canvasstate.h"
#include "libclient/utils/tempfile.h"
#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

namespace net {
class Message;
}

namespace impex {

class RecordingConverter final : public QObject, public QRunnable {
	Q_OBJECT
public:
	RecordingConverter(
		const QStringList &paths,
		const QSharedPointer<utils::TempFile> &outputFile, bool saveSnapshots,
		QObject *parent = nullptr);

	void run() override;

	void cancel();

Q_SIGNALS:
	void conversionSucceeded();
	void conversionCancelled();
	void
	conversionFailed(const QString &message, const QString &detail = QString());
	void conversionProgress(int percent);

private:
	bool isCancelled() { return m_cancelled.loadRelaxed(); }

	static bool shouldRecordMessage(const net::Message &msg);

	QStringList m_paths;
	QSharedPointer<utils::TempFile> m_tempFile;
	QAtomicInt m_cancelled;
	bool m_saveSnapshots;
};

}

#endif
