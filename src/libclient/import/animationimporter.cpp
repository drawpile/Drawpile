// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/rust.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/import/animationimporter.h"
#include "libclient/import/loadresult.h"

namespace impex {

AnimationImporter::AnimationImporter(
	const QString &path, int holdTime, int framerate)
	: m_path(path)
	, m_holdTime(holdTime)
	, m_framerate(framerate)
{
}

void AnimationImporter::run()
{
	drawdance::DrawContext drawContext = drawdance::DrawContextPool::acquire();
	DP_LoadResult result;
	drawdance::CanvasState canvasState =
		drawdance::CanvasState::noinc(DP_load_old_animation(
			drawContext.get(), qUtf8Printable(m_path), m_holdTime, m_framerate,
			drawdance::CanvasState::loadFlags(), setGroupTitle, setTrackTitle,
			&result));
	emit finished(
		canvasState,
		canvasState.isNull() ? impex::getLoadResultMessage(result) : QString());
}

void AnimationImporter::setGroupTitle(DP_TransientLayerProps *tlp, int i)
{
	QByteArray title = getTitle(i);
	DP_transient_layer_props_title_set(tlp, title.constData(), title.size());
}

void AnimationImporter::setTrackTitle(DP_TransientTrack *tt, int i)
{
	QByteArray title = getTitle(i);
	DP_transient_track_title_set(tt, title.constData(), title.size());
}

QByteArray AnimationImporter::getTitle(int i)
{
	//: Title for imported animation tracks and layer groups.
	return tr("Frames %1").arg(i + 1).toUtf8();
}

}
