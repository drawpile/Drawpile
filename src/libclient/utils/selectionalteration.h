// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_SELECTIONALTERATION_H
#define LIBCLIENT_UTILS_SELECTIONALTERATION_H
#include "libclient/drawdance/canvasstate.h"
#include <QAtomicInt>
#include <QImage>
#include <QObject>
#include <QRunnable>

class SelectionAlteration : public QObject, public QRunnable {
	Q_OBJECT
public:
	SelectionAlteration(
		const drawdance::CanvasState &canvasState, unsigned int contextId,
		int selectionId, int expand, int kernel, int feather, bool fromEdge,
		QObject *parent = nullptr);

	void run() override;

public slots:
	void cancel();

signals:
	void success(const QImage &img, int x, int y);
	void failure(int floodFillResult);

private:
	QAtomicInt m_cancel;
	const drawdance::CanvasState m_canvasState;
	const unsigned int m_contextId;
	const int m_selectionId;
	const int m_expand;
	const int m_kernel;
	const int m_feather;
	const bool m_fromEdge;
};

#endif
