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
#include "libclient/drawdance/global.h"

extern "C" {
#include <dpengine/save.h>
}

CanvasSaverRunnable::CanvasSaverRunnable(const canvas::PaintEngine *pe, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_filename(filename)
{
}

void CanvasSaverRunnable::run()
{
	QByteArray pathBytes = m_filename.toUtf8();
	qDebug("Saving to '%s'", pathBytes.constData());
	drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
	DP_SaveResult result = DP_save(
		m_pe->viewCanvasState().get(), dc.get(), DP_SAVE_IMAGE_GUESS, pathBytes.constData());
	emit saveComplete(saveResultToErrorString(result));
}


QString CanvasSaverRunnable::saveResultToErrorString(DP_SaveResult result)
{
	switch(result) {
    case DP_SAVE_RESULT_SUCCESS:
	case DP_SAVE_RESULT_CANCEL:
		return QString{};
    case DP_SAVE_RESULT_BAD_ARGUMENTS:
		return tr("Bad arguments, this is probably a bug in Drawpile.");
    case DP_SAVE_RESULT_NO_EXTENSION:
		return tr("No file extension given.");
    case DP_SAVE_RESULT_UNKNOWN_FORMAT:
		return tr("Unsupported format.");
	case DP_SAVE_RESULT_FLATTEN_ERROR:
		return tr("Couldn't merge the canvas into a flat image.");
    case DP_SAVE_RESULT_OPEN_ERROR:
		return tr("Couldn't open file for writing.");
    case DP_SAVE_RESULT_WRITE_ERROR:
		return tr("Save operation failed, but the file might have been partially written.");
	case DP_SAVE_RESULT_INTERNAL_ERROR:
		return tr("Internal error during saving.");
	default:
		return tr("Unknown error.");
	}
}
