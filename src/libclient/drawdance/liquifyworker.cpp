// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/liquify.h>
#include <dpengine/liquify_worker.h>
}
#include "libclient/drawdance/brushengine.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/liquifyworker.h"
#include <QDateTime>

namespace drawdance {

LiquifyWorker::LiquifyWorker(QObject *parent)
	: QObject(parent)
{
}

LiquifyWorker::~LiquifyWorker()
{
	clear();
}

void LiquifyWorker::init(const drawdance::Liquify &liquify, int undoDepth)
{
	DP_DrawContext *dc;
	if(m_data) {
		dc = DP_liquify_worker_draw_context(m_data);
		DP_liquify_worker_free(m_data);
	} else {
		dc = DrawContextPool::acquireRaw();
	}

	m_data = DP_liquify_worker_new_inc(
		liquify.get(), dc, undoDepth,
		&LiquifyWorker::handleLiquifyWorkerEventCallback, this);
	m_updatePending.storeRelaxed(0);
}

void LiquifyWorker::clear()
{
	if(m_data) {
		DP_DrawContext *dc = DP_liquify_worker_draw_context(m_data);
		DP_liquify_worker_free(m_data);
		m_data = nullptr;
		DrawContextPool::releaseRaw(dc);
		m_updatePending.storeRelaxed(0);
	}
}

Liquify LiquifyWorker::liquify()
{
	return Liquify::inc(DP_liquify_worker_liquify_noinc(m_data));
}

LiquifyState LiquifyWorker::takeUpdate()
{
	DP_Liquify *l = DP_liquify_worker_liquify_noinc(m_data);
	m_updatePending.storeRelaxed(0);
	return drawdance::LiquifyState::noinc(DP_liquify_current_state_inc(l));
}

void LiquifyWorker::startThread()
{
	if(!DP_liquify_worker_thread_start(m_data)) {
		qWarning("Failed to start liquify worker thread: %s", DP_error());
	}
}

void LiquifyWorker::finishThread()
{
	DP_liquify_worker_thread_finish(m_data);
}

void LiquifyWorker::setOperationMove(bool reverse)
{
	DP_LiquifyWorkerOperationParams lwop;
	lwop.type = DP_LIQUIFY_OP_TYPE_MOVE;
	lwop.arg.reverse = reverse;
	DP_liquify_worker_operation_params_set(m_data, &lwop);
}

void LiquifyWorker::setOperationScale(bool reverse)
{
	DP_LiquifyWorkerOperationParams lwop;
	lwop.type = DP_LIQUIFY_OP_TYPE_SCALE;
	lwop.arg.reverse = reverse;
	DP_liquify_worker_operation_params_set(m_data, &lwop);
}

void LiquifyWorker::setOperationRotate(bool reverse)
{
	DP_LiquifyWorkerOperationParams lwop;
	lwop.type = DP_LIQUIFY_OP_TYPE_ROTATE;
	lwop.arg.reverse = reverse;
	DP_liquify_worker_operation_params_set(m_data, &lwop);
}

void LiquifyWorker::setOperationSmoothe(float kernelRadius)
{
	DP_LiquifyWorkerOperationParams lwop;
	lwop.type = DP_LIQUIFY_OP_TYPE_SMOOTHE;
	lwop.arg.smoothe_kernel_radius = kernelRadius;
	DP_liquify_worker_operation_params_set(m_data, &lwop);
}

void LiquifyWorker::setOperationErase()
{
	DP_LiquifyWorkerOperationParams lwop;
	lwop.type = DP_LIQUIFY_OP_TYPE_ERASE;
	lwop.arg.reverse = false;
	DP_liquify_worker_operation_params_set(m_data, &lwop);
}

void LiquifyWorker::setStrokeParams(const DP_LiquifyEngineStrokeParams &lesp)
{
	DP_liquify_worker_stroke_params_set(m_data, &lesp);
}

void LiquifyWorker::beginStroke(float zoom)
{
	DP_liquify_worker_stroke_begin(m_data, zoom);
}

void LiquifyWorker::strokeTo(
	const canvas::Point &point, bool constrain, bool center)
{
	DP_BrushPoint bp = canvasPointToBrushPoint(point);
	DP_liquify_worker_stroke_to(
		m_data, &bp,
		DP_flag_uint(constrain, DP_LIQUIFY_WORKER_FLAG_CONSTRAIN) |
			DP_flag_uint(center, DP_LIQUIFY_WORKER_FLAG_CENTER));
}

void LiquifyWorker::endStroke(long long timeMsec)
{
	DP_liquify_worker_stroke_end(m_data, timeMsec);
}

void LiquifyWorker::cancelStroke(long long timeMsec)
{
	DP_liquify_worker_stroke_cancel(m_data, timeMsec);
}

void LiquifyWorker::undo()
{
	DP_liquify_worker_undo(m_data);
}

void LiquifyWorker::redo()
{
	DP_liquify_worker_redo(m_data);
}

void LiquifyWorker::requestUpdate()
{
	DP_liquify_worker_update_request(m_data);
}

void LiquifyWorker::abort()
{
	DP_liquify_worker_abort(m_data);
}

void LiquifyWorker::handleLiquifyWorkerEventCallback(
	void *user, const DP_LiquifyWorkerEvent *event)
{
	static_cast<LiquifyWorker *>(user)->handleLiquifyWorkerEvent(*event);
}

void LiquifyWorker::handleLiquifyWorkerEvent(const DP_LiquifyWorkerEvent &event)
{
	switch(event.type) {
	case DP_LIQUIFY_WORKER_EVENT_UPDATE:
		if(m_updatePending.testAndSetRelaxed(0, 1)) {
			Q_EMIT updateRequested();
		}
		return;
	case DP_LIQUIFY_WORKER_EVENT_UNDO_LIMIT_HIT:
		Q_EMIT undoLimitHit();
		return;
	}
	qWarning("Unhandled liquify worker event type %d", int(event.type));
}

}
