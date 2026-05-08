// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CANVASTOIMAGERUNNABLE
#define LIBCLIENT_UTILS_CANVASTOIMAGERUNNABLE
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/viewmode.h"
#include <QColor>
#include <QImage>
#include <QObject>
#include <QRunnable>

class CanvasToImageRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	CanvasToImageRunnable(
		const drawdance::CanvasState &canvasState,
		const DP_ViewModeFilter *vmfOrNulOrNull,
		const QColor &overrideBackgroundColor, unsigned int correlationId,
		QObject *parent = nullptr);

	void run() override;

Q_SIGNALS:
	void finished(const QImage &img, unsigned int correlationId);

private:
	const drawdance::CanvasState m_canvasState;
	const QColor m_overrideBackgroundColor;
	drawdance::ViewModeBuffer m_vmb;
	DP_ViewModeFilter m_vmf;
	const unsigned int m_correlationId;
};

#endif
