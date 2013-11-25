/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#include <QTimer>

#include "canvasscene.h"
#include "canvasitem.h"
#include "statetracker.h"

#include "annotationitem.h"

#include "widgets/layerlistwidget.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "ora/orawriter.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent, widgets::LayerListWidget *layerlistwidget)
	: QGraphicsScene(parent), _image(0), _statetracker(0), _toolpreview(0), _showAnnotationBorders(false),
	_layerlistwidget(layerlistwidget)
{
	setItemIndexMethod(NoIndex);
	_previewClearTimer = new QTimer(this);
	connect(_previewClearTimer, SIGNAL(timeout()), this, SLOT(clearPreviews()));
}

CanvasScene::~CanvasScene()
{
	delete _image;
	delete _statetracker;
}

/**
 * This prepares the canvas for new drawing commands.
 * @param myid the context id of the local user
 */
void CanvasScene::initCanvas(int myid)
{
	delete _image;
	delete _statetracker;
	_image = new CanvasItem();
	_statetracker = new StateTracker(myid, this, _image->image(), _layerlistwidget);
	
	addItem(_image);
	clearAnnotations();

	foreach(QGraphicsLineItem *p, _previewstrokes)
		delete p;
	_previewstrokes.clear();
	foreach(QGraphicsLineItem *p, _previewstrokecache)
		delete p;
	_previewstrokecache.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
}

void CanvasScene::clearAnnotations()
{
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type) {
			emit annotationDeleted(static_cast<AnnotationItem*>(item)->id());
			delete item;
		}
	}
}

void CanvasScene::showAnnotations(bool show)
{
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type)
			item->setVisible(show);
	}
}

void CanvasScene::showAnnotationBorders(bool hl)
{
	_showAnnotationBorders = hl;
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type)
			static_cast<AnnotationItem*>(item)->setShowBorder(hl);
	}
}

void CanvasScene::unHilightAnnotation(int id)
{
	AnnotationItem *i = getAnnotationById(id);
	if(i)
		i->setHighlight(false);
}

bool CanvasScene::hasAnnotations() const
{
	foreach(const QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type)
			return true;
	}
	return false;
}

AnnotationItem *CanvasScene::annotationAt(const QPoint &point)
{
	foreach(QGraphicsItem *i, items(point)) {
		if(i->type() == AnnotationItem::Type)
			return static_cast<AnnotationItem*>(i);
	}
	return 0;
}

AnnotationItem *CanvasScene::getAnnotationById(int id)
{
	foreach(QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *item = static_cast<AnnotationItem*>(i);
			if(item->id() == id)
				return item;
		}
	}
	return 0;
}

bool CanvasScene::deleteAnnotation(int id)
{
	AnnotationItem *a = getAnnotationById(id);
	if(a) {
		emit annotationDeleted(id);
		delete a;
		return true;
	}
	return false;
}

/**
 * @return board contents
 */
QImage CanvasScene::image() const
{
	if(_image)
		return _image->image()->toFlatImage();
	else
		return QImage();
}

void CanvasScene::pickColor(int x, int y)
{
	if(_image) {
		QColor color = _image->image()->colorAt(x, y);
		if(color.isValid())
			emit colorPicked(color);
	}
}


/**
 * The file format is determined from the name of the file
 * @param file file path
 * @return true on succcess
 */
bool CanvasScene::save(const QString& file) const
{
	if(file.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact.
		QList<AnnotationItem*> annotations;
		foreach(QGraphicsItem *i, items()) {
			if(i->type() == AnnotationItem::Type)
				annotations.append(static_cast<AnnotationItem*>(i));
		}

		return openraster::saveOpenRaster(file, _image->image(), annotations);
	} else {
		// Regular image formats: flatten the image first.
		return image().save(file);
	}
}

/**
 * An image cannot be saved as a regular PNG without loss of information
 * if
 * <ul>
 * <li>It has more than one layer</li>
 * <li>It has annotations</li>
 * </ul>
 */
bool CanvasScene::needSaveOra() const
{
	return _image->image()->layers() > 1 ||
		hasAnnotations();
}

/**
 * @retval true if the board contains an image
 */
bool CanvasScene::hasImage() const {
	return _image!=0;
}

/**
 * @return board width
 */
int CanvasScene::width() const {
	if(_image)
		return _image->image()->width();
	else
		return -1;
}

/**
 * @return board height
 */
int CanvasScene::height() const {
	if(_image)
		return _image->image()->height();
	else
		return -1;
}

/**
 * @return layer stack
 */
dpcore::LayerStack *CanvasScene::layers()
{
	if(_image)
		return _image->image();
	return 0;
}

void CanvasScene::setToolPreview(QGraphicsItem *preview)
{
    delete _toolpreview;
    _toolpreview = preview;
	if(preview)
		addItem(preview);
}

void CanvasScene::startPreview(const dpcore::Brush &brush, const dpcore::Point &point)
{
	_previewpen = penForBrush(brush);
	_lastpreview = point;
	addPreview(point);
}

/**
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param point stroke point
 */
void CanvasScene::addPreview(const dpcore::Point& point)
{
	QGraphicsLineItem *s;
	if(_previewstrokecache.isEmpty()) {
		s = new QGraphicsLineItem();
		addItem(s);
	} else {
		s = _previewstrokecache.takeLast();
		s->show();
	}
	s->setPen(_previewpen);
	s->setLine(_lastpreview.x(), _lastpreview.y(), point.x(), point.y());
	_previewstrokes.append(s);
	_lastpreview = point;

	// Clear out previews automatically.
	// If the user is locked, some strokes may have been dropped by
	// the server, causing an annoying tail of preview strokes.
	_previewClearTimer->start(2000);
}

void CanvasScene::takePreview(int count)
{
	while(count-->0 && !_previewstrokes.isEmpty()) {
		QGraphicsLineItem *s = _previewstrokes.takeFirst();
		s->hide();
		_previewstrokecache.append(s);
	}
}

void CanvasScene::clearPreviews()
{
	while(!_previewstrokes.isEmpty()) {
		QGraphicsLineItem *s = _previewstrokes.takeFirst();
		s->hide();
		_previewstrokecache.append(s);
	}
	// Limit the size of the cache
	while(_previewstrokecache.size() > 100) {
		delete _previewstrokecache.takeLast();
	}
}

void CanvasScene::handleDrawingCommand(protocol::MessagePtr cmd)
{
	if(_statetracker) {
		_statetracker->receiveCommand(cmd);
		emit canvasModified();
	} else {
		qWarning() << "Received a drawing command but canvas does not exist!";
	}
}

void CanvasScene::sendSnapshot()
{
	if(_statetracker) {
		qDebug() << "generating snapshot point...";
		emit newSnapshot(_statetracker->generateSnapshot());
	} else {
		qWarning() << "This shouldn't happen... Received a snapshot request but canvas does not exist!";
	}
}

QPen CanvasScene::penForBrush(const dpcore::Brush &brush)
{
	const int rad = brush.radius(1.0);
	QColor color = brush.color(1.0);
	QPen pen;
	if(rad==0) {
		pen.setWidth(1);
		color.setAlphaF(brush.opacity(1.0));
	} else {
		pen.setWidth(rad*2);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		// Approximate brush transparency
		const qreal a = brush.opacity(1.0) * rad * (1-brush.spacing()/100.0);
		color.setAlphaF(qMin(a, 1.0));
	}
	pen.setColor(color);
	return pen;
}
}

