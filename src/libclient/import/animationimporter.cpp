// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/load_animation.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/import/animationimporter.h"
#include "libclient/import/loadresult.h"

namespace impex {

AnimationImporter::AnimationImporter(int holdTime, int framerate)
	: m_holdTime(holdTime)
	, m_framerate(framerate)
{
}

void AnimationImporter::run()
{
	DP_LoadResult result;
	drawdance::CanvasState canvasState =
		drawdance::CanvasState::noinc(load(&result));
	emit finished(
		canvasState,
		canvasState.isNull() ? impex::getLoadResultMessage(result) : QString());
}

void AnimationImporter::setLayerTitle(DP_TransientLayerProps *tlp, int i)
{
	//: Title for imported animation layers.
	QByteArray title = tr("Frame %1").arg(i + 1).toUtf8();
	DP_transient_layer_props_title_set(tlp, title.constData(), title.size());
}

void AnimationImporter::setGroupTitle(DP_TransientLayerProps *tlp, int i)
{
	QByteArray title = getGroupOrTrackTitle(i);
	DP_transient_layer_props_title_set(tlp, title.constData(), title.size());
}

void AnimationImporter::setTrackTitle(DP_TransientTrack *tt, int i)
{
	QByteArray title = getGroupOrTrackTitle(i);
	DP_transient_track_title_set(tt, title.constData(), title.size());
}

QByteArray AnimationImporter::getGroupOrTrackTitle(int i)
{
	//: Title for imported animation tracks and layer groups.
	return tr("Frames %1").arg(i + 1).toUtf8();
}


AnimationLayersImporter::AnimationLayersImporter(
	const QString &path, int holdTime, int framerate)
	: AnimationImporter(holdTime, framerate)
	, m_path(path)
{
}

DP_CanvasState *AnimationLayersImporter::load(DP_LoadResult *outResult)
{
	drawdance::DrawContext drawContext = drawdance::DrawContextPool::acquire();
	return DP_load_animation_layers(
		drawContext.get(), qUtf8Printable(m_path), m_holdTime, m_framerate,
		drawdance::CanvasState::loadFlags(), setGroupTitle, setTrackTitle,
		outResult);
}


AnimationFramesImporter::AnimationFramesImporter(
	const QStringList &paths, int holdTime, int framerate)
	: AnimationImporter(holdTime, framerate)
	, m_pathsBytes(pathsToUtf8(paths))
{
}

DP_CanvasState *AnimationFramesImporter::load(DP_LoadResult *outResult)
{
	drawdance::DrawContext drawContext = drawdance::DrawContextPool::acquire();
	return DP_load_animation_frames(
		drawContext.get(), m_pathsBytes.size(), getPathAt, this, m_holdTime,
		m_framerate, setLayerTitle, setGroupTitle, setTrackTitle, outResult);
}

QVector<QByteArray>
AnimationFramesImporter::pathsToUtf8(const QStringList &paths)
{
	int count = paths.size();
	QVector<QByteArray> pathsBytes;
	pathsBytes.reserve(count);
	for(int i = 0; i < count; ++i) {
		pathsBytes.append(paths[i].toUtf8());
	}
	return pathsBytes;
}

const char *AnimationFramesImporter::getPathAt(void *user, int index)
{
	return static_cast<AnimationFramesImporter *>(user)
		->m_pathsBytes[index]
		.constData();
}


}
