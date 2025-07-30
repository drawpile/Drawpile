// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_group.h>
#include <dpmsg/msg_internal.h>
}
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/freehand.h"
#include "libclient/tools/toolcontroller.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>

using std::placeholders::_1;

namespace tools {

Freehand::Freehand(ToolController &owner)
	: Tool(
		  owner, FREEHAND, Qt::CrossCursor,
		  Capability::AllowColorPick | Capability::SupportsPressure |
			  Capability::AllowToolAdjust1 | Capability::AllowToolAdjust2 |
			  Capability::AllowToolAdjust3)
	, m_strokeWorker(
		  std::bind(&Freehand::pushMessage, this, _1),
		  std::bind(&Freehand::pollControl, this, _1),
		  std::bind(&Freehand::sync, this))
	, m_mutex(DP_mutex_new())
	, m_sem(DP_semaphore_new(0))
{
	QObject::connect(
		&m_owner, &ToolController::freehandMessagesAvailable, &m_owner,
		std::bind(&Freehand::flushMessages, this), Qt::QueuedConnection);
	m_pollTimer.setSingleShot(false);
	m_pollTimer.setTimerType(Qt::PreciseTimer);
	m_pollTimer.setInterval(15);
	QObject::connect(&m_pollTimer, &QTimer::timeout, [this] {
		poll();
	});
}

Freehand::~Freehand()
{
	DP_semaphore_free(m_sem);
	DP_mutex_free(m_mutex);
}

void Freehand::begin(const BeginParams &params)
{
	beginWith(params, nullptr, 0.0);
}

void Freehand::beginWith(
	const BeginParams &params, DP_LayerContent *floodLcOrNull,
	double floodTolerance)
{
	Q_ASSERT(!m_drawing);
	if(params.right) {
		return;
	}

	m_drawing = true;
	m_firstPoint = true;

	m_owner.setStrokeWorkerBrush(
		m_strokeWorker, type(), floodLcOrNull, floodTolerance);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = params.point;
	m_zoom = params.zoom;
	m_angle = params.angle;
	m_mirror = params.mirror;
	m_flip = params.flip;
}

void Freehand::motion(const MotionParams &params)
{
	if(m_drawing) {
		drawdance::CanvasState canvasState =
			m_owner.model()->paintEngine()->sampleCanvasState();

		if(m_firstPoint) {
			m_firstPoint = false;
			m_strokeWorker.beginStroke(
				localUserId(), canvasState, isCompatibilityMode(), true,
				m_mirror, m_flip, m_zoom, m_angle);
			m_start.setPressure(
				qMin(m_start.pressure(), params.point.pressure()));
			m_strokeWorker.strokeTo(m_start, canvasState);
		}

		m_strokeWorker.strokeTo(params.point, canvasState);
		m_strokeWorker.flushDabs();
	}
}

void Freehand::end(const EndParams &)
{
	if(m_drawing) {
		m_drawing = false;
		drawdance::CanvasState canvasState =
			m_owner.model()->paintEngine()->sampleCanvasState();

		if(m_firstPoint) {
			m_firstPoint = false;
			m_strokeWorker.beginStroke(
				localUserId(), canvasState, isCompatibilityMode(), true,
				m_mirror, m_flip, m_zoom, m_angle);
			m_strokeWorker.strokeTo(m_start, canvasState);
		}

		m_strokeWorker.endStroke(
			QDateTime::currentMSecsSinceEpoch(), canvasState, true);
	}
}

bool Freehand::undoRedo(bool redo)
{
	cancelStroke();
	m_strokeWorker.pushMessageNoinc(DP_msg_undo_new(localUserId(), 0, redo));
	return true;
}

void Freehand::offsetActiveTool(int x, int y)
{
	m_strokeWorker.addOffset(x, y);
}

void Freehand::setBrushSizeLimit(int limit)
{
	m_strokeWorker.setSizeLimit(limit);
}

void Freehand::setSelectionMaskingEnabled(bool selectionMaskingEnabled)
{
	setCapability(Capability::IgnoresSelections, !selectionMaskingEnabled);
}

void Freehand::finish()
{
	m_cancelling = true;
	cancelStroke();
	DP_SEMAPHORE_MUST_POST(m_sem);
	m_strokeWorker.finishThread();
	DP_SEMAPHORE_MUST_WAIT(m_sem);
	m_cancelling = false;
}

void Freehand::dispose()
{
	m_cancelling = true;
	cancelStroke();
	DP_SEMAPHORE_MUST_POST(m_sem);
	m_strokeWorker.finishThread();
}

void Freehand::cancelStroke()
{
	m_strokeWorker.cancelStroke(QDateTime::currentMSecsSinceEpoch(), true);
}

void Freehand::pushMessage(DP_Message *msg)
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	bool needsSignal = m_messages.isEmpty();
	m_messages.append(net::Message::noinc(msg));
	DP_MUTEX_MUST_UNLOCK(m_mutex);
	if(needsSignal) {
		emit m_owner.freehandMessagesAvailable();
	}
}

void Freehand::flushMessages()
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	int count = m_messages.size();
	if(count != 0) {
		net::Client *client = m_owner.client();
		const net::Message *msgs = m_messages.constData();
		// The message list is a mixture of drawing commands and internal sync
		// messages, the latter of which must only be sent locally.
		int start = 0;
		do {
			int chunk = 1;
			bool firstInternal = msgs[start].type() == DP_MSG_INTERNAL;
			for(int i = start + 1; i < count; ++i) {
				bool internal = msgs[i].type() == DP_MSG_INTERNAL;
				if(internal == firstInternal) {
					++chunk;
				} else {
					break;
				}
			}

			if(firstInternal) {
				client->sendLocalMessages(chunk, msgs + start);
			} else {
				client->sendCommands(chunk, msgs + start);
			}

			start += chunk;
		} while(start < count);
		m_messages.clear();
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

void Freehand::pollControl(bool enable)
{
	if(isOnMainThread()) {
		if(enable) {
			m_pollTimer.start();
		} else {
			m_pollTimer.stop();
		}
	} else {
		QMetaObject::invokeMethod(
			&m_pollTimer, enable ? "start" : "stop",
			m_cancelling ? Qt::QueuedConnection : Qt::BlockingQueuedConnection);
	}
}

void Freehand::poll()
{
	drawdance::CanvasState canvasState =
		m_owner.model()->paintEngine()->sampleCanvasState();
	m_strokeWorker.poll(QDateTime::currentMSecsSinceEpoch(), canvasState);
	m_strokeWorker.flushDabs();
}

DP_CanvasState *Freehand::sync()
{
	if(isOnMainThread()) {
		qWarning("Freehand::sync called on main thread");
		return nullptr;
	} else if(m_cancelling) {
		return nullptr;
	} else {
		pushMessage(DP_msg_internal_paint_sync_new(
			0, &Freehand::syncUnlockCallback, this));
		DP_SEMAPHORE_MUST_WAIT(m_sem);
		return m_owner.model()->paintEngine()->sampleCanvasState().take();
	}
}

void Freehand::syncUnlock()
{
	DP_SEMAPHORE_MUST_POST(m_sem);
}

void Freehand::syncUnlockCallback(void *user)
{
	Freehand *freehand = static_cast<Freehand *>(user);
	freehand->syncUnlock();
}

bool Freehand::isOnMainThread()
{
	return QCoreApplication::instance()->thread() == QThread::currentThread();
}


FreehandEraser::FreehandEraser(ToolController &owner, Freehand *freehand)
	: Tool(
		  owner, ERASER, Qt::CrossCursor,
		  Capability::AllowColorPick | Capability::SupportsPressure |
			  Capability::AllowToolAdjust1 | Capability::AllowToolAdjust2 |
			  Capability::AllowToolAdjust3)
	, m_freehand(freehand)
{
}

void FreehandEraser::begin(const BeginParams &params)
{
	m_freehand->begin(params);
}

void FreehandEraser::motion(const MotionParams &params)
{
	m_freehand->motion(params);
}

void FreehandEraser::end(const EndParams &params)
{
	m_freehand->end(params);
}

bool FreehandEraser::undoRedo(bool redo)
{
	return m_freehand->undoRedo(redo);
}

void FreehandEraser::offsetActiveTool(int x, int y)
{
	m_freehand->offsetActiveTool(x, y);
}

void FreehandEraser::finish()
{
	m_freehand->finish();
}


FreehandFill::FreehandFill(ToolController &owner, Freehand *freehand)
	: Tool(
		  owner, BRUSHFILL, Qt::CrossCursor,
		  Capability::AllowColorPick | Capability::SupportsPressure |
			  Capability::AllowToolAdjust1 | Capability::AllowToolAdjust2 |
			  Capability::AllowToolAdjust3)
	, m_freehand(freehand)
{
}

void FreehandFill::begin(const BeginParams &params)
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(!canvas) {
		return;
	}

	int layerId = canvas->layerlist()->fillSourceLayerId();
	if(layerId <= 0) {
		return;
	}

	drawdance::LayerSearchResult lsr =
		canvas->paintEngine()->viewCanvasState().searchLayer(layerId, true);
	if(drawdance::LayerContent *layerContent =
		   std::get_if<drawdance::LayerContent>(&lsr.data)) {
		setLayerContentSource(layerContent->get());
	} else if(
		drawdance::LayerGroup *layerGroup =
			std::get_if<drawdance::LayerGroup>(&lsr.data)) {
		setLayerGroupSource(layerGroup->get(), lsr.props.get());
	} else {
		qWarning("Brush fill source %d not found", layerId);
		return;
	}

	Q_ASSERT(m_sourceLc);
	m_freehand->beginWith(params, m_sourceLc, m_tolerance);
}

void FreehandFill::motion(const MotionParams &params)
{
	m_freehand->motion(params);
}

void FreehandFill::end(const EndParams &params)
{
	m_freehand->end(params);
}

bool FreehandFill::undoRedo(bool redo)
{
	return m_freehand->undoRedo(redo);
}

void FreehandFill::offsetActiveTool(int x, int y)
{
	m_freehand->offsetActiveTool(x, y);
}

void FreehandFill::finish()
{
	m_freehand->finish();
	clearSource();
}

void FreehandFill::dispose()
{
	clearSource();
}

void FreehandFill::setLayerContentSource(DP_LayerContent *lc)
{
	Q_ASSERT(lc);
	if(m_sourceLg || m_sourceLc != lc) {
		clearSource();
		qDebug("Set layer content source");
		m_sourceLc = DP_layer_content_incref(lc);
	}
}

void FreehandFill::setLayerGroupSource(DP_LayerGroup *lg, DP_LayerProps *lp)
{
	Q_ASSERT(lg);
	Q_ASSERT(lp);
	if(m_sourceLg != lg) {
		clearSource();
		qDebug("Set layer group source");
		m_sourceLg = DP_layer_group_incref(lg);
		m_sourceLc = DP_transient_layer_content_persist(
			DP_layer_group_merge(lg, lp, false));
	}
}

void FreehandFill::clearSource()
{
	qDebug("Clear source");
	DP_layer_group_decref_nullable(m_sourceLg);
	DP_layer_content_decref_nullable(m_sourceLc);
	m_sourceLg = nullptr;
	m_sourceLc = nullptr;
}

}
