// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_CANVASLOADERRUNNABLE
#define LIBCLIENT_IMPORT_CANVASLOADERRUNNABLE
extern "C" {
#include <dpengine/load_enums.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QObject>
#include <QRunnable>
#include <QString>

class CanvasLoaderRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	explicit CanvasLoaderRunnable(
		const QString &path, QObject *parent = nullptr);

	void run() override;

	const QString &path() const { return m_path; }
	DP_LoadResult result() const { return m_result; }
	DP_SaveImageType type() const { return m_type; }
	const drawdance::CanvasState &canvasState() const { return m_canvasState; }
	const QString &sessionSourceParam() const { return m_sessionSourceParam; }
	long long sessionSequenceId() const { return m_sessionSequenceId; }

signals:
	void loadComplete(
		const QString &error, const QString &detail, qint64 elapsedMsec);

private:
	const QString m_path;
	DP_LoadResult m_result = DP_LOAD_RESULT_BAD_ARGUMENTS;
	DP_SaveImageType m_type = DP_SAVE_IMAGE_UNKNOWN;
	drawdance::CanvasState m_canvasState;
	QString m_sessionSourceParam;
	long long m_sessionSequenceId = -1LL;
};

#endif
