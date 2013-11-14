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

#include "board.h"
#include "boarditem.h"
#include "annotationitem.h"
#include "user.h"
#include "preview.h"
#include "interfaces.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "ora/orawriter.h"
#include "ora/orareader.h"
#include "../shared/net/annotation.h"
#include "../shared/net/message.h"

namespace drawingboard {

Board::Board(QObject *parent, interface::BrushSource *brush, interface::ColorSource *color)
	: QGraphicsScene(parent), image_(0),localuser_(-1), toolpreview_(0), brushsrc_(brush), colorsrc_(color), hla_(false), layerwidget_(0)
{
	setItemIndexMethod(NoIndex);
}

Board::~Board()
{
	delete image_;
	clearUsers();
}

/**
 * A new image is created with the given size and initialized to a solid color
 * @param size size of the drawing board
 * @param background background color
 */
bool Board::initBoard(const QSize& size, const QColor& background)
{

	QImage image(size, QImage::Format_RGB32);
	image.fill(background.rgb());

	return initBoard(image);
}

/**
 * An existing image is used as a base.
 * @param image image to use
 */
bool Board::initBoard(QImage image)
{
	setSceneRect(0,0,image.width(), image.height());
	delete image_;
	image_ = new BoardItem(image.convertToFormat(QImage::Format_RGB32));
	addItem(image_);
	foreach(User *u, users_)
		u->setBoard(image_);
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
	previewstarted_ = false;
	return true;
}

bool Board::initBoard(const QString& file)
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
		delete image_;
		image_ = new BoardItem(layers);
		addItem(image_);
		foreach(User *u, users_)
			u->setBoard(image_);

		// Add annotations (if present)
#if 0
		foreach(QString a, reader.annotations()) {
			AnnotationItem *item = new AnnotationItem(AnnotationItem::nextId(), image_);
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
 * A single (local) user is needed to modify the drawing board, be sure
 * to create one after deleting everyone.
 */
void Board::clearUsers()
{
	commitPreviews();
	foreach(User *u, users_) {
		delete u;
	}
	users_.clear();
	localuser_ = -1;
}

/**
 * @param zeroid if true, set the ID of each annotation to zero
 * @return list of ANNOTATE messages
 */
QStringList Board::getAnnotations(bool zeroid) const
{
	QStringList messages;
#if 0
	if(image_)
		foreach(QGraphicsItem *item, image_->childItems())
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

void Board::clearAnnotations()
{
	if(image_)
		foreach(QGraphicsItem *item, image_->childItems())
			if(item->type() == AnnotationItem::Type) {
				emit annotationDeleted(static_cast<AnnotationItem*>(item));
				delete item;
			}
}

void Board::showAnnotations(bool show)
{
	if(image_)
		foreach(QGraphicsItem *item, image_->childItems())
			if(item->type() == AnnotationItem::Type)
				item->setVisible(show);
}

void Board::highlightAnnotations(bool hl)
{
	hla_ = hl;
	if(image_)
		foreach(QGraphicsItem *item, image_->childItems())
			if(item->type() == AnnotationItem::Type)
				static_cast<AnnotationItem*>(item)->forceBorder(hl);
}

/**
 * @param id user id
 */
void Board::addUser(int id)
{
	if(users_.contains(id)) {
		// This is not necessarily an error condition. The server will not
		// ghost users that haven't drawn anything. In those cases, it is
		// safe to delete the user as it's not being used anywhere.

		// Depending on the server, we might get info about ourselves twice.
		// First to tell us about ourselves (the user ID), and a second
		// time when joining a session. We have already given out a pointer
		// to the local user, so don't delete it.
		if(id==localuser_)
			return;

		qDebug() << "Reusing board user" << id;
		delete users_.take(id);
	}
	User *user = new User(image_, id);
	users_[id] = user;
}

/**
 * Designates one user as the local user
 * @param id user id
 * @pre id must have been added with addUser
 */
void Board::setLocalUser(int id)
{
	Q_ASSERT(users_.contains(id));
	localuser_ = id;
	// Set the layer list if we know it already
	if(layerwidget_)
		users_[localuser_]->setLayerList(layerwidget_);
}

/**
 * @param llist layer list widget
 */
void Board::setLayerList(widgets::LayerList *llist)
{
	layerwidget_ = llist;
	// Update local user if it exists
	if(localuser_ != -1)
		users_[localuser_]->setLayerList(layerwidget_);
}

/**
 * @return board contents
 */
QImage Board::image() const
{
	if(image_)
		return image_->image()->toFlatImage();
	else
		return QImage();
}

/**
 * The file format is determined from the name of the file
 * @param file file path
 * @return true on succcess
 */
bool Board::save(const QString& file) const
{
	if(file.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact.
		openraster::Writer writer(image_->image());
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
bool Board::needSaveOra() const
{
	return image_->image()->layers() > 1 ||
		hasAnnotations();
}

/**
 * @retval true if the board contains an image
 */
bool Board::hasImage() const {
	return image_!=0;
}

bool Board::hasAnnotations() const
{
	if(image_)
		foreach(QGraphicsItem *i, image_->childItems())
			if(i->type() == AnnotationItem::Type)
				return true;
	return false;
}

/**
 * @return board width
 */
int Board::width() const {
	if(image_)
		return int(image_->boundingRect().width());
	else
		return -1;
}

/**
 * @return board height
 */
int Board::height() const {
	if(image_)
		return int(image_->boundingRect().height());
	else
		return -1;
}

/**
 * @return layer stack
 */
dpcore::LayerStack *Board::layers()
{
	if(image_)
		return image_->image();
	return 0;
}

#if 0
/**
 * Returns a BoardEditor for modifying the drawing board either
 * directly or over the network.
 * @param session which network session the editor works over. If 0, a local editor is returned
 */
BoardEditor *Board::getEditor(network::SessionState *session)
{
	Q_ASSERT(localuser_ != -1);
	User *user = users_.value(localuser_);
	Q_ASSERT(user);
	if(session)
		return new RemoteBoardEditor(this, user, session, brushsrc_, colorsrc_);
	else
		return new LocalBoardEditor(this, user, brushsrc_, colorsrc_);
}
#endif

/**
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param point stroke point
 */
void Board::addPreview(const dpcore::Point& point)
{
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
}

/**
 */
void Board::endPreview()
{
	previewstarted_ = false;
}

/**
 * This is called when leaving a session. All pending preview strokes
 * are immediately drawn on the board.
 */
void Board::commitPreviews()
{
	dpcore::Point lastpoint(-1,-1,0);
	while(previews_.isEmpty()==false) {
		qreal distance;
		Preview *p = previews_.dequeue();
		if(p->from() != lastpoint) // TODO
			image_->drawPoint(0, p->from(), p->brush());
		else // TODO
			image_->drawLine(0, p->from(), p->to(), p->brush(), distance);
		lastpoint = p->to();
		delete p;
	}

	while(previewcache_.isEmpty()==false)
		delete previewcache_.dequeue();
}

/**
 * Remove all preview strokes from the board
 */
void Board::flushPreviews()
{
	while(previews_.isEmpty()==false) {
		Preview *p = previews_.dequeue();
		p->hidePreview();
		previewcache_.enqueue(p);
	}
}

/**
 * @param user user id
 * @param brush brush to use
 * @pre user must exist
 */
void Board::userSetTool(int user, const dpcore::Brush& brush)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->setBrush(brush);
}

/**
 * @param user user id
 * @param point coordinates
 * @pre user must exist
 */
void Board::userStroke(int user, const dpcore::Point& point)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->addStroke(point);
	if(user == localuser_ && previews_.isEmpty() == false) {
		Q_ASSERT(!previews_.isEmpty());
		Preview *pre = previews_.dequeue();
		pre->hidePreview();
		previewcache_.enqueue(pre);
	}
}

/**
 * @param user user id
 * @pre user must exist
 */
void Board::userEndStroke(int user)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->endStroke();
}

#if 0
void Board::annotate(const protocol::Annotation& annotation)
{
	if(!image_) return;
	AnnotationItem *item=0;
	bool newitem = true;
	// Find existing annotation
	foreach(QGraphicsItem *i, image_->childItems()) {
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
		item = new AnnotationItem(annotation.id, image_);
		item->forceBorder(hla_);
	}
	item->setOptions(annotation);
	if(newitem && annotation.user == localuser_)
		emit newLocalAnnotation(item);
}

void Board::unannotate(int id)
{
	if(!image_) return;
	AnnotationItem *item=0;
	foreach(QGraphicsItem *i, image_->childItems()) {
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
#endif

void Board::addLayer(const QString& name)
{
	layers()->addLayer(name, layers()->size());
}

/**
 * The layer is removed while making sure all users still have a valid
 * layer selection.
 * to something else.
 * @param layer id
 */
void Board::deleteLayer(int id, bool mergedown)
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

}

