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
#include "libclient/export/animationsaverrunnable.h"
#include "libclient/canvas/paintengine.h"
#include "rustpile/rustpile.h"

AnimationSaverRunnable::AnimationSaverRunnable(const canvas::PaintEngine *pe, rustpile::AnimationExportMode mode, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_filename(filename),
	  m_mode(mode),
	  m_cancelled(false)
{
}

bool animationSaverProgressCallback(void *ctx, float progress) {
	auto *a = static_cast<AnimationSaverRunnable*>(ctx);
	if(a->m_cancelled)
		return false;

	emit a->progress(qRound(progress * 100));
	return true;
}

void AnimationSaverRunnable::run()
{
	const auto result = rustpile::paintengine_save_animation(
		m_pe->engine(),
		reinterpret_cast<const uint16_t*>(m_filename.constData()),
		m_filename.length(),
		m_mode,
		this,
		&animationSaverProgressCallback
	);

	QString msg;
	switch(result) {
	case rustpile::CanvasIoError::NoError: break;
	case rustpile::CanvasIoError::FileOpenError: msg = tr("Couldn't open file for writing"); break;
	default: msg = tr("An error occurred while saving image");
	}

	emit saveComplete(msg, QString());
}

void AnimationSaverRunnable::cancelExport()
{
	m_cancelled = true;
}
