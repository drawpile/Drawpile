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
#include "rustpile/rustpile.h"

namespace canvas {

IndexBuilderRunnable::IndexBuilderRunnable(const PaintEngine *pe)
	: QObject(), m_paintengine(pe)
{

}

static void emit_progress(void *runnable, uint32_t progress)
{
	emit static_cast<IndexBuilderRunnable*>(runnable)->progress(progress);
}

void IndexBuilderRunnable::run()
{
	const bool result = rustpile::paintengine_build_index(
		m_paintengine->engine(),
		this,
		&emit_progress
	);

	emit indexingComplete(result);
}

}
