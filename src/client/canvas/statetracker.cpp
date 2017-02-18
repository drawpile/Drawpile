/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "statetracker.h"
#include "layerlist.h"
#include "loader.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "net/commands.h"
#include "net/internalmsg.h"
#include "tools/selection.h" // for selection transform utils

#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QPainter>

namespace canvas {

struct StateSavepoint::Data {
	Data() : timestamp(0), streampointer(-1), canvas(0), _refcount(1) {}
	Data(const Data &) = delete;
	Data &operator=(const Data&) = delete;
	~Data() { delete canvas; }

	qint64 timestamp;
	int streampointer;
	paintcore::Savepoint *canvas;
	QHash<int, DrawingContext> ctxstate;
	QVector<LayerListItem> layermodel;

private:
	int _refcount;
	friend class StateSavepoint;
};

StateSavepoint::StateSavepoint(const StateSavepoint &sp)
	: m_data(sp.m_data)
{
	if(m_data)
		++m_data->_refcount;
}

StateSavepoint &StateSavepoint::operator =(const StateSavepoint &sp)
{
	if(m_data) {
		if(sp.m_data != m_data) {
			Q_ASSERT(m_data->_refcount>0);
			if(--m_data->_refcount == 0)
				delete m_data;
			m_data = sp.m_data;
			++m_data->_refcount;
		}
	} else {
		m_data = sp.m_data;
		++m_data->_refcount;
	}
	return *this;
}

StateSavepoint::~StateSavepoint()
{
	if(m_data) {
		Q_ASSERT(m_data->_refcount>0);
		if(--m_data->_refcount == 0)
			delete m_data;
	}
}

StateSavepoint::Data *StateSavepoint::operator ->() {
	if(!m_data)
		m_data = new StateSavepoint::Data;
	return m_data;
}

qint64 StateSavepoint::timestamp() const
{
	return m_data ? m_data->timestamp : 0;

}

QImage StateSavepoint::thumbnail(const QSize &maxSize) const
{
	if(!m_data)
		return QImage();

	paintcore::LayerStack stack;
	stack.restoreSavepoint(m_data->canvas);
	QImage img = stack.toFlatImage(true);
	if(img.width() > maxSize.width() || img.height() > maxSize.height()) {
		img = img.scaled(maxSize, Qt::KeepAspectRatio);
	}
	return img;
}

QList<protocol::MessagePtr> StateSavepoint::initCommands(uint8_t contextId) const
{
	if(!m_data)
		return QList<protocol::MessagePtr>();

	paintcore::LayerStack stack;
	stack.restoreSavepoint(m_data->canvas);
	SnapshotLoader loader(contextId, &stack);
	return loader.loadInitCommands();
}

void ToolContext::updateFromToolchange(const protocol::ToolChange &cmd)
{
	layer_id = cmd.layer();
	brush.setBlendingMode(paintcore::BlendMode::Mode(cmd.blend()));
	brush.setSubpixel(cmd.mode() & protocol::TOOL_MODE_SUBPIXEL);
	brush.setIncremental(cmd.mode() & protocol::TOOL_MODE_INCREMENTAL);
	brush.setSpacing(cmd.spacing());
	brush.setSize(qMax(1, (int)cmd.size_h()));
	brush.setSize2(qMax(1, (int)cmd.size_l()));
	brush.setHardness(cmd.hard_h() / 255.0);
	brush.setHardness2(cmd.hard_l() / 255.0);
	brush.setOpacity(cmd.opacity_h() / 255.0);
	brush.setOpacity2(cmd.opacity_l() / 255.0);
	brush.setColor(cmd.color());
	brush.setSmudge(cmd.smudge_h() / 255.0);
	brush.setSmudge2(cmd.smudge_l() / 255.0);
	brush.setResmudge(cmd.resmudge());
}

/**
 * @brief Construct a state tracker instance
 *
 * @param image the canvas content
 * @param layerlist layer list model for the UI
 * @param myId ID of the local user
 * @param parent
 */
StateTracker::StateTracker(paintcore::LayerStack *image, LayerListModel *layerlist, int myId, QObject *parent)
	: QObject(parent),
		_image(image),
		m_layerlist(layerlist),
		m_myId(myId),
		m_fullhistory(true),
		_showallmarkers(false),
		m_hasParticipated(false),
		m_isQueued(false)
{
	connect(m_layerlist, &LayerListModel::layerOpacityPreview, this, &StateTracker::previewLayerOpacity);

	// Timer for periodically resetting the local fork to keep cruft from accumulating.
	// This is to make sure an out-of-sync fork gets cleaned up even if the user doesn't
	// draw anything in a while.
	_localforkCleanupTimer = new QTimer(this);
	_localforkCleanupTimer->setSingleShot(true);
	connect(_localforkCleanupTimer, &QTimer::timeout, this, &StateTracker::resetLocalFork);

	// Timer for processing drawing commands in short chunks to avoid entirely locking up the UI.
	// In the future, canvas rendering should be done in a separate thread.
	m_queuetimer = new QTimer(this);
	m_queuetimer->setSingleShot(true);
	connect(m_queuetimer, &QTimer::timeout, this, &StateTracker::processQueuedCommands);
}

StateTracker::~StateTracker()
{
}

void StateTracker::reset()
{
	m_savepoints.clear();
	m_history.resetTo(m_history.end());
	m_fullhistory = true;
	m_hasParticipated = false;
	_localfork.clear();
	m_layerlist->clear();
}

void StateTracker::localCommand(protocol::MessagePtr msg)
{
	// A fork is created at the end of the mainline history
	if(_localfork.isEmpty()) {
		_localfork.setOffset(m_history.end()-1);

		// Since the presence of a local fork blocks savepoint creation,
		// now is a good time to try to create one.
		if(msg->type() == protocol::MSG_UNDOPOINT)
			makeSavepoint(m_history.end()-1);
	}

	_localfork.addLocalMessage(msg, affectedArea(msg));

	// for the future: handle undo messages in the local fork too
	if(msg->type() != protocol::MSG_UNDO && msg->type() != protocol::MSG_UNDOPOINT) {
		int pos = m_history.end() - 1;
		handleCommand(msg, false, pos);
	}

	_localforkCleanupTimer->start(60 * 1000);
}

void StateTracker::receiveQueuedCommand(protocol::MessagePtr msg)
{
	m_msgqueue.append(msg);

	if(!m_isQueued) {
		// This introduces a tiny bit of lag, but allows sequential
		// messages to queue up even when the system is not under very heavy
		// load. Won't be needed anymore when the paint engine runs in its own
		// thread.
		m_isQueued = true;
		m_queuetimer->start(1);
	}
}

void StateTracker::processQueuedCommands()
{
	QElapsedTimer elapsed;
	elapsed.start();

	while(!m_msgqueue.isEmpty() && elapsed.elapsed() < 100) {
		receiveCommand(m_msgqueue.takeFirst());
	}

	if(!m_msgqueue.isEmpty()) {
		qDebug("Taking a breather. Still %d messages in the queue.", m_msgqueue.size());
		m_isQueued = true;
		m_queuetimer->start(20);
	} else {
		m_isQueued = false;
	}
}

void StateTracker::receiveCommand(protocol::MessagePtr msg)
{
	static const uint HISTORY_SIZE_LIMIT = 10 * 1024*1024;

	if(msg->type() == protocol::MSG_INTERNAL) {
		const auto &ci = msg.cast<protocol::ClientInternal>();
		if(ci.internalType() == protocol::ClientInternal::Type::Catchup)
			emit catchupProgress(ci.value());
		return;
	}

	if(m_history.lengthInBytes() > HISTORY_SIZE_LIMIT) {
		const uint oldlen = m_history.lengthInBytes();

		qDebug() << "Message stream history size limit reached at" << oldlen / float(1024*1024) << "Mb. Clearing..";
		m_history.cleanup(_localfork.isEmpty() ? m_history.end() : _localfork.offset());
		qDebug() << "Released" << (oldlen-m_history.lengthInBytes()) / float(1024*1024) << "Mb.";
		m_fullhistory = false;

		// Clear out old savepoints
		// First, find the oldest undo point in the stream
		int undopoint = m_history.offset();
		while(undopoint<m_history.end()) {
			if(m_history.at(undopoint)->type() == protocol::MSG_UNDOPOINT)
				break;
			++undopoint;
		}

		if(undopoint == m_history.end()) {
			qWarning() << "no undo point found after cleaning history!";
		} else {
			// Find the newest savepoint older or same age as the undo point

			// If a local fork exists, we need a savepoint that precedes it in case we need to roll back.
			if(!_localfork.isEmpty())
				undopoint = qMin(undopoint, _localfork.offset());

			int savepoint=0;
			while(savepoint < m_savepoints.count()) {
				if(m_savepoints[savepoint]->streampointer >= undopoint) {
					--savepoint;
					break;
				}
				++savepoint;
			}

			// Remove redundant save points
			qDebug() << "removing" << savepoint << "redundant save points out of" << m_savepoints.count();
			while(savepoint-- > 0)
				m_savepoints.removeFirst();
		}
	}

	// Add command to history and execute it
	m_history.append(msg);

	LocalFork::MessageAction lfa = _localfork.handleReceivedMessage(msg, affectedArea(msg));

	// Undo messages are not handled locally (at the moment)
	if(lfa == LocalFork::ALREADYDONE && (msg->type()==protocol::MSG_UNDO || msg->type()==protocol::MSG_UNDOPOINT))
		lfa = LocalFork::CONCURRENT;

	if(lfa==LocalFork::ROLLBACK) {
		// Uh oh! An inconsistency was detected: roll back the history and replay

		// first, find the newest savepoint that precedes the fork
		int savepoint = m_savepoints.size()-1;
		while(savepoint>=0) {
			if(m_savepoints.at(savepoint)->streampointer <= _localfork.offset())
				break;
			--savepoint;
		}

		if(savepoint<0) {
			// should never happen
			qFatal("No savepoint for rolling back local fork at %d!", _localfork.offset());
		} else {
			const StateSavepoint &sp = m_savepoints.at(savepoint);
			qDebug("inconsistency at %d (local fork at %d). Rolling back to %d", m_history.end(), _localfork.offset(), sp->streampointer);

			revertSavepointAndReplay(sp);
		}

	} else if(lfa==LocalFork::CONCURRENT) {
		// Concurrent operation: safe to execute
		int pos = m_history.end() - 1;
		handleCommand(msg, false, pos);
	} // else ALREADYDONE
}

void StateTracker::handleCommand(protocol::MessagePtr msg, bool replay, int pos)
{
	switch(msg->type()) {
		using namespace protocol;
		case MSG_CANVAS_RESIZE:
			handleCanvasResize(msg.cast<CanvasResize>(), pos);
			break;
		case MSG_LAYER_CREATE:
			handleLayerCreate(msg.cast<LayerCreate>());
			break;
		case MSG_LAYER_ATTR:
			handleLayerAttributes(msg.cast<LayerAttributes>());
			break;
		case MSG_LAYER_VISIBILITY:
			handleLayerVisibility(msg.cast<LayerVisibility>());
			break;
		case MSG_LAYER_RETITLE:
			handleLayerTitle(msg.cast<LayerRetitle>());
			break;
		case MSG_LAYER_ORDER:
			handleLayerOrder(msg.cast<LayerOrder>());
			break;
		case MSG_LAYER_DELETE:
			handleLayerDelete(msg.cast<LayerDelete>());
			break;
		case MSG_TOOLCHANGE:
			handleToolChange(msg.cast<ToolChange>());
			break;
		case MSG_PEN_MOVE:
			handlePenMove(msg.cast<PenMove>());
			break;
		case MSG_PEN_UP:
			handlePenUp(msg.cast<PenUp>());
			break;
		case MSG_PUTIMAGE:
			handlePutImage(msg.cast<PutImage>());
			break;
		case MSG_UNDOPOINT:
			handleUndoPoint(msg.cast<UndoPoint>(), replay, pos);
			break;
		case MSG_UNDO:
			handleUndo(msg.cast<Undo>());
			break;
		case MSG_ANNOTATION_CREATE:
			handleAnnotationCreate(msg.cast<AnnotationCreate>());
			break;
		case MSG_ANNOTATION_RESHAPE:
			handleAnnotationReshape(msg.cast<AnnotationReshape>());
			break;
		case MSG_ANNOTATION_EDIT:
			handleAnnotationEdit(msg.cast<AnnotationEdit>());
			break;
		case MSG_ANNOTATION_DELETE:
			handleAnnotationDelete(msg.cast<AnnotationDelete>());
			break;
		case MSG_FILLRECT:
			handleFillRect(msg.cast<FillRect>());
			break;
		case MSG_REGION_MOVE:
			handleMoveRegion(msg.cast<MoveRegion>());
			break;
		default:
			qWarning() << "Unhandled drawing command" << msg->type() << msg->messageName();
			return;
	}
}

/**
 * @brief Network disconnected, so end remote drawing processes
 */
void StateTracker::endRemoteContexts()
{
	// Add local fork to the mainline history
	QList<protocol::MessagePtr> localfork = _localfork.messages();
	_localfork.clear();

	for(protocol::MessagePtr m : localfork)
		m_history.append(m);

	// End drawing contexts
	QHashIterator<int, DrawingContext> iter(_contexts);
	while(iter.hasNext()) {
		iter.next();
		if(iter.key() != localId()) {
			// Simulate pen-up
			if(iter.value().pendown)
				receiveQueuedCommand(protocol::MessagePtr(new protocol::PenUp(iter.key())));
		}
	}
}

/**
 * @brief Playback ended, make sure drawing contexts (local one included) are ended
 */
void StateTracker::endPlayback()
{
	QHashIterator<int, DrawingContext> iter(_contexts);
	while(iter.hasNext()) {
		iter.next();
		if(iter.value().pendown)
			receiveQueuedCommand(protocol::MessagePtr(new protocol::PenUp(iter.key())));
	}
}



void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd, int pos)
{
	_image->resize(cmd.top(), cmd.right(), cmd.bottom(), cmd.left());

	// Generate the initial savepoint, just in case
	makeSavepoint(pos);
}

void StateTracker::handleLayerCreate(const protocol::LayerCreate &cmd)
{
	paintcore::Layer *layer = _image->createLayer(
		cmd.id(),
		cmd.source(),
		QColor::fromRgba(cmd.fill()),
		(cmd.flags() & protocol::LayerCreate::FLAG_INSERT),
		(cmd.flags() & protocol::LayerCreate::FLAG_COPY),
		cmd.title()
	);

	if(layer) {
		// Note: layers are listed bottom-first in the stack,
		// but topmost first in the view
		m_layerlist->createLayer(
			cmd.id(),
			_image->layerCount() - _image->indexOf(layer->id()) - 1,
			cmd.title()
		);

		// Auto-select layers we create
		// During the startup phase, autoselect new layers or if a default one is set,
		// just the default one.
		if(cmd.contextId() == localId() || (!m_hasParticipated && (cmd.id() == m_layerlist->defaultLayer() || !m_layerlist->defaultLayer())))
			emit layerAutoselectRequest(cmd.id());
	}
}

void StateTracker::handleLayerAttributes(const protocol::LayerAttributes &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.id());
	if(!layer) {
		qWarning() << "received layer attributes for non-existent layer" << cmd.id();
		return;
	}
	
	layer->setOpacity(cmd.opacity());
	layer->setBlend(paintcore::BlendMode::Mode(cmd.blend()));
	m_layerlist->changeLayer(cmd.id(), cmd.opacity() / 255.0, paintcore::BlendMode::Mode(cmd.blend()));
}

void StateTracker::handleLayerVisibility(const protocol::LayerVisibility &cmd)
{
	// Layer visibility affects the sending user only
	// (to hide a layer from all users, one can just set its opacity to zero.)
	if(cmd.contextId() != localId())
		return;

	paintcore::Layer *layer = _image->getLayer(cmd.id());
	if(!layer) {
		qWarning() << "received layer visibility for non-existent layer" << cmd.id();
		return;
	}

	layer->setHidden(!cmd.visible());
	m_layerlist->setLayerHidden(cmd.id(), !cmd.visible());
}

void StateTracker::previewLayerOpacity(int id, float opacity)
{
	paintcore::Layer *layer = _image->getLayer(id);
	Q_ASSERT(layer);
	if(!layer)
		return;
	layer->setOpacity(opacity*255);
}

void StateTracker::handleLayerTitle(const protocol::LayerRetitle &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.id());
	if(!layer) {
		qWarning() << "received layer title for non-existent layer" << cmd.id();
		return;
	}

	layer->setTitle(cmd.title());
	m_layerlist->retitleLayer(cmd.id(), cmd.title());
}

void StateTracker::handleLayerOrder(const protocol::LayerOrder &cmd)
{
	QList<uint16_t> currentOrder;
	for(int i=0;i<_image->layerCount();++i)
		currentOrder.append(_image->getLayerByIndex(i)->id());

	QList<uint16_t> newOrder = cmd.sanitizedOrder(currentOrder);

	if(newOrder != cmd.order()) {
		qWarning() << "invalid layer reorder!";
		qWarning() << "current order is:" << currentOrder;
		qWarning() << "  the new one was:" << cmd.order();
		qWarning() << "  fixed order is:" << newOrder;
	}

	_image->reorderLayers(newOrder);
	m_layerlist->reorderLayers(newOrder);
}

void StateTracker::handleLayerDelete(const protocol::LayerDelete &cmd)
{
	if(cmd.merge())
		_image->mergeLayerDown(cmd.id());
	_image->deleteLayer(cmd.id());
	m_layerlist->deleteLayer(cmd.id());
}

void StateTracker::handleToolChange(const protocol::ToolChange &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	ctx.tool.updateFromToolchange(cmd);

	paintcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	QString layername;
	if(layer)
		layername = layer->title();
	else
		layername = QStringLiteral("???");

	emit userMarkerAttribs(cmd.contextId(), ctx.tool.brush.color(), layername);
}

void StateTracker::handlePenMove(const protocol::PenMove &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	paintcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	if(!layer) {
		qWarning() << "penMove by user" << cmd.contextId() << "on non-existent layer" << ctx.tool.layer_id;
		return;
	}
	
	for(const protocol::PenPoint &pp : cmd.points()) {
		paintcore::Point p(pp.x / 4.0, pp.y / 4.0, pp.p/qreal(0xffff));
		const int r = ctx.tool.brush.fsize(p.pressure())/2 + 1;

		if(ctx.pendown) {
			layer->drawLine(cmd.contextId(), ctx.tool.brush, ctx.lastpoint, p, ctx.stroke);
			ctx.boundingRect |= QRect(p.x() - r, p.y() - r, r*2, r*2);

		} else {
			ctx.pendown = true;
			ctx.stroke = paintcore::StrokeState(ctx.tool.brush);
			ctx.boundingRect = QRect(p.x() - r, p.y() - r, r*2, r*2);
			layer->dab(cmd.contextId(), ctx.tool.brush, p, ctx.stroke);
		}
		ctx.lastpoint = p;
	}

	if(_showallmarkers || cmd.contextId() != localId())
		emit userMarkerMove(cmd.contextId(), ctx.lastpoint, 0);
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	paintcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	if(!layer) {
		qWarning() << "penUp by user" << cmd.contextId() << "on non-existent layer" << ctx.tool.layer_id;
		return;
	}

	// This ends an indirect stroke. In incremental mode, this does nothing.
	layer->mergeSublayer(cmd.contextId());

	ctx.pendown = false;
	emit userMarkerHide(cmd.contextId());
}

void StateTracker::handlePutImage(const protocol::PutImage &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.layer());
	if(!layer) {
		qWarning() << "putImage on non-existent layer" << cmd.layer();
		return;
	}
	const int expectedLen = cmd.width() * cmd.height() * 4;
	QByteArray data = qUncompress(cmd.image());
	if(data.length() != expectedLen) {
		qWarning() << "Invalid putImage: Expected" << expectedLen << "bytes, but got" << data.length();
		return;
	}
	QImage img(reinterpret_cast<const uchar*>(data.constData()), cmd.width(), cmd.height(), QImage::Format_ARGB32);
	layer->putImage(cmd.x(), cmd.y(), img, paintcore::BlendMode::Mode(cmd.blendmode()));

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), QPointF(cmd.x() + cmd.width()/2, cmd.y()+cmd.height()/2), 0);
}

void StateTracker::handleFillRect(const protocol::FillRect &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.layer());
	if(!layer) {
		qWarning("fillRect on non-existent layer %d", cmd.layer());
		return;
	}

	layer->fillRect(QRect(cmd.x(), cmd.y(), cmd.width(), cmd.height()), QColor::fromRgba(cmd.color()), paintcore::BlendMode::Mode(cmd.blend()));

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), QPointF(cmd.x() + cmd.width()/2, cmd.y()+cmd.height()/2), 0);
}

void StateTracker::handleMoveRegion(const protocol::MoveRegion &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.layer());
	if(!layer) {
		qWarning("moveRegion on non-existent layer %d", cmd.layer());
		return;
	}

	if(cmd.contextId() == m_myId) {
		// Moving the layer for real: make sure my preview is removed
		layer->removeSublayer(-1);
	}

	// Source region bounding rectangle
	const QRect bounds(cmd.bx(), cmd.by(), cmd.bw(), cmd.bh());

	// Target quad
	const QPolygon target({
		QPoint(cmd.x1(), cmd.y1()),
		QPoint(cmd.x2(), cmd.y2()),
		QPoint(cmd.x3(), cmd.y3()),
		QPoint(cmd.x4(), cmd.y4())
	});

	// Sanity check: without a size limit, a user could create huge temporary images and potentially other clients
	const int targetArea = target.boundingRect().size().width() * target.boundingRect().size().height();
	if(targetArea > _image->width() * _image->height()) {
		qWarning("moveRegion: cannot scale beyond image size");
		return;
	}

	// Get mask bitmap
	QImage mask;
	if(!cmd.mask().isEmpty()) {
		const int expectedLen = (cmd.bw()+31)/32 * 4 * cmd.bh(); // 1bpp lines padded to 32bit boundaries
		QByteArray maskData = qUncompress(cmd.mask());
		if(maskData.length() != expectedLen) {
			qWarning("Invalid moveRegion mask: Expected %d bytes, but got %d", expectedLen, maskData.length());
			return;
		}

		mask = QImage(reinterpret_cast<const uchar*>(maskData.constData()), cmd.bw(), cmd.bh(), QImage::Format_Mono);
		mask.setColor(0, 0);
		mask.setColor(1, 0xffffffff);
		mask = mask.convertToFormat(QImage::Format_ARGB32);
	}

	// Extract selected pixels
	QImage selbuf = layer->toImage().copy(bounds); // TODO optimize

	// Mask out unselected pixels (if necessary)
	if(!mask.isNull()) {
		QPainter mp(&selbuf);
		mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		mp.drawImage(0, 0, mask);
	}

	// Transform selected pixels

	QPoint offset;
	QImage transformed;
	if(target.boundingRect().size() == bounds.size() && target[0].x() < target[1].x()) {
		// Just translation
		transformed = selbuf;
		offset = target[0];

	} else {
		transformed = tools::SelectionTool::transformSelectionImage(selbuf, target, &offset);
		if(transformed.isNull()) {
			qWarning("moveRegion: transformation failed (%d, %d -> %d, %d -> %d, %d -> %d, %d)!",
				cmd.x1(), cmd.y1(), cmd.x2(), cmd.y2(), cmd.x3(), cmd.y3(), cmd.x4(), cmd.y4());
			return;
		}
	}

	// Erase selection mask and draw transformed image
	if(mask.isNull()) {
		layer->fillRect(bounds, Qt::transparent, paintcore::BlendMode::MODE_REPLACE);
	} else {
		layer->putImage(bounds.x(), bounds.y(), mask, paintcore::BlendMode::MODE_ERASE);
	}

	layer->putImage(offset.x(), offset.y(), transformed, paintcore::BlendMode::MODE_NORMAL);

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), target.boundingRect().center(), 0);
}

void StateTracker::handleUndoPoint(const protocol::UndoPoint &cmd, bool replay, int pos)
{
	// New undo point. This branches the undo history. Since we store the
	// commands in a linear sequence, this branching is represented by marking
	// the unreachable commands as GONE.
	if(!replay) {
		int i = pos - 1; // skip the one just added
		int upCount = 1;

		// Mark undone actions as GONE
		while(m_history.isValidIndex(i) && upCount < protocol::UNDO_DEPTH_LIMIT) {
			protocol::MessagePtr msg = m_history.at(i);
			if(msg->type() == protocol::MSG_UNDOPOINT)
				++upCount;
			if(msg->contextId() == cmd.contextId()) {
				// optimization: we can stop searching after finding the first GONE command
				if(msg->type() != protocol::MSG_UNDO && msg->undoState() == protocol::GONE)
					break;
				else if(msg->undoState() == protocol::UNDONE)
					msg->setUndoState(protocol::GONE);
			}
			--i;
		}

		// Keep rewinding until the oldest reachable undo point is found
		while(m_history.isValidIndex(i) && upCount < protocol::UNDO_DEPTH_LIMIT) {
			if(m_history.at(i)->type() == protocol::MSG_UNDOPOINT) {
				++upCount;
			}
			--i;
		}

		// Release all state savepoints older then the oldest UndoPoint
		if(upCount>=protocol::UNDO_DEPTH_LIMIT) {
			if(!_localfork.isEmpty())
				i = qMin(i, _localfork.offset() - 1);

			QMutableListIterator<StateSavepoint> spi(m_savepoints);
			spi.toBack();

			// In order to be able to return to the oldest undo point, we must leave
			// one snapshot that is as old, or older.
			bool first = true;

			while(spi.hasPrevious()) {
				const StateSavepoint &sp = spi.previous();
				if(sp->streampointer <= i) {
					if(first)
						first = false;
					else
						spi.remove();
				}
			}
		}
	}

	// Make a new savepoint (if possible)
	makeSavepoint(pos);

	// To be correct, we should set the "participated" flag on any command
	// sent by us. In practice, however, an UndoPoint is always sent
	// when making changes so it is enough to set the flag here.
	if(cmd.contextId() == localId())
		m_hasParticipated = true;
}

void StateTracker::handleUndo(protocol::Undo &cmd)
{
	// Undo/redo commands are never replayed, so start
	// by marking it as unavailable.
	cmd.setUndoState(protocol::GONE);

	const uint8_t ctxid = cmd.overrideId() ? cmd.overrideId() : cmd.contextId();

	// Step 1. Find undo or redo point
	int pos = m_history.end();
	int upCount = 0;

	if(cmd.isRedo()) {
		// Find the oldest undone UndoPoint
		int redostart = pos;
		while(m_history.isValidIndex(--pos) && upCount <= protocol::UNDO_DEPTH_LIMIT) {
			const protocol::MessagePtr msg = m_history.at(pos);
			if(msg->type() == protocol::MSG_UNDOPOINT) {
				++upCount;
				if(msg->contextId() == ctxid) {
					if(msg->undoState() != protocol::DONE)
						redostart = pos;
					else
						break;
				}
			}
		}

		if(redostart == m_history.end()) {
			qDebug() << "nothing to redo for user" << cmd.contextId();
			return;
		}
		pos = redostart;

	} else {
		// Find the newest UndoPoint not marked as undone.
		while(m_history.isValidIndex(--pos) && upCount <= protocol::UNDO_DEPTH_LIMIT) {
			const protocol::MessagePtr msg = m_history.at(pos);
			if(msg->type() == protocol::MSG_UNDOPOINT) {
				++upCount;
				if(msg->contextId() == ctxid && msg->undoState() == protocol::DONE)
					break;
			}
		}
	}

	if(upCount > protocol::UNDO_DEPTH_LIMIT) {
		qDebug() << "user" << cmd.contextId() << "cannot undo/redo beyond history limit";
		return;
	}

	// pos is now at the starting UndoPoint
	if(!m_history.isValidIndex(pos)) {
		qWarning() << "Cannot " << (cmd.isRedo() ? "redo" : "undo") << "action by user" << ctxid << ": not enough messages in buffer!";
		return;
	}

	// Step 2. Find nearest save point
	StateSavepoint savepoint;
	for(int i=m_savepoints.count()-1;i>=0;--i) {
		if(m_savepoints.at(i)->streampointer <= pos) {
			savepoint = m_savepoints.at(i);
			break;
		}
	}

	if(!savepoint) {
		qWarning() << "Cannot" << (cmd.isRedo() ? "redo" : "undo") << "action by user" << ctxid << ": no savepoint found!";
		return;
	}

	// Step 3. (Un)mark all actions by the user as undone
	if(cmd.isRedo()) {
		int i=pos;
		int sequence=2;
		// Un-undo messages until the start of the next undone sequence
		while(i<m_history.end()) {
			protocol::MessagePtr msg = m_history.at(i);
			if(msg->contextId() == ctxid) {
				if(msg->type() == protocol::MSG_UNDOPOINT && msg->undoState() != protocol::GONE)
					if(--sequence==0)
						break;

				// GONE messages cannot be redone
				if(msg->undoState() == protocol::UNDONE)
					msg->setUndoState(protocol::DONE);
			}
			++i;
		}

	} else {
		// Mark all messages from undo point to the end as undone.
		for(int i=pos;i<m_history.end();++i) {
			protocol::MessagePtr msg = m_history.at(i);
			if(msg->contextId() == ctxid)
				msg->setUndoState(protocol::MessageUndoState(protocol::UNDONE | msg->undoState()));
		}
	}

	// Step 4. Revert to the savepoint and replay with undone commands removed (or added back)
	revertSavepointAndReplay(savepoint);
}

StateSavepoint StateTracker::createSavepoint(int pos)
{
	StateSavepoint savepoint;
	savepoint->timestamp = QDateTime::currentMSecsSinceEpoch();
	savepoint->streampointer = pos<0 ? m_history.end() : pos;
	savepoint->canvas = _image->makeSavepoint();
	savepoint->ctxstate = _contexts;
	savepoint->layermodel = m_layerlist->getLayers();

	return savepoint;
}

void StateTracker::makeSavepoint(int pos)
{
	// Make sure there is something in the message stream buffer
	if(m_history.end() <= m_history.offset())
		return;

	// Don't make savepoints while a local fork exists, since
	// there will be stuff on the canvas that is not yet in
	// the mainline session history
	if(!_localfork.isEmpty())
		return;

	// Check if sufficient time and actions has elapsed from previous savepoint
	if(!m_savepoints.isEmpty()) {
		const StateSavepoint sp = m_savepoints.last();
		quint64 now = QDateTime::currentMSecsSinceEpoch();
		if(now - sp->timestamp < 1000 && m_history.end() - sp->streampointer < 100)
			return;
	}

	// Looks like a good spot for a savepoint
	m_savepoints.append(createSavepoint(pos));
}


void StateTracker::resetToSavepoint(const StateSavepoint savepoint)
{
	// This function is called when jumping to a recorded savepoint

	m_history.resetTo(savepoint->streampointer);
	m_savepoints.clear();

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;
	m_layerlist->setLayers(savepoint->layermodel);

	m_savepoints.append(savepoint);
}

void StateTracker::revertSavepointAndReplay(const StateSavepoint savepoint)
{
	// This function is called when reverting to an earlier state to undo
	// an action.

	Q_ASSERT(m_savepoints.contains(savepoint));

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;
	m_layerlist->setLayers(savepoint->layermodel);

	// Reverting a savepoint destroys all newer savepoints
	while(m_savepoints.last() != savepoint)
		m_savepoints.removeLast();

	// Replay all not-undo actions (and local fork)
	int pos = savepoint->streampointer + 1;
	while(pos < m_history.end()) {
		if(m_history.at(pos)->undoState() == protocol::DONE) {
			handleCommand(m_history.at(pos), true, pos);
		}
		++pos;
	}

	// Note. At this point we could replay the localfork, but this tends to
	// cause more trouble than its worth. Since we're receiving data, the data
	// should be making the roundtrip any moment now anyway.
	_localfork.clear();

	emit retconned();
}

void StateTracker::handleAnnotationCreate(const protocol::AnnotationCreate &cmd)
{
	_image->annotations()->addAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
	if(cmd.contextId() == localId())
		emit myAnnotationCreated(cmd.id());
}

void StateTracker::handleAnnotationReshape(const protocol::AnnotationReshape &cmd)
{
	_image->annotations()->reshapeAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
}

void StateTracker::handleAnnotationEdit(const protocol::AnnotationEdit &cmd)
{
	_image->annotations()->changeAnnotation(
		cmd.id(),
		cmd.text(),
		cmd.flags() & protocol::AnnotationEdit::FLAG_PROTECT,
		cmd.flags() & (protocol::AnnotationEdit::FLAG_VALIGN_BOTTOM|protocol::AnnotationEdit::FLAG_VALIGN_CENTER),
		QColor::fromRgba(cmd.bg())
		);
}

void StateTracker::handleAnnotationDelete(const protocol::AnnotationDelete &cmd)
{
	_image->annotations()->deleteAnnotation(cmd.id());
}

void StateSavepoint::toDatastream(QDataStream &out) const
{
	Q_ASSERT(m_data);
	const auto *d = m_data;

	// Write stream pointer
	out << quint32(d->streampointer);

	// Write drawing contexts
	out << quint8(d->ctxstate.size());
	for(const quint8 ctxid : d->ctxstate.keys()) {
		const DrawingContext &ctx = d->ctxstate[ctxid];

		// write context ID
		out << ctxid;

		// write tool context
		protocol::MessagePtr tc = net::command::brushToToolChange(ctxid, ctx.tool.layer_id, ctx.tool.brush);
		QByteArray tcb(tc->length(), '\0');
		tc->serialize(tcb.data());
		out.writeBytes(tcb.data(), tcb.length());

		// write last point
		out << ctx.lastpoint.x();
		out << ctx.lastpoint.y();
		out << ctx.lastpoint.pressure();

		// write pendown bit
		out << ctx.pendown;

		// write stroke state
		out << ctx.stroke.distance << ctx.stroke.smudgeDistance << ctx.stroke.smudgeColor;
	}

	// Write layer model
	out << quint8(d->layermodel.size());
	for(const LayerListItem &layer : d->layermodel) {
		// Write layer ID
		out << qint32(layer.id);

		// Write layer title
		out << layer.title;

		// Write layer opacity and flags
		out << layer.opacity;
		out << quint8(layer.blend);
		out << layer.hidden << layer.locked;

		// Write layer ACL
		out << layer.exclusive;
	}

	// Write layer stack
	d->canvas->toDatastream(out);
}

StateSavepoint StateSavepoint::fromDatastream(QDataStream &in, StateTracker *owner)
{
	StateSavepoint sp;
	sp.m_data = new StateSavepoint::Data;
	auto *d = sp.m_data;

	// Read stream pointer
	quint32 sptr;
	in >> sptr;
	d->streampointer = sptr;

	// Read drawing contexts
	quint8 contexts;
	in >> contexts;
	while(contexts--) {
		DrawingContext ctx;

		// Read context id
		quint8 ctxid;
		in >> ctxid;

		// Read tool context
		char *msgbuf;
		unsigned int msglen;
		in.readBytes(msgbuf, msglen);

		protocol::Message *tc = protocol::Message::deserialize((const uchar*)msgbuf, msglen, true);
		delete [] msgbuf;
		if(!tc) {
			qWarning() << "invalid tool change message in snapshot!";
			return StateSavepoint();
		}
		ctx.tool.updateFromToolchange(static_cast<const protocol::ToolChange&>(*tc));
		delete tc;

		// Read last point
		qreal lpx, lpy, lpp;
		in >> lpx >> lpy >> lpp;
		ctx.lastpoint = paintcore::Point(lpx, lpy, lpp);

		// Read pendown bit
		in >> ctx.pendown;

		// Read stroke state
		in >> ctx.stroke.distance >> ctx.stroke.smudgeDistance >> ctx.stroke.smudgeColor;

		// Note: ctx.bounds is used only for retconning during online drawing
		// so we don't need to restore it here, since saved snapshots are currently
		// used only for session playback.

		d->ctxstate[ctxid] = ctx;
	}

	// Read layer list
	quint8 layercount;
	in >> layercount;
	while(layercount--) {
		// Read layer ID
		qint32 layerid;
		in >> layerid;

		// Read layer title
		QString title;
		in >> title;

		// Read opacity and flags
		float opacity;
		in >> opacity;

		quint8 blend;
		in >> blend;

		bool hidden;
		in >> hidden;

		bool locked;
		in >> locked;

		// Read layer ACL
		QList<uint8_t> acls;
		in >> acls;

		sp->layermodel.append(LayerListItem {
			layerid,
			title,
			opacity,
			paintcore::BlendMode::Mode(blend),
			hidden,
			locked,
			acls
		});
	}

	// Read layerstack snapshot
	d->canvas = paintcore::Savepoint::fromDatastream(in, owner->image());

	return sp;
}

bool StateTracker::isLayerLocked(int id) const
{
	for(const LayerListItem &l : m_layerlist->getLayers()) {
		if(l.id == id)
			return l.isLockedFor(localId());
	}

	qWarning("isLayerLocked(%d): no such layer!", id);
	return false;
}

void StateTracker::resetLocalFork()
{
	if(!_localfork.isEmpty()) {
		int savepoint = m_savepoints.size()-1;
		while(savepoint>0) {
			if(m_savepoints.at(savepoint)->streampointer <= _localfork.offset())
				break;
			--savepoint;
		}

		if(savepoint<0) {
			// should never happen
			qFatal("No savepoint for rolling back local fork at %d!", _localfork.offset());
		} else {
			const StateSavepoint &sp = m_savepoints.at(savepoint);
			qDebug("Resetting local fork and rolling back %d commands", m_history.end() - sp->streampointer);

			_localfork.clear();
			revertSavepointAndReplay(sp);
		}
	}
}

/**
 * @brief Get the affected area of the given message
 *
 * Note. This uses the current state!
 *
 * @param msg
 * @return
 */
AffectedArea StateTracker::affectedArea(protocol::MessagePtr msg) const
{
	Q_ASSERT(msg->isCommand());

	switch(msg->type()) {
	using namespace protocol;
	case MSG_LAYER_CREATE: return AffectedArea(AffectedArea::LAYERATTRS, msg.cast<LayerCreate>().id());
	case MSG_LAYER_ATTR: return AffectedArea(AffectedArea::LAYERATTRS, msg.cast<LayerAttributes>().id());
	case MSG_LAYER_RETITLE: return AffectedArea(AffectedArea::LAYERATTRS, msg.cast<LayerRetitle>().id());

	case MSG_PUTIMAGE: {
		const PutImage &m = msg.cast<PutImage>();
		return AffectedArea(AffectedArea::PIXELS, m.layer(), QRect(m.x(), m.y(), m.width(), m.height()));
	}
	case MSG_TOOLCHANGE: return AffectedArea(AffectedArea::USERATTRS, 0);
	case MSG_PEN_MOVE: {
		const DrawingContext &ctx = _contexts.value(msg->contextId());

		// Non-incremental brush draws on a private layer: we must check ordering in PenUp
		if(!ctx.tool.brush.incremental())
			return AffectedArea(AffectedArea::USERATTRS, 0);

		const PenMove &m = msg.cast<PenMove>();

		// Find the bounding rectangle of the received piece of the stroke.
		QRect bounds;

		if(ctx.pendown)
			bounds  = QRect(ctx.lastpoint.toPoint(), QSize(1,1));
		else
			bounds = QRect(m.points().first().x/4, m.points().first().y/4, 1, 1);

		for(const PenPoint &pp : m.points()) {
			bounds |= QRect(pp.x/4, pp.y/4, 1, 1);
		}

		const int r = qMax(ctx.tool.brush.size1(), ctx.tool.brush.size2()) / 2 + 1;
		bounds.adjust(-r, -r, r, r);
		return AffectedArea(AffectedArea::PIXELS, ctx.tool.layer_id, bounds);
	}
	case MSG_PEN_UP: {
		const DrawingContext &ctx = _contexts.value(msg->contextId());
		if(ctx.tool.brush.incremental())
			return AffectedArea(AffectedArea::USERATTRS, 0);

		// Non-incremental brushes get composited only at pen-up.
		// We need the bounding rectangle of the entire stroke.
		return AffectedArea(AffectedArea::PIXELS, ctx.tool.layer_id, ctx.boundingRect);
	}
	case MSG_FILLRECT: {
		const FillRect &fr = msg.cast<FillRect>();
		return AffectedArea(AffectedArea::PIXELS, fr.layer(), QRect(fr.x(), fr.y(), fr.width(), fr.height()));
	}

	case MSG_ANNOTATION_CREATE: return AffectedArea(AffectedArea::ANNOTATION, msg.cast<AnnotationCreate>().id());
	case MSG_ANNOTATION_RESHAPE: return AffectedArea(AffectedArea::ANNOTATION, msg.cast<AnnotationReshape>().id());
	case MSG_ANNOTATION_EDIT: return AffectedArea(AffectedArea::ANNOTATION, msg.cast<AnnotationEdit>().id());
	case MSG_ANNOTATION_DELETE: return AffectedArea(AffectedArea::ANNOTATION, msg.cast<AnnotationDelete>().id());

	case MSG_UNDOPOINT: return AffectedArea(AffectedArea::USERATTRS, 0);

	default: return AffectedArea(AffectedArea::EVERYTHING, 0);
	}
}

}
