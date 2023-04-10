// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/export/animationsaverrunnable.h"
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/canvas/paintengine.h"

AnimationSaverRunnable::AnimationSaverRunnable(const canvas::PaintEngine *pe, SaveFn saveFn, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_vmb(),
	  m_filename(filename),
	  m_saveFn(saveFn),
	  m_cancelled(false)
{
}

void AnimationSaverRunnable::run()
{
	QByteArray pathBytes = m_filename.toUtf8();
	DP_SaveResult result = m_saveFn(
		m_pe->viewCanvasState().get(), m_vmb.get(), pathBytes.constData(), onProgress, this);
	emit saveComplete(CanvasSaverRunnable::saveResultToErrorString(result));
}

void AnimationSaverRunnable::cancelExport()
{
	m_cancelled = true;
}

bool AnimationSaverRunnable::onProgress(void *user, double progress)
{
	AnimationSaverRunnable *saver = static_cast<AnimationSaverRunnable*>(user);
	if(saver->m_cancelled) {
		return false;
	} else {
		emit saver->progress(qRound(progress * 100));
		return true;
	}
}
