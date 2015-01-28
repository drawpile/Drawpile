/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#include "loader.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "net/layerlist.h"
#include "net/utils.h"

#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

#include <QDebug>
#include <QDateTime>
#include <QTimer>

namespace drawingboard {

struct StateSavepoint::Data {
	Data() : timestamp(0), streampointer(-1), canvas(0), _refcount(1) {}
	Data(const Data &) = delete;
	Data &operator=(const Data&) = delete;
	~Data() { delete canvas; }

	qint64 timestamp;
	int streampointer;
	paintcore::Savepoint *canvas;
	QHash<int, DrawingContext> ctxstate;
	QVector<net::LayerListItem> layermodel;

private:
	int _refcount;
	friend class StateSavepoint;
};

StateSavepoint::StateSavepoint(const StateSavepoint &sp)
	: _data(sp._data)
{
	if(_data)
		++_data->_refcount;
}

StateSavepoint &StateSavepoint::operator =(const StateSavepoint &sp)
{
	if(_data) {
		if(sp._data != _data) {
			Q_ASSERT(_data->_refcount>0);
			if(--_data->_refcount == 0)
				delete _data;
			_data = sp._data;
			++_data->_refcount;
		}
	} else {
		_data = sp._data;
		++_data->_refcount;
	}
	return *this;
}

StateSavepoint::~StateSavepoint()
{
	if(_data) {
		Q_ASSERT(_data->_refcount>0);
		if(--_data->_refcount == 0)
			delete _data;
	}
}

StateSavepoint::Data *StateSavepoint::operator ->() {
	if(!_data)
		_data = new StateSavepoint::Data;
	return _data;
}

void ToolContext::updateFromToolchange(const protocol::ToolChange &cmd)
{
	layer_id = cmd.layer();
	brush.setBlendingMode(cmd.blend());
	brush.setSubpixel(cmd.mode() & protocol::TOOL_MODE_SUBPIXEL);
	brush.setIncremental(cmd.mode() & protocol::TOOL_MODE_INCREMENTAL);
	brush.setSpacing(cmd.spacing());
	brush.setSize(qMax(1, (int)cmd.size_h()));
	brush.setSize2(qMax(1, (int)cmd.size_l()));
	brush.setHardness(cmd.hard_h() / 255.0);
	brush.setHardness2(cmd.hard_l() / 255.0);
	brush.setOpacity(cmd.opacity_h() / 255.0);
	brush.setOpacity2(cmd.opacity_l() / 255.0);
	brush.setColor(cmd.color_h());
	brush.setColor2(cmd.color_l());
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
StateTracker::StateTracker(paintcore::LayerStack *image, net::LayerListModel *layerlist, int myId, QObject *parent)
	: QObject(parent),
		_image(image),
		_layerlist(layerlist),
		_myid(myId),
		_msgstream_sizelimit(1024 * 1024 * 10),
		_hassnapshot(true),
		_showallmarkers(false),
		_hasParticipated(false)
{
	connect(_layerlist, SIGNAL(layerOpacityPreview(int,float)), this, SLOT(previewLayerOpacity(int,float)));

	// Timer for periodically resetting the local fork to keep cruft from accumulating.
	// This is to make sure an out-of-sync fork gets cleaned up even if the user doesn't
	// draw anything in a while.
	_localforkCleanupTimer = new QTimer(this);
	_localforkCleanupTimer->setSingleShot(true);
	connect(_localforkCleanupTimer, &QTimer::timeout, this, &StateTracker::resetLocalFork);
}

StateTracker::~StateTracker()
{
}

void StateTracker::localCommand(protocol::MessagePtr msg)
{
	// A fork is created at the end of the mainline history
	if(_localfork.isEmpty()) {
		_localfork.setOffset(_msgstream.end()-1);

		// Since the presence of a local fork blocks savepoint creation,
		// now is a good time to try to create one.
		if(msg->type() == protocol::MSG_UNDOPOINT)
			makeSavepoint(_msgstream.end()-1);
	}

	_localfork.addLocalMessage(msg, affectedArea(msg));

	// for the future: handle undo messages in the local fork too
	if(msg->type() != protocol::MSG_UNDO && msg->type() != protocol::MSG_UNDOPOINT) {
		int pos = _msgstream.end() - 1;
		handleCommand(msg, false, pos);
	}

	_localforkCleanupTimer->start(60 * 1000);
}

void StateTracker::receiveCommand(protocol::MessagePtr msg)
{
	// Cleanup
	if(_msgstream_sizelimit>0 && _msgstream.lengthInBytes() > _msgstream_sizelimit) {
		uint oldlen = _msgstream.lengthInBytes();
		qDebug() << "Message stream history size limit reached at" << oldlen / float(1024*1024) << "Mb. Clearing..";
		_msgstream.hardCleanup(0, _localfork.isEmpty() ? _msgstream.end() : _localfork.offset());
		qDebug() << "Released" << (oldlen-_msgstream.lengthInBytes()) / float(1024*1024) << "Mb.";
		_hassnapshot = false;

		// Clear out old savepoints
		// First, find the oldest undo point in the stream
		int undopoint = _msgstream.offset();
		while(undopoint<_msgstream.end()) {
			if(_msgstream.at(undopoint)->type() == protocol::MSG_UNDOPOINT)
				break;
			++undopoint;
		}

		if(undopoint == _msgstream.end()) {
			qWarning() << "no undo point found after cleaning history!";
		} else {
			// Find the newest savepoint older or same age as the undo point

			// If a local fork exists, we need a savepoint that precedes it in case we need to roll back.
			if(!_localfork.isEmpty())
				undopoint = qMin(undopoint, _localfork.offset());

			int savepoint=0;
			while(savepoint < _savepoints.count()) {
				if(_savepoints[savepoint]->streampointer >= undopoint) {
					--savepoint;
					break;
				}
				++savepoint;
			}

			// Remove redundant save points
			qDebug() << "removing" << savepoint << "redundant save points out of" << _savepoints.count();
			while(savepoint-- > 0)
				_savepoints.removeFirst();
		}
	}

	// Add command to history and execute it
	_msgstream.append(msg);

	LocalFork::MessageAction lfa = _localfork.handleReceivedMessage(msg, affectedArea(msg));

	// Undo messages are not handled locally (at the moment)
	if(lfa == LocalFork::ALREADYDONE && (msg->type()==protocol::MSG_UNDO || msg->type()==protocol::MSG_UNDOPOINT))
		lfa = LocalFork::CONCURRENT;

	if(lfa==LocalFork::ROLLBACK) {
		// Uh oh! An inconsistency was detected: roll back the history and replay

		// first, find the newest savepoint that precedes the fork
		int savepoint = _savepoints.size()-1;
		while(savepoint>0) {
			if(_savepoints.at(savepoint)->streampointer <= _localfork.offset())
				break;
			--savepoint;
		}

		if(savepoint<0) {
			// should never happen
			qFatal("No savepoint for rolling back local fork at %d!", _localfork.offset());
		} else {
			const StateSavepoint &sp = _savepoints.at(savepoint);
			qDebug("inconsistency at %d (local fork at %d). Rolling back to %d", _msgstream.end(), _localfork.offset(), sp->streampointer);

			revertSavepointAndReplay(sp);
		}

	} else if(lfa==LocalFork::CONCURRENT) {
		// Concurrent operation: safe to execute
		int pos = _msgstream.end() - 1;
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
		default:
			qWarning() << "Unhandled drawing command" << msg->type();
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
		_msgstream.append(m);

	// End drawing contexts
	QHashIterator<int, DrawingContext> iter(_contexts);
	while(iter.hasNext()) {
		iter.next();
		if(iter.key() != _myid) {
			// Simulate pen-up
			if(iter.value().pendown)
				receiveCommand(protocol::MessagePtr(new protocol::PenUp(iter.key())));
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
			receiveCommand(protocol::MessagePtr(new protocol::PenUp(iter.key())));
	}
}

QList<protocol::MessagePtr> StateTracker::generateSnapshot(bool forcenew)
{
	QList<protocol::MessagePtr> snapshot;

	if(!_hassnapshot || forcenew) {
		// Generate snapshot
		snapshot = SnapshotLoader(this).loadInitCommands();

		// Replace old message stream with snapshot since it didn't contain one
		_msgstream.resetTo(0);
		foreach(protocol::MessagePtr ptr, snapshot)
			_msgstream.append(ptr);

		// Update size limit
		if(_msgstream_sizelimit>0 && _msgstream.lengthInBytes() > _msgstream_sizelimit)
			_msgstream_sizelimit = 2 * _msgstream.lengthInBytes();

		_hassnapshot = true;

	} else {
		// Message stream contains (starts with) a snapshot: use it
		snapshot = _msgstream.toList();

		// Add layer ACL status
		// This is for the initial session snapshot. For new snapshots the
		// server will add the correct layer ACLs.
		for(const net::LayerListItem &layer : _layerlist->getLayers()) {
			if(layer.isLockedFor(_myid))
				snapshot << protocol::MessagePtr(new protocol::LayerACL(_myid, layer.id, true, QList<uint8_t>()));
		}
	}

	return snapshot;
}

void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd, int pos)
{
	_image->resize(cmd.top(), cmd.right(), cmd.bottom(), cmd.left());

	// Generate the initial savepoint, just in case
	makeSavepoint(pos);
}

void StateTracker::handleLayerCreate(const protocol::LayerCreate &cmd)
{
	if(_image->addLayer(cmd.id(), cmd.title(), QColor::fromRgba(cmd.fill()))) {
		_layerlist->createLayer(cmd.id(), cmd.title());

		if(cmd.contextId() == _myid || !_hasParticipated)
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
	layer->setBlend(cmd.blend());
	_layerlist->changeLayer(cmd.id(), cmd.opacity() / 255.0, cmd.blend());
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
	_layerlist->retitleLayer(cmd.id(), cmd.title());
}

void StateTracker::handleLayerOrder(const protocol::LayerOrder &cmd)
{
	QList<uint16_t> currentOrder;
	for(int i=0;i<_image->layers();++i)
		currentOrder.append(_image->getLayerByIndex(i)->id());

	QList<uint16_t> newOrder = cmd.sanitizedOrder(currentOrder);

	if(newOrder != cmd.order()) {
		qWarning() << "invalid layer reorder!";
		qWarning() << "current order is:" << currentOrder;
		qWarning() << "  the new one was:" << cmd.order();
		qWarning() << "  fixed order is:" << newOrder;
	}

	_image->reorderLayers(newOrder);
	_layerlist->reorderLayers(newOrder);
}

void StateTracker::handleLayerDelete(const protocol::LayerDelete &cmd)
{
	if(cmd.merge())
		_image->mergeLayerDown(cmd.id());
	_image->deleteLayer(cmd.id());
	_layerlist->deleteLayer(cmd.id());
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

	emit userMarkerAttribs(cmd.contextId(), ctx.tool.brush.color1(), layername);
}

void StateTracker::handlePenMove(const protocol::PenMove &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	paintcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	if(!layer) {
		qWarning() << "penMove by user" << cmd.contextId() << "on non-existent layer" << ctx.tool.layer_id;
		return;
	}
	
	foreach(const protocol::PenPoint &pp, cmd.points()) {
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

	if(_showallmarkers || cmd.contextId() != _myid)
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
	layer->putImage(cmd.x(), cmd.y(), img, cmd.mode());
}

void StateTracker::handleFillRect(const protocol::FillRect &cmd)
{
	paintcore::Layer *layer = _image->getLayer(cmd.layer());
	if(!layer) {
		qWarning() << "fillRect on non-existent layer" << cmd.layer();
		return;
	}

	layer->fillRect(QRect(cmd.x(), cmd.y(), cmd.width(), cmd.height()), QColor::fromRgba(cmd.color()), cmd.blend());
}

void StateTracker::handleUndoPoint(const protocol::UndoPoint &cmd, bool replay, int pos)
{
	// New undo point. This branches the undo history. Since we store the
	// commands in a linear sequence, this branching is represented by marking
	// the unreachable commands as GONE.
	if(!replay) {
		int i = pos - 1; // skip the one just added
		while(_msgstream.isValidIndex(i)) {
			protocol::MessagePtr msg = _msgstream.at(i);
			if(msg->contextId() == cmd.contextId()) {
				// optimization: we can stop searching after finding the first GONE command
				if(msg->type() != protocol::MSG_UNDO && msg->undoState() == protocol::GONE)
					break;
				else if(msg->undoState() == protocol::UNDONE)
					msg->setUndoState(protocol::GONE);
			}
			--i;
		}

		// Release state snapshots older than the oldest allowed undopoint
		i = pos - 1;
		int upcount = 0;
		while(_msgstream.isValidIndex(i)) {
			if(_msgstream.at(i)->type() == protocol::MSG_UNDOPOINT) {
				++upcount;
				if(upcount>protocol::UNDO_HISTORY_LIMIT)
					break;
			}
			--i;
		}

		if(upcount>protocol::UNDO_HISTORY_LIMIT) {
			if(!_localfork.isEmpty())
				i = qMin(i, _localfork.offset() - 1);

			QMutableListIterator<StateSavepoint> spi(_savepoints);
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
	if(cmd.contextId() == _myid)
		_hasParticipated = true;
}

void StateTracker::handleUndo(protocol::Undo &cmd)
{
	// Undo/redo commands are never replayed, so start
	// by marking it as unavailable.
	cmd.setUndoState(protocol::GONE);

	if(cmd.points()==0) {
		qWarning() << "zero undo from user" << cmd.contextId();
		return;
	}

	const uint8_t ctxid = cmd.contextId();
	const bool undo = cmd.points()>0;
	int actions = qAbs(cmd.points());

	// Step 1. Find undo or redo point
	int pos = _msgstream.end();
	if(undo) {
		// Search for undoable actions from the end of the
		// command stream towards the beginning
		while(actions>0 && _msgstream.isValidIndex(--pos)) {
			protocol::MessagePtr msg = _msgstream.at(pos);
			if(msg->type() == protocol::MSG_UNDOPOINT && msg->contextId() == ctxid) {
				if(msg->undoState() == protocol::DONE)
					--actions;
			}
		}
	} else {
		// Find the start of the undo sequence
		int redostart = pos;
		while(_msgstream.isValidIndex(--pos)) {
			protocol::MessagePtr msg = _msgstream.at(pos);
			if(msg->type() == protocol::MSG_UNDOPOINT && msg->contextId() == ctxid) {
				if(msg->undoState() != protocol::DONE)
					redostart = pos;
				else
					break;
			}
		}

		if(redostart == _msgstream.end()) {
			qWarning() << "nothing to redo for user" << cmd.contextId();
			return;
		}
		pos = redostart;
	}

	if(!_msgstream.isValidIndex(pos)) {
		// Normally the server should enforce undo limits to prevent
		// this from happening
		qWarning() << "Cannot undo action by user" << ctxid << ": not enough messages in buffer!";
		return;
	}

	// Step 2. Find nearest save point
	StateSavepoint savepoint;
	for(int i=_savepoints.count()-1;i>=0;--i) {
		if(_savepoints.at(i)->streampointer <= pos) {
			savepoint = _savepoints.at(i);
			break;
		}
	}

	if(!savepoint) {
		qWarning() << "Cannot undo action by user" << ctxid << ": no savepoint found!";
		return;
	}

	// Step 3. (Un)mark all actions by the user as undone
	if(undo) {
		for(int i=pos;i<_msgstream.end();++i) {
			protocol::MessagePtr msg = _msgstream.at(i);
			if(msg->contextId() == ctxid)
				msg->setUndoState(protocol::MessageUndoState(protocol::UNDONE | msg->undoState()));
		}
	} else {
		int i=pos;
		++actions;
		while(i<_msgstream.end()) {
			protocol::MessagePtr msg = _msgstream.at(i);
			if(msg->contextId() == ctxid) {
				if(msg->type() == protocol::MSG_UNDOPOINT && msg->undoState() != protocol::GONE)
					if(--actions==0)
						break;

				// GONE messages cannot be redone
				if(msg->undoState() == protocol::UNDONE)
					msg->setUndoState(protocol::DONE);
			}
			++i;
		}
	}

	// Step 4. Revert to savepoint and replay with undone commands removed
	revertSavepointAndReplay(savepoint);
}

StateSavepoint StateTracker::createSavepoint(int pos)
{
	StateSavepoint savepoint;
	savepoint->timestamp = QDateTime::currentMSecsSinceEpoch();
	savepoint->streampointer = pos<0 ? _msgstream.end() : pos;
	savepoint->canvas = _image->makeSavepoint();
	savepoint->ctxstate = _contexts;
	savepoint->layermodel = _layerlist->getLayers();

	return savepoint;
}

void StateTracker::makeSavepoint(int pos)
{
	// Make sure there is something in the message stream buffer
	if(_msgstream.end() <= _msgstream.offset())
		return;

	// Don't make savepoints while a local fork exists, since
	// there will be stuff on the canvas that is not yet in
	// the mainline session history
	if(!_localfork.isEmpty())
		return;

	// Check if sufficient time and actions has elapsed from previous savepoint
	if(!_savepoints.isEmpty()) {
		const StateSavepoint sp = _savepoints.last();
		quint64 now = QDateTime::currentMSecsSinceEpoch();
		if(now - sp->timestamp < 1000 && _msgstream.end() - sp->streampointer < 100)
			return;
	}

	// Looks like a good spot for a savepoint
	_savepoints.append(createSavepoint(pos));
}


void StateTracker::resetToSavepoint(const StateSavepoint savepoint)
{
	// This function is called when jumping to a recorded savepoint

	_msgstream.resetTo(savepoint->streampointer);
	_savepoints.clear();

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;
	_layerlist->setLayers(savepoint->layermodel);

	_savepoints.append(savepoint);
}

void StateTracker::revertSavepointAndReplay(const StateSavepoint savepoint)
{
	// This function is called when reverting to an earlier state to undo
	// an action.

	Q_ASSERT(_savepoints.contains(savepoint));

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;
	_layerlist->setLayers(savepoint->layermodel);

	// Reverting a savepoint destroys all newer savepoints
	while(_savepoints.last() != savepoint)
		_savepoints.removeLast();

	// Replay all not-undo actions (and local fork)
	int pos = savepoint->streampointer + 1;
	while(pos < _msgstream.end()) {
		if(_msgstream.at(pos)->undoState() == protocol::DONE) {
			handleCommand(_msgstream.at(pos), true, pos);
		}
		++pos;
	}

	if(!_localfork.isEmpty()) {
		Q_ASSERT(_localfork.offset() >= savepoint->streampointer);
		_localfork.setOffset(pos-1);
		const QList<protocol::MessagePtr> local = _localfork.messages();
		for(const protocol::MessagePtr &msg : local) {
			if(msg->type() != protocol::MSG_UNDO && msg->type() != protocol::MSG_UNDOPOINT)
				handleCommand(msg, true, pos);
		}

	}
}

void StateTracker::handleAnnotationCreate(const protocol::AnnotationCreate &cmd)
{
	_image->addAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
	if(cmd.contextId() == _myid)
		emit myAnnotationCreated(cmd.id());
}

void StateTracker::handleAnnotationReshape(const protocol::AnnotationReshape &cmd)
{
	_image->reshapeAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
}

void StateTracker::handleAnnotationEdit(const protocol::AnnotationEdit &cmd)
{
	_image->changeAnnotation(cmd.id(), cmd.text(), QColor::fromRgba(cmd.bg()));
}

void StateTracker::handleAnnotationDelete(const protocol::AnnotationDelete &cmd)
{
	_image->deleteAnnotation(cmd.id());
}

void StateSavepoint::toDatastream(QDataStream &out) const
{
	Q_ASSERT(_data);
	const auto *d = _data;

	// Write stream pointer
	out << quint32(d->streampointer);

	// Write drawing contexts
	out << quint8(d->ctxstate.size());
	foreach(quint8 ctxid, d->ctxstate.keys()) {
		const DrawingContext &ctx = d->ctxstate[ctxid];

		// write context ID
		out << ctxid;

		// write tool context
		protocol::MessagePtr tc = net::brushToToolChange(ctxid, ctx.tool.layer_id, ctx.tool.brush);
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
	foreach(const net::LayerListItem &layer, d->layermodel) {
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
	sp._data = new StateSavepoint::Data;
	auto *d = sp._data;

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

		protocol::Message *tc = protocol::Message::deserialize((const uchar*)msgbuf, msglen);
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

		sp->layermodel.append(net::LayerListItem {
			layerid,
			title,
			opacity,
			blend,
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
	for(const net::LayerListItem &l : _layerlist->getLayers()) {
		if(l.id == id)
			return l.isLockedFor(_myid);
	}

	qWarning("isLayerLocked(%d): no such layer!", id);
	return false;
}

void StateTracker::resetLocalFork()
{
	if(!_localfork.isEmpty()) {
		int savepoint = _savepoints.size()-1;
		while(savepoint>0) {
			if(_savepoints.at(savepoint)->streampointer <= _localfork.offset())
				break;
			--savepoint;
		}

		if(savepoint<0) {
			// should never happen
			qFatal("No savepoint for rolling back local fork at %d!", _localfork.offset());
		} else {
			const StateSavepoint &sp = _savepoints.at(savepoint);
			qDebug("Resetting local fork and rolling back %d commands", _msgstream.end() - sp->streampointer);

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
