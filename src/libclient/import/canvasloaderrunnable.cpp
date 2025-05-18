// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/import/canvasloaderrunnable.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/global.h"
#include <QElapsedTimer>

CanvasLoaderRunnable::CanvasLoaderRunnable(const QString &path, QObject *parent)
	: QObject(parent)
	, m_path(path)
{
}

void CanvasLoaderRunnable::run()
{
	QElapsedTimer timer;
	timer.start();
	m_canvasState = drawdance::CanvasState::load(m_path, &m_result, &m_type);
	QString error, detail;
	if(m_result != DP_LOAD_RESULT_SUCCESS) {
		error = impex::getLoadResultMessage(m_result);
		if(impex::shouldIncludeLoadResultDpError(m_result)) {
			detail = QString::fromUtf8(DP_error());
		}
	}
	emit loadComplete(error, detail, timer.elapsed());
}
