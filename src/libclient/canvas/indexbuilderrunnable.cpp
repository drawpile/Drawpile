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

#include "libclient/canvas/indexbuilderrunnable.h"
#include "libclient/canvas/paintengine.h"

namespace canvas {

IndexBuilderRunnable::IndexBuilderRunnable(PaintEngine *pe)
	: QObject{}
	, m_paintengine{pe}
{
}

void IndexBuilderRunnable::run()
{
	bool success = m_paintengine->buildPlaybackIndex([this](int percent) {
		emit progress(percent);
	});
	emit indexingComplete(success, success ? QString{} : QString::fromUtf8(DP_error()));
}

}
