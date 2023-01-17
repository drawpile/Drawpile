/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "animationsaverrunnable.h"
#include "canvassaverrunnable.h"
#include "canvas/paintengine.h"

AnimationSaverRunnable::AnimationSaverRunnable(const canvas::PaintEngine *pe, SaveFn saveFn, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_filename(filename),
	  m_saveFn(saveFn),
	  m_cancelled(false)
{
}

void AnimationSaverRunnable::run()
{
	QByteArray pathBytes = m_filename.toUtf8();
	DP_SaveResult result = m_saveFn(
		m_pe->viewCanvasState().get(), pathBytes.constData(), onProgress, this);
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
