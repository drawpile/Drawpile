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

AntiOverflowSource::~AntiOverflowSource()
{
	clear();
}

DP_LayerContent *AntiOverflowSource::get(ToolController &owner)
{
	canvas::CanvasModel *canvas = owner.model();
	if(!canvas) {
		clear();
		return nullptr;
	}

	canvas::LayerListModel *layerlist = canvas->layerlist();
	int fillSourceLayerId = layerlist->fillSourceLayerId();
	if(fillSourceLayerId <= 0) {
		clear();
		return nullptr;
	}

	drawdance::LayerSearchResult lsr =
		canvas->paintEngine()->viewCanvasState().searchLayer(
			fillSourceLayerId, true);
	if(drawdance::LayerContent *layerContent =
		   std::get_if<drawdance::LayerContent>(&lsr.data)) {
		return setLayerContentSource(layerContent->get());
	} else if(
		drawdance::LayerGroup *layerGroup =
			std::get_if<drawdance::LayerGroup>(&lsr.data)) {
		return setLayerGroupSource(layerGroup->get(), lsr.props.get());
	} else {
		qWarning("Anti-overflow source %d not found", fillSourceLayerId);
		clear();
		return nullptr;
	}
}

void AntiOverflowSource::clear()
{
	DP_layer_group_decref_nullable(m_sourceLg);
	DP_layer_content_decref_nullable(m_sourceLc);
	m_sourceLg = nullptr;
	m_sourceLc = nullptr;
}

DP_LayerContent *AntiOverflowSource::setLayerContentSource(DP_LayerContent *lc)
{
	Q_ASSERT(lc);
	if(m_sourceLg || m_sourceLc != lc) {
		clear();
		m_sourceLc = DP_layer_content_incref(lc);
	}
	return m_sourceLc;
}

DP_LayerContent *
AntiOverflowSource::setLayerGroupSource(DP_LayerGroup *lg, DP_LayerProps *lp)
{
	Q_ASSERT(lg);
	Q_ASSERT(lp);
	if(m_sourceLg != lg) {
		clear();
		m_sourceLg = DP_layer_group_incref(lg);
		m_sourceLc = DP_transient_layer_content_persist(
			DP_layer_group_merge(lg, lp, false));
	}
	return m_sourceLc;
}


Freehand::Freehand(ToolController &owner, DP_MaskSync *ms)
	: Tool(
		  owner, FREEHAND, Qt::CrossCursor,
		  Capability::AllowColorPick | Capability::SupportsPressure |
			  Capability::AllowToolAdjust1 | Capability::AllowToolAdjust2 |
			  Capability::AllowToolAdjust3)
	, m_strokeWorker(
		  ms, std::bind(&Freehand::pushMessage, this, _1),
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
	beginStroke(params, this);
}

void Freehand::beginStroke(const BeginParams &params, SnapToPixelToggle *target)
{
	Q_ASSERT(!m_drawing);
	if(params.right) {
		return;
	}

	const brushes::ActiveBrush &brush = m_owner.activeBrush();
	bool pixelArtInput = brush.isPixelArtInput();
	target->setSnapToPixel(pixelArtInput);

	const DP_AntiOverflow &antiOverflow = brush.constAntiOverflow();
	DP_LayerContent *floodLc;
	double floodTolerance;
	int floodExpand;
	if(antiOverflow.enabled && !isCompatibilityMode()) {
		floodLc = m_antiOverflowSource.get(m_owner);
		if(!floodLc) {
			emit m_owner.showMessageRequested(
				QCoreApplication::translate(
					"tools::FreehandSettings",
					"Anti-overflow requires a fill source layer."));
			return;
		}
		floodTolerance = double(antiOverflow.tolerance) / 255.0;
		floodExpand = antiOverflow.expand;
	} else {
		floodLc = nullptr;
		floodTolerance = 0.0;
		floodExpand = 0;
	}

	m_drawing = true;
	m_firstPoint = true;

	m_owner.setStrokeWorkerBrush(
		m_strokeWorker, type(), floodLc, floodTolerance, floodExpand);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = params.point;
	m_zoom = params.zoom;
	m_angle = params.angle;
	m_mirror = params.mirror;
	m_flip = params.flip;

	if(m_owner.activeBrush().isPixelArtInput()) {
		strokeTo(params.point);
	}
}

void Freehand::motion(const MotionParams &params)
{
	if(m_drawing) {
		strokeTo(params.point);
	}
}

void Freehand::strokeTo(const canvas::Point &point)
{
	Q_ASSERT(m_drawing);
	drawdance::CanvasState canvasState =
		m_owner.model()->paintEngine()->sampleCanvasState();

	if(m_firstPoint) {
		m_firstPoint = false;
		m_strokeWorker.beginStroke(
			localUserId(), canvasState, isCompatibilityMode(), true, m_mirror,
			m_flip, m_zoom, m_angle);
		m_start.setPressure(qMin(m_start.pressure(), point.pressure()));
		m_strokeWorker.strokeTo(m_start, canvasState);
	}

	m_strokeWorker.strokeTo(point, canvasState);
	m_strokeWorker.flushDabs();
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
	// There may be messages not yet emitted that would POST to m_sem. Those
	// need to be executed early, otherwise the following WAIT may hang.
	executePendingSyncMessages();
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

void Freehand::setSnapToPixel(bool snapToPixel)
{
	setCapability(Capability::SnapsToPixel, snapToPixel);
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

void Freehand::executePendingSyncMessages()
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	int i = 0;
	int count = m_messages.size();
	QVector<QThread *> threads;
	while(i < count) {
		if(isPaintSyncInternalMessage(m_messages[i])) {
			m_messages.removeAt(i);
			DP_SEMAPHORE_MUST_POST(m_sem);
			--count;
		} else {
			++i;
		}
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

bool Freehand::isPaintSyncInternalMessage(const net::Message &msg)
{
	return msg.type() == DP_MSG_INTERNAL &&
		   DP_msg_internal_type(msg.toInternal()) ==
			   DP_MSG_INTERNAL_TYPE_PAINT_SYNC;
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
	m_freehand->beginStroke(params, this);
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

void FreehandEraser::setSnapToPixel(bool snapToPixel)
{
	setCapability(Capability::SnapsToPixel, snapToPixel);
}

}
