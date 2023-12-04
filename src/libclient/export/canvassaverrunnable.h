// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_CANVASSAVERRUNNABLE_H
#define LIBCLIENT_EXPORT_CANVASSAVERRUNNABLE_H
extern "C" {
#include <dpengine/save.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QByteArray>
#include <QObject>
#include <QRunnable>

/**
 * @brief A runnable for saving a canvas in a background thread
 */
class CanvasSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	CanvasSaverRunnable(
		const drawdance::CanvasState &canvasState, DP_SaveImageType type,
		const QString &path, QObject *parent = nullptr);

	void run() override;

	static QString saveResultToErrorString(DP_SaveResult result);

signals:
	/**
	 * @brief Emitted once the file has been saved
	 * @param error the error message (blank string if no error occurred)
	 */
	void saveComplete(const QString &error);

private:
	static bool
	bakeAnnotation(void *user, DP_Annotation *a, unsigned char *out);

	drawdance::CanvasState m_canvasState;
	DP_SaveImageType m_type;
	QByteArray m_path;
};

#endif
