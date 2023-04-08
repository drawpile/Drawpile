// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CANVASSAVERRUNNABLE_H
#define CANVASSAVERRUNNABLE_H

extern "C" {
#include <dpengine/save.h>
}

#include <QObject>
#include <QRunnable>

namespace canvas { class PaintEngine; }

/**
 * @brief A runnable for saving a canvas in a background thread
 *
 * When constructed, a copy of the layerstack is made.
 */
class CanvasSaverRunnable final : public QObject, public QRunnable
{
	Q_OBJECT
public:
	CanvasSaverRunnable(const canvas::PaintEngine *pe, const QString &filename, QObject *parent = nullptr);

	void run() override;

	static QString saveResultToErrorString(DP_SaveResult result);

signals:
	/**
	 * @brief Emitted once the file has been saved
	 * @param error the error message (blank string if no error occurred)
	 */
	void saveComplete(const QString &error);

private:
	const canvas::PaintEngine *m_pe;
	QString m_filename;
};

#endif
