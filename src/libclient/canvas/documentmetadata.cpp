// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/canvasstate.h"

namespace canvas {

DocumentMetadata::DocumentMetadata(PaintEngine *engine, QObject *parent)
    : QObject{parent}, m_engine(engine), m_framerate(0), m_frameCount(0)
{
	Q_ASSERT(engine);
	refreshMetadata(m_engine->historyCanvasState().documentMetadata());
	connect(engine, &PaintEngine::documentMetadataChanged, this, &DocumentMetadata::refreshMetadata);
}

void DocumentMetadata::refreshMetadata(const drawdance::DocumentMetadata &dm)
{
	// Note: dpix and dpiy are presently not used in the GUI.
	// To be included here when needed.

	const int framerate = dm.framerate();
	if(framerate != m_framerate) {
		m_framerate = framerate;
		emit framerateChanged(framerate);
	}

	const int frameCount = dm.frameCount();
	if(frameCount != m_frameCount) {
		m_frameCount = frameCount;
		emit frameCountChanged(frameCount);
	}
}

}
