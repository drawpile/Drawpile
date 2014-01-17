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
#include <QApplication>
#include <QPainter>

#include "canvasscene.h"
#include "canvasitem.h"
#include "selectionitem.h"
#include "annotationitem.h"
#include "usermarkeritem.h"
#include "statetracker.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "ora/orawriter.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent)
	: QGraphicsScene(parent), _image(0), _statetracker(0), _toolpreview(0), _selection(0), _showAnnotationBorders(false), _showUserMarkers(true)
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
void CanvasScene::initCanvas(net::Client *client)
{
	delete _image;
	delete _statetracker;
	_image = new CanvasItem();
	_statetracker = new StateTracker(this, client);
	
	connect(_statetracker, SIGNAL(myAnnotationCreated(AnnotationItem*)), this, SIGNAL(myAnnotationCreated(AnnotationItem*)));
	connect(_statetracker, SIGNAL(myLayerCreated(int)), this, SIGNAL(myLayerCreated(int)));
	connect(_image->image(), SIGNAL(resized(int,int)), this, SLOT(handleCanvasResize(int,int)));

	addItem(_image);
	clearAnnotations();

	foreach(QGraphicsLineItem *p, _previewstrokes)
		delete p;
	_previewstrokes.clear();
	foreach(QGraphicsLineItem *p, _previewstrokecache)
		delete p;
	_previewstrokecache.clear();
	foreach(UserMarkerItem *i, _usermarkers)
		delete i;
	_usermarkers.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit canvasInitialized();
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

void CanvasScene::handleCanvasResize(int xoffset, int yoffset)
{
	setSceneRect(_image->boundingRect());
	if(xoffset || yoffset) {
		QPoint offset(xoffset, yoffset);

		// Adjust annotation positions
		foreach(AnnotationItem *a, getAnnotations()) {
			a->setGeometry(a->geometry().translated(offset));
		}

		// Adjust selection (if it exists)
		if(_selection) {
			_selection->setRect(_selection->rect().translated(offset));
		}
	}
}

/**
 * @return flattened canvas contents
 */
QImage CanvasScene::image() const
{
	if(!hasImage())
		return QImage();

	QImage image = _image->image()->toFlatImage();

	// Include visible annotations
	{
		QPainter painter(&image);
		foreach(AnnotationItem *a, getAnnotations(true)) {
			QImage ai = a->toImage();
			painter.drawImage(a->geometry().topLeft(), ai);
		}
	}

	return image;
}

QImage CanvasScene::selectionToImage(int layerId)
{
	if(!hasImage())
		return QImage();

	QImage img;

	paintcore::Layer *layer = layers()->getLayer(layerId);
	if(layer)
		img = layer->toImage();
	else
		img = image();

	if(_selection)
		img = img.copy(_selection->rect().intersected(QRect(0, 0, width(), height())));

	return img;
}

void CanvasScene::pasteFromImage(const QImage &image, const QPoint &defaultPoint)
{
	Q_ASSERT(hasImage());

	QPoint center;
	if(_selection)
		center = _selection->rect().center();
	else
		center = defaultPoint;

	SelectionItem *paste = new SelectionItem();
	paste->setRect(QRect(QPoint(center.x() - image.width()/2, center.y() - image.height()/2), image.size()));
	paste->setPasteImage(image);

	setSelectionItem(paste);
}

void CanvasScene::pickColor(int x, int y, int layer)
{
	if(_image) {
		QColor color;
		if(layer>0) {
			const paintcore::Layer *l = _image->image()->getLayer(layer);
			if(layer)
				color = l->colorAt(x, y);
		} else {
			color = _image->image()->colorAt(x, y);
		}

		if(color.isValid() && color.alpha()>0) {
			color.setAlpha(255);
			emit colorPicked(color);
		}
	}
}

QList<AnnotationItem*> CanvasScene::getAnnotations(bool onlyVisible) const
{
	QList<AnnotationItem*> annotations;
	foreach(QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *a = static_cast<AnnotationItem*>(i);
			if(!onlyVisible || a->isVisible())
				annotations.append(a);
		}
	}
	return annotations;
}

void CanvasScene::setAnnotations(const QVector<AnnotationState> &annotations)
{
	clearAnnotations();
	foreach(const AnnotationState &a, annotations) {
		AnnotationItem *i = new AnnotationItem(a);
		i->setShowBorder(showAnnotationBorders());
		addItem(i);
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
		return openraster::saveOpenRaster(file, _image->image(), getAnnotations());
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
paintcore::LayerStack *CanvasScene::layers()
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

void CanvasScene::setSelectionItem(SelectionItem *selection)
{
	delete _selection;
	_selection = selection;
	if(selection)
		addItem(selection);
}

void CanvasScene::startPreview(const paintcore::Brush &brush, const paintcore::Point &point)
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
void CanvasScene::addPreview(const paintcore::Point& point)
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

void CanvasScene::sendSnapshot(bool forcenew)
{
	if(_statetracker) {
		qDebug() << "generating snapshot point...";
		emit newSnapshot(_statetracker->generateSnapshot(forcenew));
	} else {
		qWarning() << "This shouldn't happen... Received a snapshot request but canvas does not exist!";
	}
}

QPen CanvasScene::penForBrush(const paintcore::Brush &brush)
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
		const qreal a = brush.opacity(1.0) * qMin(1.0, 0.5+brush.hardness(1.0)) * (1-brush.spacing()/100.0);
		color.setAlphaF(qMin(a, 1.0));
	}
	pen.setColor(color);
	return pen;
}

UserMarkerItem *CanvasScene::getOrCreateUserMarker(int id)
{
	if(_usermarkers.contains(id))
		return _usermarkers[id];

	UserMarkerItem *item = new UserMarkerItem;
	item->setColor(QColor(0, 0, 0));
	item->setText(QString("#%1").arg(id));
	item->hide();
	addItem(item);
	_usermarkers[id] = item;
	return item;
}

void CanvasScene::setUserMarkerName(int id, const QString &name)
{
	auto *item = getOrCreateUserMarker(id);
	item->setText(name);
}

void CanvasScene::moveUserMarker(int id, const QColor &color, const paintcore::Point &point)
{
	if(_showUserMarkers) {
		auto *item = getOrCreateUserMarker(id);
		item->setColor(color);
		item->setPos(point);
		item->fadein();
	}
}

void CanvasScene::hideUserMarker(int id)
{
	if(_usermarkers.contains(id)) {
		_usermarkers[id]->fadeout();
	}
}

void CanvasScene::showUserMarkers(bool show)
{
	_showUserMarkers = show;
	if(!show) {
		foreach(UserMarkerItem *item, _usermarkers)
			item->hide();
	}
}

}
