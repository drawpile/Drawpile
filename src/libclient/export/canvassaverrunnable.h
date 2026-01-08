// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_CANVASSAVERRUNNABLE_H
#define LIBCLIENT_EXPORT_CANVASSAVERRUNNABLE_H
extern "C" {
#include <dpengine/save_enums.h>
}
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/viewmode.h"
#include <QByteArray>
#include <QObject>
#include <QRunnable>

class QFile;
class QTemporaryDir;

namespace canvas {
class PaintEngine;
}

/**
 * @brief A runnable for saving a canvas in a background thread
 */
class CanvasSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	CanvasSaverRunnable(
		const drawdance::CanvasState &canvasState, DP_SaveImageType type,
		const QString &path, const canvas::PaintEngine *paintEngine = nullptr,
		QTemporaryDir *tempDir = nullptr, QObject *parent = nullptr);

	~CanvasSaverRunnable() override;

	void run() override;

	static QString saveResultToErrorString(
		DP_SaveResult result, DP_SaveImageType type = DP_SAVE_IMAGE_UNKNOWN);

	static QString
	badDimensionsErrorString(int maxDimension, const QString &format);

signals:
	void saveSucceeded(qint64 elapsedMsec);
	void saveCancelled();
	void saveFailed(const QString &errorMessage);

private:
	static bool
	bakeAnnotation(void *user, DP_Annotation *a, unsigned char *out);

#ifdef Q_OS_ANDROID
	DP_SaveResult copyToTargetFile(QFile &tempFile) const;
#endif

	drawdance::CanvasState m_canvasState;
	DP_SaveImageType m_type;
	QString m_path;
	drawdance::ViewModeBuffer *m_vmb = nullptr;
	DP_ViewModeFilter m_vmf;
	QTemporaryDir *m_tempDir;
};

#endif
