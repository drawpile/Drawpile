// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_ANIMATIONRENDERER_H
#define DESKTOP_UTILS_ANIMATIONRENDERER_H
extern "C" {
#include <dpengine/view_mode.h>
}
#include <QAtomicInteger>
#include <QObject>
#include <QVector>

class QRect;
struct DP_ViewModeBuffer;
struct DP_Worker;

namespace drawdance {
class CanvasState;
}

namespace utils {

class AnimationRenderer final : public QObject {
	Q_OBJECT
public:
	explicit AnimationRenderer(QObject *parent = nullptr);
	~AnimationRenderer() override;

	AnimationRenderer(const AnimationRenderer &) = delete;
	AnimationRenderer(AnimationRenderer &&) = delete;
	AnimationRenderer &operator=(const AnimationRenderer &) = delete;
	AnimationRenderer &operator=(AnimationRenderer &&) = delete;

	unsigned int render(
		const drawdance::CanvasState &canvasState, const QRect &crop,
		const QSize &maxSize, int rangeStart, int rangeEndExclusive,
		int currentRangeIndex);

	// Asynchronous destruction without waiting for running jobs. Orphans this,
	// cancels current batch and enqueues a job that calls deleteLater.
	void detachDelete();

signals:
	void frameRendered(
		unsigned int batchId, const QVector<int> &frameIndexes,
		const QPixmap &frame);

private:
	DP_Worker *m_worker;
	QVector<DP_ViewModeBuffer> m_vmbs;
	QAtomicInteger<unsigned int> m_batchId;

	static QVector<int> buildFrameOrder(
		int frameCount, int rangeStart, int rangeEndExclusive,
		int currentRangeIndex);

	static void gatherFrame(
		const drawdance::CanvasState &canvasState, QVector<int> &indexes,
		QVector<int> &frameIndexBuffer);

	bool isCancelled(unsigned int batchId) const
	{
		return batchId != m_batchId.loadAcquire();
	}

	static void handleWorkerJob(void *element, int threadIndex);

	void renderFrame(
		unsigned int batchId, const drawdance::CanvasState &canvasState,
		int threadIndex, int frameIndexCount, int *frameIndexes,
		const QRect &crop, const QSize &maxSize);
};

}

#endif
