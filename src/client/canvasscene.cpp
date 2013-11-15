/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2010 Calle Laakkonen

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

#include <QMessageBox>

#include "canvasscene.h"
#include "canvasitem.h"
#include "statetracker.h"

#include "annotationitem.h"
#include "preview.h"
#include "interfaces.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "ora/orawriter.h"
#include "ora/orareader.h"

namespace drawingboard {

CanvasScene::CanvasScene(QObject *parent, interface::BrushSource *brush)
	: QGraphicsScene(parent), _image(0), _statetracker(0), toolpreview_(0), brushsrc_(brush), hla_(false)
{
	setItemIndexMethod(NoIndex);
}

CanvasScene::~CanvasScene()
{
	delete _image;
	delete _statetracker;
}

/**
 * A new image is created with the given size and initialized to a solid color
 * @param size size of the drawing board
 * @param background background color
 */
bool CanvasScene::initBoard(const QSize& size, const QColor& background)
{

	QImage image(size, QImage::Format_RGB32);
	image.fill(background.rgb());

	return initBoard(image);
}

/**
 * An existing image is used as a base.
 * @param image image to use
 */
bool CanvasScene::initBoard(QImage image)
{
	setSceneRect(0,0,image.width(), image.height());
	delete _image;
	delete _statetracker;
	_image = new CanvasItem(image.convertToFormat(QImage::Format_RGB32));
	_statetracker = new StateTracker(_image);
	addItem(_image);
	
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
	previewstarted_ = false;
	return true;
}

bool CanvasScene::initBoard(const QString& file)
{
	using openraster::Reader;
	if(file.endsWith(".ora", Qt::CaseInsensitive)) {
		Reader reader = openraster::Reader(file);
		if(reader.load()==false) {
			qWarning() << "Unable to open" << file << "reason:" << reader.error();
			return false;
		}

		if(reader.warnings() != Reader::NO_WARNINGS) {
			QString text = tr("DrawPile does not support all the features used in this OpenRaster file. Saving this file may result in data loss.\n");
			if((reader.warnings() & Reader::ORA_EXTENDED))
				text += "\n- " + tr("Application specific extensions are used");
			if((reader.warnings() & Reader::ORA_NESTED))
				text += "\n- " + tr("Nested layers are not fully supported.");
			QMessageBox::warning(0, tr("Partially supported OpenRaster"), text);
		}

		// Image loaded, clear out the board
		dpcore::LayerStack *layers = reader.layers();
		setSceneRect(0,0,layers->width(), layers->height());
		delete _image;
		delete _statetracker;
		_image = new CanvasItem(layers);
		_statetracker = new StateTracker(_image);
		addItem(_image);

		// Add annotations (if present)
#if 0
		foreach(QString a, reader.annotations()) {
			AnnotationItem *item = new AnnotationItem(AnnotationItem::nextId(), _image);
			item->forceBorder(hla_);
			// Parse the annotation message
			protocol::Message msg(a);
			item->setOptions(protocol::Annotation(msg.tokens()));
		}
#endif

		// Refresh UI
		QList<QRectF> regions;
		regions.append(sceneRect());
		emit changed(regions);
		previewstarted_ = false;
	} else {
		// Not an OpenRaster file, no need for complex loading
		QImage image;
        if(image.load(file)==false)
			return false;
		return initBoard(image);
	}
	return true;
}

/**
 * @param zeroid if true, set the ID of each annotation to zero
 * @return list of ANNOTATE messages
 */
QStringList CanvasScene::getAnnotations(bool zeroid) const
{
	QStringList messages;
#if 0
	if(_image)
		foreach(QGraphicsItem *item, _image->childItems())
			if(item->type() == AnnotationItem::Type) {
				protocol::Annotation a;
				static_cast<AnnotationItem*>(item)->getOptions(a);
				if(zeroid)
					a.id = 0;
				messages << protocol::Message::quote(a.tokens());
			}
#endif
	return messages;
}

void CanvasScene::clearAnnotations()
{
	if(_image)
		foreach(QGraphicsItem *item, _image->childItems())
			if(item->type() == AnnotationItem::Type) {
				emit annotationDeleted(static_cast<AnnotationItem*>(item));
				delete item;
			}
}

void CanvasScene::showAnnotations(bool show)
{
	if(_image)
		foreach(QGraphicsItem *item, _image->childItems())
			if(item->type() == AnnotationItem::Type)
				item->setVisible(show);
}

void CanvasScene::highlightAnnotations(bool hl)
{
	hla_ = hl;
	if(_image)
		foreach(QGraphicsItem *item, _image->childItems())
			if(item->type() == AnnotationItem::Type)
				static_cast<AnnotationItem*>(item)->forceBorder(hl);
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

/**
 * The file format is determined from the name of the file
 * @param file file path
 * @return true on succcess
 */
bool CanvasScene::save(const QString& file) const
{
	if(file.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact.
		openraster::Writer writer(_image->image());
		writer.setAnnotations(getAnnotations(true));
		return writer.save(file);
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

bool CanvasScene::hasAnnotations() const
{
	if(_image)
		foreach(QGraphicsItem *i, _image->childItems())
			if(i->type() == AnnotationItem::Type)
				return true;
	return false;
}

/**
 * @return board width
 */
int CanvasScene::width() const {
	if(_image)
		return int(_image->boundingRect().width());
	else
		return -1;
}

/**
 * @return board height
 */
int CanvasScene::height() const {
	if(_image)
		return int(_image->boundingRect().height());
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

/**
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param point stroke point
 */
void CanvasScene::addPreview(const dpcore::Point& point)
{
#if 0
	Q_ASSERT(localuser_ != -1);
	User *user = users_.value(localuser_);

	Preview *pre;
	if(previewcache_.isEmpty()) {
		pre = new StrokePreview(user->board());
	} else
		pre = previewcache_.dequeue();
	if(previewstarted_) {
		pre->preview(lastpreview_, point, brushsrc_->getBrush());
	} else {
		pre->preview(point, point, brushsrc_->getBrush());
		previewstarted_ = true;
	}
	lastpreview_ = point;
	previews_.enqueue(pre);
#endif
}

/**
 */
void CanvasScene::endPreview()
{
	previewstarted_ = false;
}

/**
 * This is called when leaving a session. All pending preview strokes
 * are immediately drawn on the board.
 */
void CanvasScene::commitPreviews()
{
	dpcore::Point lastpoint(-1,-1,0);
	while(previews_.isEmpty()==false) {
		qreal distance;
		Preview *p = previews_.dequeue();
		if(p->from() != lastpoint) // TODO
			_image->drawPoint(0, p->from(), p->brush());
		else // TODO
			_image->drawLine(0, p->from(), p->to(), p->brush(), distance);
		lastpoint = p->to();
		delete p;
	}

	while(previewcache_.isEmpty()==false)
		delete previewcache_.dequeue();
}

/**
 * Remove all preview strokes from the board
 */
void CanvasScene::flushPreviews()
{
	while(previews_.isEmpty()==false) {
		Preview *p = previews_.dequeue();
		p->hidePreview();
		previewcache_.enqueue(p);
	}
}

void CanvasScene::handleDrawingCommand(protocol::Message *cmd)
{
	Q_ASSERT(_statetracker);
	_statetracker->receiveCommand(cmd);
}

#if 0
void CanvasScene::annotate(const protocol::Annotation& annotation)
{
	if(!_image) return;
	AnnotationItem *item=0;
	bool newitem = true;
	// Find existing annotation
	foreach(QGraphicsItem *i, _image->childItems()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *ai = static_cast<AnnotationItem*>(i);
			if(ai->id() == annotation.id) {
				item = ai;
				newitem = false;
				break;
			}
		}
	}

	if(item==0) {
		item = new AnnotationItem(annotation.id, _image);
		item->forceBorder(hla_);
	}
	item->setOptions(annotation);
	if(newitem && annotation.user == localuser_)
		emit newLocalAnnotation(item);
}

void CanvasScene::unannotate(int id)
{
	if(!_image) return;
	AnnotationItem *item=0;
	foreach(QGraphicsItem *i, _image->childItems()) {
		if(i->type() == AnnotationItem::Type) {
			AnnotationItem *ai = static_cast<AnnotationItem*>(i);
			if(ai->id() == id) {
				item = ai;
				break;
			}
		}
	}
	if(item) {
		emit annotationDeleted(item);
		delete item;
	} else {
		qWarning() << "Can't delete annotation" << id << "because it doesn't exist!";
	}
}

void CanvasScene::addLayer(const QString& name)
{
	layers()->addLayer(name, layers()->size());
}

/**
 * The layer is removed while making sure all users still have a valid
 * layer selection.
 * to something else.
 * @param layer id
 */
void CanvasScene::deleteLayer(int id, bool mergedown)
{
	const int index = layers()->id2index(id);
	if(index<0) {
		// Should never happen
		qWarning() << "Tried to delete nonexistent layer" << id;
		return;
	}

	if(mergedown)
		layers()->mergeLayerDown(id);

	layers()->deleteLayer(id);

	foreach(User *u, users_) {
		if(u->layer() == id) {
			if(layers()->layers()-index>0)
				u->setLayerId(layers()->getLayerByIndex(index)->id());
			else
				u->setLayerId(layers()->getLayerByIndex(0)->id());
		}
	}

	update();
}

#endif

}

