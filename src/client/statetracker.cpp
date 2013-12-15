/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include "canvasscene.h" // needed for annotations
#include "annotationitem.h"
#include "loader.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "net/client.h"
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
	dpcore::Savepoint *canvas;
	QHash<int, DrawingContext> ctxstate;
};

StateTracker::StateTracker(CanvasScene *scene, net::Client *client, QObject *parent)
	: QObject(parent),
	  _scene(scene),
	  _image(scene->layers()),
	  _layerlist(client->layerlist()),
	  _myid(client->myId()),
	  _hassnapshot(true),
	  _msgstream_sizelimit(1024 * 1024 * 10)
{
	connect(client, SIGNAL(layerVisibilityChange(int,bool)), _image, SLOT(setLayerHidden(int,bool)));
}

StateTracker::~StateTracker()
{
	while(!_savepoints.isEmpty())
		delete _savepoints.takeLast();
}

void StateTracker::receiveCommand(protocol::MessagePtr msg, bool replay)
{
	if(!replay) {
		if(_msgstream_sizelimit>0 && _msgstream.lengthInBytes() > _msgstream_sizelimit) {
			qDebug() << "Message stream history size limit reached at" << _msgstream.lengthInBytes() / float(1024*1024) << "Mb. Clearing..";
			_msgstream.hardCleanup(_msgstream_sizelimit);
			_hassnapshot = false;
		}
		_msgstream.append(msg);
	}

	switch(msg->type()) {
		using namespace protocol;
		case MSG_CANVAS_RESIZE:
			handleCanvasResize(msg.cast<CanvasResize>());
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
			handleUndoPoint(msg.cast<UndoPoint>());
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

QList<protocol::MessagePtr> StateTracker::generateSnapshot(bool forcenew)
{
	if(!_hassnapshot || forcenew) {
		// Generate snapshot
		QList<protocol::MessagePtr> snapshot = SnapshotLoader(_scene).loadInitCommands();

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

void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd)
{
	if(_image->width()>0) {
		// TODO support actual resizing
		qWarning() << "canvas resize is currently supported on session initialization only.";
	} else {
		_image->init(QSize(cmd.width(), cmd.height()));

		// Generate the initial savepoint, just in case
		makeSavepoint();
	}
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
	dpcore::Layer *layer = _image->getLayer(cmd.id());
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
	dpcore::Layer *layer = _image->getLayer(cmd.id());
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
	dpcore::Brush &b = ctx.tool.brush;
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
}

void StateTracker::handlePenMove(const protocol::PenMove &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	dpcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	if(!layer) {
		qWarning() << "penMove by user" << cmd.contextId() << "on non-existent layer" << ctx.tool.layer_id;
		return;
	}
	
	dpcore::Point p;
	foreach(const protocol::PenPoint pp, cmd.points()) {
		// The coordinate encoding code is in net/client.cpp
		p = dpcore::Point(
			(pp.x >> 2) - 128,
			(pp.y >> 2) - 128,
			(pp.x & 3) / 4.0,
			(pp.y & 3) / 4.0,
			pp.p/255.0
		);

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
		_scene->takePreview(cmd.points().size());
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
	dpcore::Layer *layer = _image->getLayer(ctx.tool.layer_id);
	if(!layer) {
		qWarning() << "penUp by user" << cmd.contextId() << "on non-existent layer" << ctx.tool.layer_id;
		return;
	}

	// This ends an indirect stroke. In incremental mode, this does nothing.
	layer->mergeSublayer(cmd.contextId());

	ctx.pendown = false;
}

void StateTracker::handlePutImage(const protocol::PutImage &cmd)
{
	dpcore::Layer *layer = _image->getLayer(cmd.layer());
	if(!layer) {
		qWarning() << "putImage on non-existent layer" << cmd.layer();
		return;
	}
	QByteArray data = qUncompress(cmd.image());
	QImage img(reinterpret_cast<const uchar*>(data.constData()), cmd.width(), cmd.height(), QImage::Format_ARGB32);
	layer->putImage(cmd.x(), cmd.y(), img, (cmd.flags() & protocol::PutImage::MODE_BLEND));
}

void StateTracker::handleUndoPoint(const protocol::UndoPoint &cmd)
{
	Q_UNUSED(cmd);
	makeSavepoint();
}

void StateTracker::handleUndo(protocol::Undo &cmd)
{
	if(cmd.points()==0) {
		qWarning() << "zero undo from user" << cmd.contextId();
		return;
	}

	const uint8_t ctxid = cmd.effectiveId();
	int actions = qAbs(cmd.points());
	bool undo = cmd.points()>0;

	// Step 1. Find undo point
	int pos = _msgstream.end();
	while(actions>0 && _msgstream.isValidIndex(--pos)) {
		protocol::MessagePtr msg = _msgstream.at(pos);
		if(msg->type() == protocol::MSG_UNDOPOINT) {
			const protocol::UndoPoint &up = msg.cast<protocol::UndoPoint>();
			if(undo) {
				if(up.contextId() == ctxid && !up.isUndone())
					--actions;
			} else {
				// Redo stops at first not-undone action
				if(up.contextId() == ctxid) {
					if(up.isUndone())
						--actions;
					else {
						qWarning() << "reached non-undone point by user" << cmd.contextId() << "but we were still looking for another" << actions;
						if(actions == -cmd.points()) {
							qWarning() << "...nothing to redo";
							return;
						}
						actions = 0;
					}

				}
			}
		}
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
	for(int i=pos;i<_msgstream.end();++i) {
		if(_msgstream.at(i)->contextId() == ctxid) {
			// Don't unmark UNDOs, otherwise we end up in an infinite loop
			if(undo || _msgstream.at(i)->type() != protocol::MSG_UNDO)
				_msgstream.at(i)->setUndone(undo);
		}
	}

	// The redo action must be marked as undone to avoid looping
	if(!undo)
		cmd.setUndone(true);

	// Step 4. Revert to savepoint
	revertSavepoint(savepoint);

	qDebug() << "reverted to" << savepoint->streampointer + 1 << "of" << _msgstream.end();
	// Step 5. Replay commands, excluding undone actions
	pos = savepoint->streampointer + 1;
	while(pos < _msgstream.end()) {
		if(!_msgstream.at(pos)->isUndone())
			receiveCommand(_msgstream.at(pos), true);
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
bool StateTracker::canMakeSavepoint() const
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

	// Check if all users are in PEN_UP state
	// (this is not strictly necessary, but it makes for neater savepoints)
	foreach(const DrawingContext &ctx, _contexts)
		if(ctx.pendown)
			return false;

	// Ok, looks like we're good to go
	return true;
}

void StateTracker::makeSavepoint()
{
	if(canMakeSavepoint()) {
		qDebug() << "making savepoint";
		StateSavepoint *savepoint = new StateSavepoint;
		savepoint->timestamp = QDateTime::currentMSecsSinceEpoch();
		savepoint->streampointer = _msgstream.end() - 1;
		savepoint->canvas = _image->makeSavepoint();
		savepoint->ctxstate = _contexts;

		_savepoints.append(savepoint);
	}
}

void StateTracker::revertSavepoint(const StateSavepoint *savepoint)
{
	Q_ASSERT(_savepoints.contains(const_cast<StateSavepoint*>(savepoint)));

	_image->restoreSavepoint(savepoint->canvas);
	_contexts = savepoint->ctxstate;

	// Reverting a savepoint destroys all newer savepoints
	while(_savepoints.last() != savepoint)
		delete _savepoints.takeLast();
}

void StateTracker::handleAnnotationCreate(const protocol::AnnotationCreate &cmd)
{
	AnnotationItem *item = new AnnotationItem(cmd.id());
	item->setShowBorder(_scene->showAnnotationBorders());
	item->setGeometry(QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));

	_scene->addItem(item);
	if(cmd.contextId() == _myid)
		emit myAnnotationCreated(item);
}

void StateTracker::handleAnnotationReshape(const protocol::AnnotationReshape &cmd)
{
	AnnotationItem *item = _scene->getAnnotationById(cmd.id());
	if(!item) {
		qWarning() << "Got annotation reshape for non-existent annotation" << cmd.id();
		return;
	}

	item->setGeometry(QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
}

void StateTracker::handleAnnotationEdit(const protocol::AnnotationEdit &cmd)
{
	AnnotationItem *item = _scene->getAnnotationById(cmd.id());
	if(!item) {
		qWarning() << "Got annotation edit for non-existent annotation" << cmd.id();
		return;
	}

	item->setBackgroundColor(QColor::fromRgba(cmd.bg()));
	item->setText(cmd.text());
}

void StateTracker::handleAnnotationDelete(const protocol::AnnotationDelete &cmd)
{
	if(!_scene->deleteAnnotation(cmd.id()))
		qWarning() << "Got annotation delete for non-existent annotation" << cmd.id();
}

}
