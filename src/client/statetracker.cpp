/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#include <QDebug>
#include <QDateTime>

#include "statetracker.h"
#include "loader.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "net/layerlist.h"

#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

namespace drawingboard {

struct StateSavepoint {
	StateSavepoint() : timestamp(0), streampointer(-1), canvas(0) {}
	StateSavepoint(const StateSavepoint &) = delete;
	StateSavepoint &operator=(const StateSavepoint&) = delete;
	~StateSavepoint() { delete canvas; }

	qint64 timestamp;
	int streampointer;
	paintcore::Savepoint *canvas;
	QHash<int, DrawingContext> ctxstate;
	QVector<net::LayerListItem> layermodel;
};

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
		_showallmarkers(false)
{
}

StateTracker::~StateTracker()
{
	while(!_savepoints.isEmpty())
		delete _savepoints.takeLast();
}

void StateTracker::receiveCommand(protocol::MessagePtr msg)
{
	// Cleanup
	if(_msgstream_sizelimit>0 && _msgstream.lengthInBytes() > _msgstream_sizelimit) {
		uint oldlen = _msgstream.lengthInBytes();
		qDebug() << "Message stream history size limit reached at" << oldlen / float(1024*1024) << "Mb. Clearing..";
		_msgstream.hardCleanup(0, _msgstream.end());
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
			int savepoint=0;
			while(savepoint < _savepoints.count()) {
				if(_savepoints[savepoint]->streampointer > undopoint) {
					--savepoint;
					break;
				}
				++savepoint;
			}

			// Remove redundant save points
			Q_ASSERT(savepoint>=0);
			if(savepoint<0) {
				qWarning() << "no savepoint for undo point" << undopoint << "after cleanup!";
			} else {
				qDebug() << "removing" << savepoint << "redundant save points out of" << _savepoints.count();
				while(savepoint--)
					delete _savepoints.takeFirst();
			}
		}
	}

	// Add command to history and execute it
	_msgstream.append(msg);
	int pos = _msgstream.end() - 1;
	handleCommand(msg, false, pos);
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
	if(!_hassnapshot || forcenew) {
		// Generate snapshot
		QList<protocol::MessagePtr> snapshot = SnapshotLoader(this).loadInitCommands();

		// Replace old message stream with snapshot since it didn't contain one
		_msgstream.clear();
		foreach(protocol::MessagePtr ptr, snapshot)
			_msgstream.append(ptr);

		// Update size limit
		if(_msgstream_sizelimit>0 && _msgstream.lengthInBytes() > _msgstream_sizelimit)
			_msgstream_sizelimit = 2 * _msgstream.lengthInBytes();

		_hassnapshot = true;

		return snapshot;
	} else {
		// Message stream contains (starts with) a snapshot: return it
		return _msgstream.toList();
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
	_image->addLayer(cmd.id(), cmd.title(), QColor::fromRgba(cmd.fill()));
	_layerlist->createLayer(cmd.id(), cmd.title());
	if(cmd.contextId() == _myid)
		emit myLayerCreated(cmd.id());
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
	_image->reorderLayers(cmd.order());
	_layerlist->reorderLayers(cmd.order());
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
	paintcore::Brush &b = ctx.tool.brush;
	ctx.tool.layer_id = cmd.layer();
	b.setBlendingMode(cmd.blend());
	b.setSubpixel(cmd.mode() & protocol::TOOL_MODE_SUBPIXEL);
	b.setIncremental(cmd.mode() & protocol::TOOL_MODE_INCREMENTAL);
	b.setSpacing(cmd.spacing());
	b.setRadius(cmd.size_h());
	b.setRadius2(cmd.size_l());
	b.setHardness(cmd.hard_h() / 255.0);
	b.setHardness2(cmd.hard_l() / 255.0);
	b.setOpacity(cmd.opacity_h() / 255.0);
	b.setOpacity2(cmd.opacity_l() / 255.0);
	b.setColor(cmd.color_h());
	b.setColor2(cmd.color_l());

	emit userMarkerColor(cmd.contextId(), b.color1());
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

		if(ctx.pendown) {
			layer->drawLine(cmd.contextId(), ctx.tool.brush, ctx.lastpoint, p, ctx.distance_accumulator);
		} else {
			ctx.pendown = true;
			ctx.distance_accumulator = 0;
			layer->dab(cmd.contextId(), ctx.tool.brush, p);
		}
		ctx.lastpoint = p;
	}

	if(cmd.contextId() == _myid)
		emit myStrokesCommitted(cmd.points().size());

	if(_showallmarkers || cmd.contextId() != _myid)
		emit userMarkerMove(cmd.contextId(), ctx.lastpoint.x(), ctx.lastpoint.y(), 0);
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
	QByteArray data = qUncompress(cmd.image());
	QImage img(reinterpret_cast<const uchar*>(data.constData()), cmd.width(), cmd.height(), QImage::Format_ARGB32);
	layer->putImage(cmd.x(), cmd.y(), img, (cmd.flags() & protocol::PutImage::MODE_BLEND));
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
	}

	// Make a new savepoint (if possible)
	makeSavepoint(pos);
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
	const StateSavepoint *savepoint = 0;
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

	// Step 4. Revert to savepoint
	revertSavepoint(savepoint);

	// Step 5. Replay commands, excluding undone actions
	pos = savepoint->streampointer + 1;
	while(pos < _msgstream.end()) {
		if(_msgstream.at(pos)->undoState() == protocol::DONE) {
			handleCommand(_msgstream.at(pos), true, pos);
		}
		++pos;
	}
}

/**
 * @brief Check if it is possible to create a new savepoint now
 *
 * The following criteria are used:
 *
 * - All users must be in PEN_UP state
 * - Sufficient time must have elapsed since last savepoint
 * @return
 */
bool StateTracker::canMakeSavepoint(int pos) const
{
	// Make sure there is something in the message stream buffer
	if(_msgstream.end() <= _msgstream.offset())
		return false;

	// Check if sufficient time and actions has elapsed from previous savepoint
	if(!_savepoints.isEmpty()) {
		const StateSavepoint *sp = _savepoints.last();
		quint64 now = QDateTime::currentMSecsSinceEpoch();
		if(now - sp->timestamp < 1000 && _msgstream.end() - sp->streampointer < 100)
			return false;
	}

	// Ok, looks like we're good to go
	return true;
}

void StateTracker::makeSavepoint(int pos)
{
	if(canMakeSavepoint(pos)) {
		StateSavepoint *savepoint = new StateSavepoint;
		savepoint->timestamp = QDateTime::currentMSecsSinceEpoch();
		savepoint->streampointer = pos;
		savepoint->canvas = _image->makeSavepoint();
		savepoint->ctxstate = _contexts;
		savepoint->layermodel = _layerlist->getLayers();

		_savepoints.append(savepoint);
	}
}

void StateTracker::revertSavepoint(const StateSavepoint *savepoint)
{
	Q_ASSERT(_savepoints.contains(const_cast<StateSavepoint*>(savepoint)));

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;
	_layerlist->setLayers(savepoint->layermodel);

	// Reverting a savepoint destroys all newer savepoints
	while(_savepoints.last() != savepoint)
		delete _savepoints.takeLast();
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

}
