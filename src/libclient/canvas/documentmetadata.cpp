/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

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

#include "libclient/canvas/documentmetadata.h"
#include "rustpile/rustpile.h"
#include "libclient/canvas/paintengine.h"

namespace canvas {

DocumentMetadata::DocumentMetadata(PaintEngine *engine, QObject *parent)
    : QObject{parent}, m_engine(engine), m_framerate(15), m_useTimeline(false)
{
	Q_ASSERT(engine);
	refreshMetadata();
	connect(engine, &PaintEngine::metadataChanged, this, &DocumentMetadata::refreshMetadata);
}

void DocumentMetadata::refreshMetadata()
{
	const auto e = m_engine->engine();

	// Note: dpix and dpiy are presently not used in the GUI.
	// To be included here when needed.

	const int framerate = rustpile::paintengine_get_metadata_int(e, rustpile::MetadataInt::Framerate);
	if(framerate != m_framerate) {
		m_framerate = framerate;
		emit framerateChanged(framerate);
	}

	const bool useTimeline = rustpile::paintengine_get_metadata_int(e, rustpile::MetadataInt::UseTimeline);
	if(useTimeline != m_useTimeline) {
		m_useTimeline = useTimeline;
		emit useTimelineChanged(useTimeline);
	}
}

}
