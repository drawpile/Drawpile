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

#include "statetracker.h"
#include "canvasscene.h" // needed for annotations
#include "annotationitem.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "widgets/layerlistwidget.h"

#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"

namespace drawingboard {

StateTracker::StateTracker(int myid, CanvasScene *scene, dpcore::LayerStack *image, widgets::LayerListWidget *layerlist, QObject *parent)
	: QObject(parent), _scene(scene), _image(image), _layerlist(layerlist), _myid(myid)
{
	_layerlist->init();
	connect(_layerlist, SIGNAL(layerSetHidden(int,bool)), _image, SLOT(setLayerHidden(int,bool)));
}
	
void StateTracker::receiveCommand(protocol::MessagePtr msg)
{
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
	_msgstream.append(msg);
}

QList<protocol::MessagePtr> StateTracker::generateSnapshot()
{
	// New snapshot point generation
	return _msgstream.toList();
}

void StateTracker::handleCanvasResize(const protocol::CanvasResize &cmd)
{
	if(_image->width()>0) {
		// TODO support actual resizing
		qWarning() << "canvas resize is currently supported on session initialization only.";
	} else {
		_image->init(QSize(cmd.width(), cmd.height()));
	}
}

void StateTracker::handleLayerCreate(const protocol::LayerCreate &cmd)
{
	_image->addLayer(cmd.id(), cmd.title(), QColor::fromRgba(cmd.fill()));
	qDebug() << "added layer" << cmd.id();
	_layerlist->addLayer(cmd.id(), cmd.title());
}

void StateTracker::handleLayerAttributes(const protocol::LayerAttributes &cmd)
{
	dpcore::Layer *layer = _image->getLayer(cmd.id());
	if(!layer) {
		qWarning() << "received layer attributes for non-existent layer" << cmd.id();
		return;
	}
	
	layer->setOpacity(cmd.opacity());
	layer->setTitle(cmd.title());
	_layerlist->changeLayer(cmd.id(), cmd.opacity() / 255.0, cmd.title());
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
			layer->drawLine(ctx.tool.brush, ctx.lastpoint, p, ctx.distance_accumulator);
		} else {
			ctx.pendown = true;
			ctx.distance_accumulator = 0;
			layer->dab(ctx.tool.brush, p);
		}
		ctx.lastpoint = p;
	}
	if(cmd.contextId() == _myid)
		_scene->takePreview(cmd.points().size());
}

void StateTracker::handlePenUp(const protocol::PenUp &cmd)
{
	DrawingContext &ctx = _contexts[cmd.contextId()];
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

void StateTracker::handleAnnotationCreate(const protocol::AnnotationCreate &cmd)
{
	AnnotationItem *item = new AnnotationItem(cmd.id());
	item->setShowBorder(_scene->showAnnotationBorders());
	item->setGeometry(QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));

	_scene->addItem(item);
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
	AnnotationItem *item = _scene->getAnnotationById(cmd.id());
	if(!item) {
		qWarning() << "Got annotation delete for non-existent annotation" << cmd.id();
		return;
	}

	delete item;
}

}
