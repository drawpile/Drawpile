/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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
#include "canvassaverrunnable.h"
#include "canvasmodel.h"
#include "ora/orawriter.h"

#include <QImageWriter>

namespace canvas {

CanvasSaverRunnable::CanvasSaverRunnable(const CanvasModel *canvas, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_layerstack(nullptr), // FIXME: canvas->layerStack()->clone(this)),
	  m_filename(filename)
{
}

void CanvasSaverRunnable::run()
{
#if 0 // FIXME
	bool ok;
	QString errorMessage;

	if(m_filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact.
		ok = openraster::saveOpenRaster(m_filename, m_layerstack, &errorMessage);

	} else {
		// Regular image formats: flatten the image first.
		QImageWriter writer(m_filename);
		if(!writer.write(m_layerstack->toFlatImage(false, true, false))) {
			errorMessage = writer.errorString();
			ok = false;

		} else
			ok = true;
	}

	if(!ok && errorMessage.isEmpty())
		errorMessage = "Unknown Error";

	emit saveComplete(errorMessage);
#endif
}

}
