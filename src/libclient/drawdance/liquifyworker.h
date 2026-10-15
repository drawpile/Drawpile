// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_LIQUIFYWORKER_H
#define LIBCLIENT_DRAWDANCE_LIQUIFYWORKER_H
#include "libclient/drawdance/liquify.h"
#include <QAtomicInt>
#include <QObject>

struct DP_LiquifyEngineStrokeParams;
struct DP_LiquifyWorker;
struct DP_LiquifyWorkerEvent;

namespace canvas {
class Point;
}

namespace drawdance {

class LiquifyWorker final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(LiquifyWorker)
public:
	explicit LiquifyWorker(QObject *parent = nullptr);
	~LiquifyWorker() override;

	void init(const drawdance::Liquify &liquify, int undoDepth);
	void clear();

	DP_LiquifyWorker *get() const { return m_data; }

	bool isNull() const { return m_data == nullptr; }

	Liquify liquify();

	LiquifyState takeUpdate();

	void startThread();
	void finishThread();

	void setOperationMove(bool reverse);
	void setOperationScale(bool reverse);
	void setOperationRotate(bool reverse);
	void setOperationSmoothe(float kernelRadius);
	void setOperationErase();

	void setStrokeParams(const DP_LiquifyEngineStrokeParams &lesp);

	void beginStroke(float zoom);
	void strokeTo(const canvas::Point &point, bool constrain, bool center);
	void endStroke(long long timeMsec);
	void cancelStroke(long long timeMsec);

	void undo();
	void redo();

	void requestUpdate();

	void abort();

Q_SIGNALS:
	void updateRequested();
	void undoLimitHit();

private:
	static void handleLiquifyWorkerEventCallback(
		void *user, const DP_LiquifyWorkerEvent *event);

	void handleLiquifyWorkerEvent(const DP_LiquifyWorkerEvent &event);

	DP_LiquifyWorker *m_data = nullptr;
	QAtomicInt m_updatePending;
};

}

#endif
