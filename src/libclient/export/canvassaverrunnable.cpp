/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/canvas/paintengine.h"
#include "rustpile/rustpile.h"

CanvasSaverRunnable::CanvasSaverRunnable(const canvas::PaintEngine *pe, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_filename(filename)
{
}

void CanvasSaverRunnable::run()
{
	const auto result = rustpile::paintengine_save_file(
		m_pe->engine(),
		reinterpret_cast<const uint16_t*>(m_filename.constData()),
		m_filename.length()
	);

	switch(result) {
	case rustpile::CanvasIoError::NoError: emit saveComplete(QString()); break;
	case rustpile::CanvasIoError::FileOpenError: emit saveComplete(tr("Couldn't open file for writing")); break;
	default: emit saveComplete(tr("An error occurred while saving image"));
	}
}
