// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/canvastoimagerunnable.h"

CanvasToImageRunnable::CanvasToImageRunnable(
	const drawdance::CanvasState &canvasState, unsigned int correlationId,
	QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_correlationId(correlationId)
{
}

void CanvasToImageRunnable::run()
{
	QImage img;
	if(!m_canvasState.isNull() && !m_canvasState.size().isEmpty()) {
		img = m_canvasState.toFlatImage();
	}
	Q_EMIT finished(img, m_correlationId);
}
