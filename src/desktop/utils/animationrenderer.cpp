// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
}
#include "desktop/utils/animationrenderer.h"
#include "libclient/drawdance/canvasstate.h"
#include "libshared/util/qtcompat.h"
#include <QImage>
#include <QPixmap>
#include <QRect>

namespace utils {

namespace {

struct FrameRange {
	int frameCount;
	int rangeStart;
	int rangeEndExclusive;
	int startIndex;
};

struct FrameRenderJob {
	AnimationRenderer *ar;
	DP_CanvasState *cs;
	unsigned int batchId;
	int frameIndexCount;
	int *frameIndexes;
	int cropX, cropY, cropWidth, cropHeight;
	int maxWidth, maxHeight;
};

}

AnimationRenderer::AnimationRenderer(QObject *parent)
	: QObject(parent)
	, m_worker(DP_worker_new(
		  64, sizeof(FrameRenderJob), DP_thread_cpu_count(128),
		  handleWorkerJob))
	, m_vmbs(compat::cast_6<compat::sizetype>(DP_worker_thread_count(m_worker)))
	, m_batchId(0)
{
	for(DP_ViewModeBuffer &vmb : m_vmbs) {
		DP_view_mode_buffer_init(&vmb);
	}
}

AnimationRenderer::~AnimationRenderer()
{
	++m_batchId;
	DP_worker_free_join(m_worker);
	for(DP_ViewModeBuffer &vmb : m_vmbs) {
		DP_view_mode_buffer_dispose(&vmb);
	}
}

unsigned int AnimationRenderer::render(
	const drawdance::CanvasState &canvasState, const QRect &crop,
	const QSize &maxSize, int rangeStart, int rangeEndExclusive,
	int currentRangeIndex)
{
	unsigned int batchId = ++m_batchId;
	QVector<int> indexes = buildFrameOrder(
		canvasState.frameCount(), rangeStart, rangeEndExclusive,
		currentRangeIndex);
	QVector<int> frameIndexBuffer;
	while(!indexes.isEmpty()) {
		gatherFrame(canvasState, indexes, frameIndexBuffer);
		int frameIndexCount = compat::cast_6<int>(frameIndexBuffer.size());
		int *frameIndexes = new int[frameIndexCount];
		for(int i = 0; i < frameIndexCount; ++i) {
			frameIndexes[i] = frameIndexBuffer[i];
		}
		FrameRenderJob job = {
			this,
			canvasState.getInc(),
			batchId,
			frameIndexCount,
			frameIndexes,
			crop.x(),
			crop.y(),
			crop.width(),
			crop.height(),
			maxSize.width(),
			maxSize.height(),
		};
		DP_worker_push(m_worker, &job);
	}
	return batchId;
}

void AnimationRenderer::detachDelete()
{
	setParent(nullptr);
	unsigned int batchId = ++m_batchId;
	FrameRenderJob job = {this, nullptr, batchId, 0, nullptr, 0, 0, 0, 0, 0, 0};
	DP_worker_push(m_worker, &job);
}

QVector<int> AnimationRenderer::buildFrameOrder(
	int frameCount, int rangeStart, int rangeEndExclusive,
	int currentRangeIndex)
{
	// We build the frames in priority order. Stuff that's in the user's
	// selected frame range is more important than what's outside of it, frames
	// that are viewed sooner are more important than ones that come later.
	QVector<int> indexes;
	indexes.reserve(frameCount);
	// From the currently visible frame to the end of the frame range.
	int endExclusive = qMin(rangeEndExclusive, frameCount);
	int start = qMin(rangeStart, rangeEndExclusive);
	int current = qMax(currentRangeIndex, start);
	for(int i = current; i < endExclusive; ++i) {
		Q_ASSERT(!indexes.contains(i));
		indexes.append(i);
	}
	// From the beginning of the frame range to the currently visible frame.
	for(int i = start; i < current; ++i) {
		Q_ASSERT(!indexes.contains(i));
		indexes.append(i);
	}
	// Frames after the range.
	for(int i = endExclusive; i < frameCount; ++i) {
		Q_ASSERT(!indexes.contains(i));
		indexes.append(i);
	}
	// Frames before the range.
	for(int i = 0; i < start; ++i) {
		Q_ASSERT(!indexes.contains(i));
		indexes.append(i);
	}
	return indexes;
}

void AnimationRenderer::gatherFrame(
	const drawdance::CanvasState &canvasState, QVector<int> &indexes,
	QVector<int> &frameIndexBuffer)
{
	frameIndexBuffer.clear();
	int firstIndex = indexes.takeFirst();
	frameIndexBuffer.append(firstIndex);
	compat::sizetype remaining = indexes.size();
	compat::sizetype i = 0;
	while(i < remaining) {
		if(canvasState.sameFrame(firstIndex, indexes[i])) {
			frameIndexBuffer.append(indexes.takeAt(i));
			--remaining;
		} else {
			++i;
		}
	}
}

void AnimationRenderer::handleWorkerJob(void *element, int threadIndex)
{
	FrameRenderJob *job = static_cast<FrameRenderJob *>(element);
	AnimationRenderer *ar = job->ar;
	int *frameIndexes = job->frameIndexes;
	if(frameIndexes) {
		drawdance::CanvasState canvasState =
			drawdance::CanvasState::noinc(job->cs);
		unsigned int batchId = job->batchId;
		ar->renderFrame(
			batchId, canvasState, threadIndex, job->frameIndexCount,
			frameIndexes,
			QRect(job->cropX, job->cropY, job->cropWidth, job->cropHeight),
			QSize(job->maxWidth, job->maxHeight));
		delete[] job->frameIndexes;
	} else {
		ar->deleteLater();
	}
}

void AnimationRenderer::renderFrame(
	unsigned int batchId, const drawdance::CanvasState &canvasState,
	int threadIndex, int frameIndexCount, int *frameIndexes, const QRect &crop,
	const QSize &maxSize)
{
	Q_ASSERT(frameIndexCount > 0);
	Q_ASSERT(frameIndexes);
	if(isCancelled(batchId)) {
		return;
	}

	// We've been tasked to render nothing.
	if(canvasState.isNull() || crop.isEmpty()) {
		emit frameRendered(
			batchId, QVector<int>(frameIndexes, frameIndexes + frameIndexCount),
			QPixmap());
		return;
	}

	DP_ViewModeFilter vmf = DP_view_mode_filter_make_frame_render(
		&m_vmbs[threadIndex], canvasState.get(), frameIndexes[0]);
	QImage img = canvasState.toFlatImage(true, true, &crop, &vmf);

	if(isCancelled(batchId)) {
		return;
	}
	if(img.width() > maxSize.width() || img.height() > maxSize.height()) {
		img = img.scaled(
			img.size().boundedTo(maxSize), Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}

	if(isCancelled(batchId)) {
		return;
	}
	emit frameRendered(
		batchId, QVector<int>(frameIndexes, frameIndexes + frameIndexCount),
		QPixmap::fromImage(img));
}

}
