// SPDX-License-Identifier: GPL-3.0-or-later

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
