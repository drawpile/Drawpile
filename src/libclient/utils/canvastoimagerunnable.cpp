// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/canvastoimagerunnable.h"

CanvasToImageRunnable::CanvasToImageRunnable(
	const drawdance::CanvasState &canvasState,
	const DP_ViewModeFilter *vmfOrNull, unsigned int correlationId,
	QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_vmf(DP_view_mode_filter_clone(m_vmb.get(), vmfOrNull))
	, m_correlationId(correlationId)
{
}

void CanvasToImageRunnable::run()
{
	QImage img;
	if(!m_canvasState.isNull() && !m_canvasState.size().isEmpty()) {
		img = m_canvasState.toFlatImage(true, true, nullptr, &m_vmf);
	}
	Q_EMIT finished(img, m_correlationId);
}
