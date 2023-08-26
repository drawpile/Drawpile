// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/export/animationsaverrunnable.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/export/canvassaverrunnable.h"

AnimationSaverRunnable::AnimationSaverRunnable(
	const drawdance::CanvasState &canvasState, SaveFn saveFn,
	const QString &filename, QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_filename(filename)
	, m_saveFn(saveFn)
	, m_cancelled(false)
{
}

void AnimationSaverRunnable::run()
{
	QByteArray pathBytes = m_filename.toUtf8();
	DP_SaveResult result =
		m_saveFn(m_canvasState.get(), pathBytes.constData(), onProgress, this);
	emit saveComplete(CanvasSaverRunnable::saveResultToErrorString(result));
}

void AnimationSaverRunnable::cancelExport()
{
	m_cancelled = true;
}

bool AnimationSaverRunnable::onProgress(void *user, double progress)
{
	AnimationSaverRunnable *saver = static_cast<AnimationSaverRunnable *>(user);
	if(saver->m_cancelled) {
		return false;
	} else {
		emit saver->progress(qRound(progress * 100));
		return true;
	}
}
