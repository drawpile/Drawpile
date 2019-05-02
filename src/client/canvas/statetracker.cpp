/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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
#include "canvasmodel.h"
#include "layerlist.h"
#include "loader.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "brushes/brushpainter.h"
#include "net/commands.h"
#include "net/internalmsg.h"
#include "tools/selection.h" // for selection transform utils

#include "../shared/net/brushes.h"
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

struct StateSavepoint::Data : public QSharedData {
	int streampointer = 0;
	qint64 timestamp = 0;
	paintcore::Savepoint canvas;
	QVector<LayerListItem> layermodel;
};

StateSavepoint::StateSavepoint()
{
}

StateSavepoint::StateSavepoint(Data *d)
	: d(d)
{
}

StateSavepoint::StateSavepoint(const StateSavepoint &other)
	: d(other.d)
{
}

StateSavepoint &StateSavepoint::operator=(const StateSavepoint &other)
{
	d = other.d;
	return *this;
}

StateSavepoint::~StateSavepoint()
{
}

qint64 StateSavepoint::timestamp() const
{
	return d ? d->timestamp : 0;
}

paintcore::Savepoint StateSavepoint::canvas() const
{
	Q_ASSERT(d);
	return d->canvas;
}

QImage StateSavepoint::thumbnail(const QSize &maxSize) const
{
	if(!d)
		return QImage();

	paintcore::LayerStack stack;
	stack.editor(0).restoreSavepoint(d->canvas);
	QImage img = stack.toFlatImage(true, true);
	if(img.width() > maxSize.width() || img.height() > maxSize.height()) {
		img = img.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	return img;
}

protocol::MessageList StateSavepoint::initCommands(uint8_t contextId, const CanvasModel *canvas) const
{
	if(!d)
		return protocol::MessageList();

	paintcore::LayerStack stack;
	stack.editor(0).restoreSavepoint(d->canvas);
	SnapshotLoader loader(contextId, &stack, canvas->aclFilter());
	loader.setDefaultLayer(canvas->layerlist()->defaultLayer());
	loader.setPinnedMessage(canvas->pinnedMessage());
	return loader.loadInitCommands();
}

StateSavepoint StateSavepoint::fromCanvasSavepoint(const paintcore::Savepoint &savepoint)
{
	auto *d = new StateSavepoint::Data;

	d->timestamp = QDateTime::currentMSecsSinceEpoch();
	d->canvas = savepoint;

	for(const paintcore::Layer *l : savepoint.layers) {
		d->layermodel << LayerListItem {
			uint16_t(l->id()),
			l->title(),
			l->opacity() / 255.0f,
			l->blendmode(),
			l->isHidden(),
			l->isCensored(),
			l->isFixed()
		};
	}
	return StateSavepoint(d);
}

/**
 * @brief Construct a state tracker instance
 *
 * @param image the canvas content
 * @param layerlist layer list model for the UI
 * @param myId ID of the local user
 * @param parent
 */
StateTracker::StateTracker(paintcore::LayerStack *image, LayerListModel *layerlist, uint8_t myId, QObject *parent)
	: QObject(parent),
		m_layerstack(image),
		m_layerlist(layerlist),
		m_myId(myId),
		m_myLastLayer(-1),
		_showallmarkers(false),
		m_hasParticipated(false),
		m_localPenDown(false),
		m_isQueued(false)
{
	connect(m_layerlist, &LayerListModel::layerOpacityPreview, this, &StateTracker::previewLayerOpacity);

	// Reset local fork if it falls behind too much
	m_localfork.setFallbehind(10000);

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
	m_hasParticipated = false;
	m_localPenDown = false;
	m_msgqueue.clear();
	m_localfork.clear();
	m_layerlist->clear();

	// Make sure there is always a savepoint in the history
	makeSavepoint(m_history.end()-1);
}

void StateTracker::localCommand(protocol::MessagePtr msg)
{
	// A fork is created at the end of the mainline history
	if(m_localfork.isEmpty()) {
		m_localfork.setOffset(m_history.end()-1);

		// Since the presence of a local fork blocks savepoint creation,
		// now is a good time to try to create one.
		if(msg->type() == protocol::MSG_UNDOPOINT)
			makeSavepoint(m_history.end()-1);
	}

	m_localfork.addLocalMessage(msg, affectedArea(msg));

	// Remember last used layer
	switch(msg->type()) {
	using namespace protocol;
	case MSG_DRAWDABS_CLASSIC:
	case MSG_DRAWDABS_PIXEL:
	case MSG_DRAWDABS_PIXEL_SQUARE:
	case MSG_LAYER_CREATE:
	case MSG_PUTIMAGE:
	case MSG_FILLRECT:
	case MSG_REGION_MOVE:
		m_myLastLayer = msg->layer();
		break;
	default: break;
	}

	// for the future: handle undo messages in the local fork too
	if(msg->type() != protocol::MSG_UNDO && msg->type() != protocol::MSG_UNDOPOINT) {
		int pos = m_history.end() - 1;
		handleCommand(msg, false, pos);
	}
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
	if(msg->type() == protocol::MSG_INTERNAL) {
		// MSG_INTERNAL is a pseudo-message used for internal synchronization
		const auto &ci = msg.cast<protocol::ClientInternal>();
		switch(ci.internalType()) {
		case protocol::ClientInternal::Type::Catchup:
			emit catchupProgress(ci.value());
			break;
		case protocol::ClientInternal::Type::SequencePoint:
			emit sequencePoint(ci.value());
			break;
		case protocol::ClientInternal::Type::TruncateHistory:
			handleTruncateHistory();
			break;
		}
		return;
	}

	// Add command to history and execute it
	m_history.append(msg);

	LocalFork::MessageAction lfa = m_localfork.handleReceivedMessage(msg, affectedArea(msg));

	// Undo messages are not handled locally (at the moment)
	if(lfa == LocalFork::ALREADYDONE && (msg->type()==protocol::MSG_UNDO || msg->type()==protocol::MSG_UNDOPOINT))
		lfa = LocalFork::CONCURRENT;

	if(lfa==LocalFork::ROLLBACK) {
		// Uh oh! An inconsistency was detected: roll back the history and replay

		// first, find the newest savepoint that precedes the fork
		int savepoint = m_savepoints.size()-1;
		while(savepoint>=0) {
			if(m_savepoints.at(savepoint)->streampointer <= m_localfork.offset())
				break;
			--savepoint;
		}

		if(savepoint<0) {
			// should never happen
			qWarning("No savepoint for rolling back local fork at %d!", m_localfork.offset());

		} else {
			const StateSavepoint &sp = m_savepoints.at(savepoint);
			qDebug("inconsistency at %d (local fork at %d). Rolling back to %d", m_history.end(), m_localfork.offset(), sp->streampointer);

			// Avoid rollback churn by clearing the local fork, but not if
			// local drawing is in progress. If we clear the fork then,
			// we trigger a self-conflict feedback loop until the stroke finishes.
			if(!m_localPenDown)
				m_localfork.clear();

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
		case MSG_DRAWDABS_CLASSIC:
		case MSG_DRAWDABS_PIXEL:
		case MSG_DRAWDABS_PIXEL_SQUARE:
			handleDrawDabs(*msg);
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
		case MSG_PUTTILE:
			handlePutTile(msg.cast<PutTile>());
			break;
		case MSG_CANVAS_BACKGROUND:
			handleCanvasBackground(msg.cast<CanvasBackground>());
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
	auto localfork = m_localfork.messages();
	m_localfork.clear();

	for(protocol::MessagePtr m : localfork)
		m_history.append(m);

	// Make sure there are no lingering indirect strokes
	// TODO this should probably be done with an InternalMsg,
	// in case there is still stuff in the queue
	auto layers = m_layerstack->editor(0);
	layers.mergeAllSublayers();

	m_myLastLayer = -1;
}

/**
 * @brief Playback ended, make sure drawing contexts (local one included) are ended
 */
void StateTracker::endPlayback()
{
	auto layers = m_layerstack->editor(0);
	layers.mergeAllSublayers();
}



void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd, int pos)
{
	{
		auto layers = m_layerstack->editor(cmd.contextId());
		layers.resize(cmd.top(), cmd.right(), cmd.bottom(), cmd.left());
	}

	// Generate the initial savepoint, just in case
	makeSavepoint(pos);
}

void StateTracker::handleCanvasBackground(const protocol::CanvasBackground &cmd)
{
	paintcore::Tile t;
	if(cmd.isSolidColor()) {
		t = paintcore::Tile(QColor::fromRgba(cmd.color()));

	} else {
		QByteArray data = qUncompress(cmd.image());
		if(data.length() != paintcore::Tile::BYTES) {
			qWarning() << "Invalid canvas background: Expected" << paintcore::Tile::BYTES << "bytes, but got" << data.length();
			return;
		}

		t = paintcore::Tile(data);
	}
	t.setLastEditedBy(cmd.contextId());
	m_layerstack->editor(cmd.contextId()).setBackground(t);
}

void StateTracker::handleLayerCreate(const protocol::LayerCreate &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());

	auto layer = layers.createLayer(
		cmd.layer(),
		cmd.source(),
		QColor::fromRgba(cmd.fill()),
		(cmd.flags() & protocol::LayerCreate::FLAG_INSERT),
		(cmd.flags() & protocol::LayerCreate::FLAG_COPY),
		cmd.title()
	);

	if(layer.isNull()) {
		qWarning("Layer creation failed (id=%d, source=%d)", cmd.layer(), cmd.source());
		return;
	}

	// Note: layers are listed bottom-first in the stack,
	// but topmost first in the view
	m_layerlist->createLayer(
		cmd.layer(),
		layers->layerCount() - layers->indexOf(layer->id()) - 1,
		cmd.title()
	);

	// Auto-select layers we create
	// During the startup phase, autoselect new layers or if a default one is set,
	// just the default one. If there is a remembered layer selection, it takes precedence
	// over others.
	if(
			// Autoselect layers created by me
			(m_hasParticipated && cmd.contextId() == localId()) ||
			// If this user has not yet drawn anything...
			(!m_hasParticipated && (
				// ... and if there is no remembered layer...
				((m_myLastLayer <= 0) && ( // ...select default layer or if not selected, any new layer
					layer->id() == m_layerlist->defaultLayer() ||
					!m_layerlist->defaultLayer()
				)) ||
				// ... and if there is a remembered layer, select only that one
				(m_myLastLayer>0 && layer->id() == m_myLastLayer)
			))
	   )
	{
		emit layerAutoselectRequest(layer->id());
	}
}

void StateTracker::handleLayerAttributes(const protocol::LayerAttributes &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("Received layer attributes for non-existent layer #%d", cmd.layer());
		return;
	}

	const auto bm = paintcore::BlendMode::Mode(cmd.blend());

	if(cmd.sublayer()>0) {
		auto sl = layer.getEditableSubLayer(cmd.sublayer(), bm, cmd.opacity());
		// getSubLayer does not touch the attributes if the sublayer already exists
		sl.setBlend(bm);
		sl.setOpacity(cmd.opacity());

	} else {
		layer.setBlend(bm);
		layer.setOpacity(cmd.opacity());
		layer.setCensored(cmd.isCensored());
		layer.setFixed(cmd.isFixed());
		m_layerlist->changeLayer(layer->id(), cmd.isCensored(), cmd.isFixed(), cmd.opacity() / 255.0, paintcore::BlendMode::Mode(cmd.blend()));
	}
}

void StateTracker::handleLayerVisibility(const protocol::LayerVisibility &cmd)
{
	// Layer visibility affects the sending user only
	// (to hide a layer from all users, one can just set its opacity to zero.)
	if(cmd.contextId() != localId())
		return;

	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("Received layer visibility for non-existent layer #%d", cmd.layer());
		return;
	}

	layer.setHidden(!cmd.visible());
	m_layerlist->setLayerHidden(layer->id(), !cmd.visible());
}

void StateTracker::previewLayerOpacity(int id, float opacity)
{
	auto layers = m_layerstack->editor(0);
	auto layer = layers.getEditableLayer(id);

	if(layer.isNull()) {
		qWarning("previewLayerOpacity(%d): no such layer!", id);
		return;
	}
	layer.setOpacity(opacity*255);
}

void StateTracker::handleLayerTitle(const protocol::LayerRetitle &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());

	if(layer.isNull()) {
		qWarning() << "received layer title for non-existent layer" << cmd.layer();
		return;
	}

	layer.setTitle(cmd.title());
	m_layerlist->retitleLayer(layer->id(), cmd.title());
}

void StateTracker::handleLayerOrder(const protocol::LayerOrder &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());

	QList<uint16_t> currentOrder;
	for(int i=0;i<layers->layerCount();++i)
		currentOrder.append(layers->getLayerByIndex(i)->id());

	QList<uint16_t> newOrder = cmd.sanitizedOrder(currentOrder);

	if(newOrder != cmd.order()) {
		qWarning() << "invalid layer reorder!";
		qWarning() << "current order is:" << currentOrder;
		qWarning() << "  the new one was:" << cmd.order();
		qWarning() << "  fixed order is:" << newOrder;
	}

	layers.reorderLayers(newOrder);
	m_layerlist->reorderLayers(newOrder);
}

void StateTracker::handleLayerDelete(const protocol::LayerDelete &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());

	if(cmd.merge())
		layers.mergeLayerDown(cmd.layer());
	layers.deleteLayer(cmd.layer());
	m_layerlist->deleteLayer(cmd.layer());
}

void StateTracker::handleDrawDabs(const protocol::Message &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());

	brushes::drawBrushDabs(cmd, layers);

	if(_showallmarkers || cmd.contextId() != localId())
		emit userMarkerMove(cmd.contextId(), cmd.layer(), static_cast<const protocol::DrawDabs&>(cmd).lastPoint());
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	// This ends an indirect stroke. In incremental mode, this does nothing.
	 m_layerstack->editor(cmd.contextId()).mergeSublayers(cmd.contextId());
	emit userMarkerHide(cmd.contextId());
}

void StateTracker::handlePutImage(const protocol::PutImage &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("PutImage on non-existent layer #%d", cmd.layer());
		return;
	}

	const int expectedLen = cmd.width() * cmd.height() * 4;
	QByteArray data = qUncompress(cmd.image());
	if(data.length() != expectedLen) {
		qWarning() << "Invalid putImage: Expected" << expectedLen << "bytes, but got" << data.length();
		return;
	}
	QImage img(reinterpret_cast<const uchar*>(data.constData()), cmd.width(), cmd.height(), QImage::Format_ARGB32_Premultiplied);
	layer.putImage(cmd.x(), cmd.y(), img, paintcore::BlendMode::Mode(cmd.blendmode()));

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), layer->id(), QPoint(cmd.x() + cmd.width()/2, cmd.y()+cmd.height()/2));
}

void StateTracker::handlePutTile(const protocol::PutTile &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("PutTile on non-existent layer #%d", cmd.layer());
		return;
	}

	paintcore::Tile t;
	if(cmd.isSolidColor()) {
		t = paintcore::Tile(QColor::fromRgba(cmd.color()), cmd.contextId());

	} else {
		QByteArray data = qUncompress(cmd.image());
		if(data.length() != paintcore::Tile::BYTES) {
			qWarning() << "Invalid putTile: Expected" << paintcore::Tile::BYTES << "bytes, but got" << data.length();
			return;
		}

		t = paintcore::Tile(data, cmd.contextId());
	}

	layer.putTile(cmd.column(), cmd.row(), cmd.repeat(), t, cmd.sublayer());
}

void StateTracker::handleFillRect(const protocol::FillRect &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("FillRect on non-existent layer #%d", cmd.layer());
		return;
	}

	layer.fillRect(QRect(cmd.x(), cmd.y(), cmd.width(), cmd.height()), QColor::fromRgba(cmd.color()), paintcore::BlendMode::Mode(cmd.blend()));

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), layer->id(), QPoint(cmd.x() + cmd.width()/2, cmd.y()+cmd.height()/2));
}

void StateTracker::handleMoveRegion(const protocol::MoveRegion &cmd)
{
	auto layers = m_layerstack->editor(cmd.contextId());
	auto layer = layers.getEditableLayer(cmd.layer());
	if(layer.isNull()) {
		qWarning("MoveRegion on non-existent layer #%d", cmd.layer());
		return;
	}

	if(cmd.contextId() == m_myId) {
		// Moving the layer for real: make sure my preview is removed
		layer.removeSublayer(-1);
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
	if(targetArea > m_layerstack->width() * m_layerstack->height()) {
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
		mask = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);
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
		layer.fillRect(bounds, Qt::transparent, paintcore::BlendMode::MODE_REPLACE);
	} else {
		layer.putImage(bounds.x(), bounds.y(), mask, paintcore::BlendMode::MODE_ERASE);
	}

	layer.putImage(offset.x(), offset.y(), transformed, paintcore::BlendMode::MODE_NORMAL);

	if(_showallmarkers || cmd.contextId() != m_myId)
		emit userMarkerMove(cmd.contextId(), layer->id(), target.boundingRect().center());
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
			if(!m_localfork.isEmpty())
				i = qMin(i, m_localfork.offset() - 1);

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

		// Release all reset points older than one minute
		// TODO maybe make the time adjustable?
		QMutableListIterator<StateSavepoint> rpi(m_resetpoints);
		const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - (60 * 1000);
		while(rpi.hasNext()) {
			const StateSavepoint &sp = rpi.next();
			if(sp.timestamp() < cutoff)
				rpi.remove();
			else // Note: we assume reset points are in chronological order.
				break;
		}
	}

	// Clear out history older than the oldest savepoint
	Q_ASSERT(!m_savepoints.isEmpty());
	m_history.cleanup(m_savepoints.first()->streampointer);

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
	auto *data = new StateSavepoint::Data;
	data->timestamp = QDateTime::currentMSecsSinceEpoch();
	data->streampointer = pos<0 ? m_history.end() : pos;
	data->canvas = m_layerstack->makeSavepoint();
	data->layermodel = m_layerlist->getLayers();

	return StateSavepoint(data);
}

void StateTracker::makeSavepoint(int pos)
{
	// Don't make savepoints while a local fork exists, since
	// there will be stuff on the canvas that is not yet in
	// the mainline session history
	if(!m_localfork.isEmpty())
		return;

	// Check if sufficient time and actions has elapsed from previous savepoint
	if(!m_savepoints.isEmpty()) {
		static const qint64 MIN_INTERVAL_MS = 1000;
		static const int MIN_INTERVAL_MSGS = 100;

		const StateSavepoint sp = m_savepoints.last();
		const auto now = QDateTime::currentMSecsSinceEpoch();
		if(now - sp->timestamp < MIN_INTERVAL_MS && m_history.end() - sp->streampointer < MIN_INTERVAL_MSGS)
			return;
	}

	// Looks like a good spot for a savepoint
	const auto sp = createSavepoint(pos);
	m_savepoints << sp;
	m_resetpoints << sp;
}


void StateTracker::resetToSavepoint(const StateSavepoint savepoint)
{
	// This function is called when jumping to a recorded savepoint
	if(!savepoint) {
		qWarning("resetToSavepoint() was called with a null savepoint!");
		return;
	}

	m_history.resetTo(savepoint->streampointer);
	m_savepoints.clear();

	m_layerstack->editor(0).restoreSavepoint(savepoint->canvas);
	m_layerlist->setLayers(savepoint->layermodel);

	m_savepoints.append(savepoint);
}

void StateTracker::revertSavepointAndReplay(const StateSavepoint savepoint)
{
	// This function is called when reverting to an earlier state to undo
	// an action.
	if(!savepoint) {
		qWarning("revertSavepointAndReplay() was called with a null savepoint!");
		return;
	}
	if(!m_savepoints.contains(savepoint)) {
		qWarning("revertSavepointAndReplay() the given savepoint was not found!");
		return;
	}

	m_layerstack->editor(0).restoreSavepoint(savepoint->canvas);
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

	// Replay the local fork
	if(!m_localfork.isEmpty()) {
		Q_ASSERT(m_localfork.offset() >= savepoint->streampointer);
		m_localfork.setOffset(pos-1);
		const auto local = m_localfork.messages();
		for(const protocol::MessagePtr &msg : local) {
			if(msg->type() != protocol::MSG_UNDO && msg->type() != protocol::MSG_UNDOPOINT)
				handleCommand(msg, true, pos);
		}
	}
}

void StateTracker::handleTruncateHistory()
{
	int pos = m_history.end()-1;
	int upCount = 0;

	qWarning("Truncating undo history at %d", pos);
	while(m_history.isValidIndex(pos) && upCount <= protocol::UNDO_DEPTH_LIMIT) {
		protocol::MessagePtr msg = m_history.at(pos);

		if(msg->type() == protocol::MSG_UNDOPOINT) {
			++upCount;
			msg->setUndoState(protocol::GONE);
		}

		--pos;
	}
	qWarning("Marked %d UPs", upCount);
}

void StateTracker::handleAnnotationCreate(const protocol::AnnotationCreate &cmd)
{
	m_layerstack->annotations()->addAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
	if(cmd.contextId() == localId())
		emit myAnnotationCreated(cmd.id());
}

void StateTracker::handleAnnotationReshape(const protocol::AnnotationReshape &cmd)
{
	m_layerstack->annotations()->reshapeAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
}

void StateTracker::handleAnnotationEdit(const protocol::AnnotationEdit &cmd)
{
	m_layerstack->annotations()->changeAnnotation(
		cmd.id(),
		cmd.text(),
		cmd.flags() & protocol::AnnotationEdit::FLAG_PROTECT,
		cmd.flags() & (protocol::AnnotationEdit::FLAG_VALIGN_BOTTOM|protocol::AnnotationEdit::FLAG_VALIGN_CENTER),
		QColor::fromRgba(cmd.bg())
		);
}

void StateTracker::handleAnnotationDelete(const protocol::AnnotationDelete &cmd)
{
	m_layerstack->annotations()->deleteAnnotation(cmd.id());
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
	case MSG_LAYER_CREATE:
	case MSG_LAYER_ATTR:
	case MSG_LAYER_RETITLE:
		return AffectedArea(AffectedArea::LAYERATTRS, msg->layer());
	case MSG_LAYER_VISIBILITY: return AffectedArea(AffectedArea::USERATTRS, 0);

	case MSG_PUTIMAGE: {
		const PutImage &m = msg.cast<PutImage>();
		return AffectedArea(AffectedArea::PIXELS, m.layer(), QRect(m.x(), m.y(), m.width(), m.height()));
	}
	case MSG_PUTTILE: {
		const PutTile &m = msg.cast<PutTile>();
		return AffectedArea(AffectedArea::PIXELS, m.layer(), QRect(
			m.column() * paintcore::Tile::SIZE,
			m.row() * paintcore::Tile::SIZE,
			paintcore::Tile::SIZE, paintcore::Tile::SIZE));
	}

	case MSG_DRAWDABS_CLASSIC:
	case MSG_DRAWDABS_PIXEL:
	case MSG_DRAWDABS_PIXEL_SQUARE: {
		const DrawDabs &dd = msg.cast<DrawDabs>();

		// Indirect drawing mode: check bounds in PenUp
		if(dd.isIndirect())
			return AffectedArea(AffectedArea::USERATTRS, 0);

		return AffectedArea(AffectedArea::PIXELS, dd.layer(), dd.bounds());
	}
	case MSG_PEN_UP: {
		QPair<int,QRect> bounds = m_layerstack->findChangeBounds(msg->contextId());
		if(bounds.first)
			return AffectedArea(AffectedArea::PIXELS, bounds.first, bounds.second);
		else
			return AffectedArea(AffectedArea::USERATTRS, 0);
	}
	case MSG_FILLRECT: {
		const FillRect &fr = msg.cast<FillRect>();
		return AffectedArea(AffectedArea::PIXELS, fr.layer(), QRect(fr.x(), fr.y(), fr.width(), fr.height()));
	}

	case MSG_ANNOTATION_CREATE:
	case MSG_ANNOTATION_RESHAPE:
	case MSG_ANNOTATION_EDIT:
	case MSG_ANNOTATION_DELETE:
		return AffectedArea(AffectedArea::ANNOTATION, msg->layer());

	case MSG_REGION_MOVE: {
		const MoveRegion &mr = msg.cast<MoveRegion>();
		return AffectedArea(AffectedArea::PIXELS, mr.layer(), mr.sourceBounds().united(mr.targetBounds()));
	}

	case MSG_UNDOPOINT: return AffectedArea(AffectedArea::USERATTRS, 0);

	case MSG_CANVAS_BACKGROUND: return AffectedArea(AffectedArea::PIXELS, -1, QRect(0, 0, 1, 1));

	default:
#ifndef NDEBUG
		qWarning("%s: affects EVERYTHING", qPrintable(msg->messageName()));
#endif
		return AffectedArea(AffectedArea::EVERYTHING, 0);
	}
}

}
