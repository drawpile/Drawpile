/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#include <QDebug>
#include <QTimer>
#include <QApplication>
#include <QPainter>

#include "scene/canvasscene.h"
#include "scene/canvasitem.h"
#include "scene/selectionitem.h"
#include "scene/annotationitem.h"
#include "scene/usermarkeritem.h"
#include "scene/lasertrailitem.h"
#include "statetracker.h"

#include "net/client.h"
#include "core/annotation.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "ora/orawriter.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent)
	: QGraphicsScene(parent), _image(0), _statetracker(0),
	  _toolpreview(0),
	  _selection(0),
	  _showAnnotations(true), _showAnnotationBorders(false), _showUserMarkers(true), _showUserLayers(true), _showLaserTrails(true)
{
	setItemIndexMethod(NoIndex);

	// Timer for on-canvas animations (user pointer fadeout, laser trail flickering and such)
	_animTickTimer = new QTimer(this);
	connect(_animTickTimer, SIGNAL(timeout()), this, SLOT(advanceUsermarkerAnimation()));
	_animTickTimer->setInterval(200);
	_animTickTimer->start(200);
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
	_statetracker = new StateTracker(_image->image(), client->layerlist(), client->myId(), this);

	connect(_statetracker, &StateTracker::myAnnotationCreated, [this](int id) {
		emit myAnnotationCreated(getAnnotationItem(id));
	});
	connect(_statetracker, SIGNAL(layerAutoselectRequest(int)), this, SIGNAL(layerAutoselectRequest(int)));
	connect(_statetracker, SIGNAL(userMarkerAttribs(int,QColor,QString)), this, SLOT(setUserMarkerAttribs(int,QColor,QString)));
	connect(_statetracker, SIGNAL(userMarkerMove(int,QPointF,int)), this, SLOT(moveUserMarker(int,QPointF,int)));
	connect(_statetracker, SIGNAL(userMarkerHide(int)), this, SLOT(hideUserMarker(int)));

	connect(_image->image(), SIGNAL(resized(int,int,QSize)), this, SLOT(handleCanvasResize(int,int,QSize)));
	connect(_image->image(), SIGNAL(annotationChanged(int)), this, SLOT(handleAnnotationChange(int)));
	connect(client, SIGNAL(layerVisibilityChange(int,bool)), _image->image(), SLOT(setLayerHidden(int,bool)));

	addItem(_image);

	// Clear out any old annotation items
	foreach(QGraphicsItem *item, items()) {
		if(item->type() == AnnotationItem::Type) {
			emit annotationDeleted(static_cast<AnnotationItem*>(item)->id());
			delete item;
		}
	}

	foreach(UserMarkerItem *i, _usermarkers)
		delete i;
	_usermarkers.clear();
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit canvasInitialized();
	emit changed(regions);
}

void CanvasScene::showAnnotations(bool show)
{
	_showAnnotations = show;
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

AnnotationItem *CanvasScene::annotationAt(const QPoint &point)
{
	foreach(QGraphicsItem *i, items(point)) {
		if(i->type() == AnnotationItem::Type)
			return static_cast<AnnotationItem*>(i);
	}
	return 0;
}

QList<int> CanvasScene::listEmptyAnnotations() const
{
	QList<int> ids;
	foreach(const paintcore::Annotation *a, _image->image()->annotations()) {
		if(a->isEmpty())
			ids << a->id();
	}
	return ids;
}

void CanvasScene::handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize)
{
	QRectF bounds = _image->boundingRect();

	// Include some empty space around the canvas to make working
	// near the borders easier.
	const float wPadding = 300;
	const float hPadding = 300;

	setSceneRect(bounds.adjusted(-wPadding, -hPadding, wPadding, hPadding));
	if(xoffset || yoffset) {
		QPoint offset(xoffset, yoffset);

		// Adjust selection (if it exists)
		if(_selection)
			_selection->translate(offset);
	}
	emit canvasResized(xoffset, yoffset, oldsize);
}

AnnotationItem *CanvasScene::getAnnotationItem(int id)
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

int CanvasScene::getAvailableAnnotationId() const
{
	const int prefix = statetracker()->localId() << 8;
	QList<int> takenIds;
	foreach(QGraphicsItem *i, items()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *item = static_cast<AnnotationItem*>(i);
			if((item->id() & 0xff00) == prefix)
				takenIds.append(item->id());
		}
	}

	for(int i=0;i<256;++i) {
		int id = prefix | i;
		if(!takenIds.contains(id))
			return id;
	}

	return 0;
}

void CanvasScene::handleAnnotationChange(int id)
{
	const paintcore::Annotation *a = _image->image()->getAnnotation(id);

	// Find annotation item
	AnnotationItem *item = getAnnotationItem(id);

	// Refresh item
	if(!a) {
		// Annotation deleted
		emit annotationDeleted(id);
		delete item;
	} else {
		if(!item) {
			// Annotation created
			item = new AnnotationItem(id, _image->image());
			item->setShowBorder(showAnnotationBorders());
			item->setVisible(_showAnnotations);
			addItem(item);
		}

		item->refresh();
	}
}

/**
 * @brief Advance canvas animations
 *
 * Note. We don't use the scene's built-in animation features since we care about
 * just a few specific animations.
 */
void CanvasScene::advanceUsermarkerAnimation()
{
	const float STEP = 0.2; // time delta in seconds

	QMutableListIterator<LaserTrailItem*> laseri(_lasertrails);
	while(laseri.hasNext()) {
		auto *l = laseri.next();
		if(l->fadeoutStep(STEP)) {
			delete l;
			laseri.remove();
		}
	}

	foreach(UserMarkerItem *um, _usermarkers) {
		um->fadeoutStep(STEP);
	}

	if(_selection)
		_selection->marchingAnts();
}

/**
 * @return flattened canvas contents
 */
QImage CanvasScene::image() const
{
	if(!hasImage())
		return QImage();

	return _image->image()->toFlatImage(_showAnnotations);
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


	if(_selection) {
		img = img.copy(_selection->polygonRect().intersected(QRect(0, 0, width(), height())));

		if(!_selection->isAxisAlignedRectangle()) {
			// Mask out pixels outside the selection
			QPainter mp(&img);
			QPair<QPoint, QImage> mask = _selection->polygonMask(QColor(255, 255, 255));
			mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			mp.drawImage(0, 0, mask.second);
		}
	}

	return img;
}

void CanvasScene::pasteFromImage(const QImage &image, const QPoint &defaultPoint)
{
	Q_ASSERT(hasImage());

	QPoint center;
	if(_selection)
		center = _selection->polygonRect().center();
	else
		center = defaultPoint;

	SelectionItem *paste = new SelectionItem();
	paste->setRect(QRect(QPoint(center.x() - image.width()/2, center.y() - image.height()/2), image.size()));
	paste->setPasteImage(image);

	setSelectionItem(paste);
}

void CanvasScene::pickColor(int x, int y, int layer, bool bg)
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
			emit colorPicked(color, bg);
		}
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
		return openraster::saveOpenRaster(file, _image->image());
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
	return _image->image()->layers() > 1 || _image->image()->hasAnnotations();
}

/**
 * @retval true if the board contains an image
 */
bool CanvasScene::hasImage() const {
	return _image!=0;
}

QSize CanvasScene::imageSize() const
{
	if(_image)
		return QSize(_image->image()->width(), _image->image()->height());
	else
		return QSize();
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

void CanvasScene::setSelectionItem(const QRect &rect)
{
	auto *sel = new SelectionItem;
	sel->setRect(rect);
	setSelectionItem(sel);
}

void CanvasScene::setSelectionItem(const QPolygon &polygon)
{
	auto *sel = new SelectionItem;
	sel->setPolygon(polygon);
	setSelectionItem(sel);
}

void CanvasScene::setSelectionItem(SelectionItem *selection)
{
	delete _selection;
	_selection = selection;
	if(selection)
		addItem(selection);
}

void CanvasScene::handleLocalCommand(protocol::MessagePtr cmd)
{
	if(_statetracker) {
		_statetracker->localCommand(cmd);
		emit canvasModified();
	} else {
		qWarning() << "Received a local drawing command but canvas does not exist!";
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
	const int size = brush.size2();
	QColor color = brush.color(1.0);
	QPen pen;
	if(size<=1) {
		pen.setWidth(1);
		color.setAlphaF(brush.opacity(1.0));
	} else {
		pen.setWidth(size);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		// Approximate brush transparency
		const qreal a = brush.opacity(1.0) * qMin(1.0, 0.5+brush.hardness(1.0)) * (1-brush.spacing()/100.0);
		color.setAlphaF(qMin(a, 1.0));
	}
	pen.setColor(color);
	return pen;
}

void CanvasScene::setTitle(const QString &title)
{
	if(_statetracker)
		_statetracker->setTitle(title);
}

QString CanvasScene::title() const
{
	if(_statetracker)
		return _statetracker->title();
	else
		return QString();
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

void CanvasScene::setUserMarkerAttribs(int id, const QColor &color, const QString &layer)
{
	auto *item = getOrCreateUserMarker(id);
	item->setColor(color);
	item->setSubtext(_showUserLayers ? layer : QString());
}

void CanvasScene::moveUserMarker(int id, const QPointF &point, int trail)
{
	auto *item = getOrCreateUserMarker(id);

	if(trail>0 && _showLaserTrails) {
		auto *laser = new LaserTrailItem(QLineF(item->pos(), point), item->color(), trail);
		_lasertrails.append(laser);
		addItem(laser);

		// Limit total laser trail length
		if(_lasertrails.size() > 500)
			delete _lasertrails.takeFirst();
	}

	item->setPos(point);

	if(_showUserMarkers)
		item->fadein();
}

void CanvasScene::hideUserMarker(int id)
{
	if(id<0 && _statetracker)
		id = _statetracker->localId();

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

void CanvasScene::showUserLayers(bool show)
{
	_showUserLayers = show;
	QHashIterator<int,UserMarkerItem*> i(_usermarkers);
	while(i.hasNext()) {
		i.next();

		QString layer;
		if(show && _statetracker->drawingContexts().contains(i.key())) {
			int id = _statetracker->drawingContexts()[i.key()].tool.layer_id;
			paintcore::Layer *l = _statetracker->image()->getLayer(id);
			if(l)
				layer = l->title();
		}
		i.value()->setSubtext(layer);
	}
}

void CanvasScene::showLaserTrails(bool show)
{
	_showLaserTrails = show;
	if(!show) {
		while(!_lasertrails.isEmpty())
			delete _lasertrails.takeLast();
	}
}

}
