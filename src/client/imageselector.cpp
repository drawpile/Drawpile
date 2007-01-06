/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPainter>
#include <QList>
#include <QUrl>

#include "imageselector.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

ImageSelector::ImageSelector(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), color_(Qt::white), size_(30,20), mode_(ORIGINAL)
{
	setAcceptDrops(true);
}

QImage ImageSelector::image() const {
	switch(mode_) {
		case ORIGINAL: return original_;
		case IMAGE: return image_;
		case COLOR:
					QImage img(size_, QImage::Format_RGB32);
					img.fill(color_.rgb());
					return img;
	}
	// Should never be reached
	return QImage();
}

/**
 * Draw widget contents on screen
 * @param event event info
 */
void ImageSelector::paintEvent(QPaintEvent *event)
{
	QFrame::paintEvent(event);
	int w = frameWidth();
	QRect bounds = frameRect().adjusted(w,w,-w,-w);
	QPainter painter(this);
	if(mode_ == COLOR) {
		// Draw a rectangle of solid color
		QSize s = size_;
		if(s.width() > bounds.width() || s.height() > bounds.height()) {
			s.scale(bounds.size(), Qt::KeepAspectRatio);
		}
		painter.fillRect(
				QRect(
					w+bounds.width()/2-s.width()/2,
					w+bounds.height()/2-s.height()/2,
					s.width(),
					s.height()
					),
				QBrush(color_)
				);
	} else {
		QPoint p(
				w + bounds.width()/2 - cache_.width()/2,
				w + bounds.height()/2 - cache_.height()/2
				);
		painter.drawPixmap(p,cache_);
	}
}

void ImageSelector::resizeEvent(QResizeEvent *)
{
	if(mode_ == ORIGINAL)
		updateCache(original_);
	else if(mode_ == IMAGE)
		updateCache(image_);
}

void ImageSelector::updateCache(const QImage& src)
{
	int w = frameWidth();
	QRect bounds = frameRect().adjusted(w,w,-w,-w);
	if(src.width() > bounds.width() || src.height() > bounds.height()) {
		cache_ = QPixmap::fromImage(
				src.scaled(bounds.size(), Qt::KeepAspectRatio)
				);
	} else {
		cache_ = QPixmap::fromImage(src);
	}
}

void ImageSelector::setOriginal(const QImage& image)
{
	original_ = image;
	if(mode_ == ORIGINAL) {
		size_ = original_.size();
		emit widthChanged(size_.width());
		emit heightChanged(size_.height());
		updateCache(original_);
		update();
	}
}

void ImageSelector::setImage(const QImage& image)
{
	image_ = image;
	if(mode_ == IMAGE) {
		size_ = image_.size();
		emit widthChanged(size_.width());
		emit heightChanged(size_.height());
		updateCache(image_);
		update();
	}
}

void ImageSelector::setColor(const QColor& color)
{
	color_ = color;
	if(mode_ == COLOR)
		update();
}

void ImageSelector::setWidth(int w)
{
	size_.setWidth(w);
	if(mode_ == COLOR)
		update();
}

void ImageSelector::setHeight(int h)
{
	size_.setHeight(h);
	if(mode_ == COLOR)
		update();
}

void ImageSelector::chooseOriginal()
{
	mode_ = ORIGINAL;
	size_ = original_.size();
	emit widthChanged(size_.width());
	emit heightChanged(size_.height());
	updateCache(original_);
	update();
}

void ImageSelector::chooseColor()
{
	mode_ = COLOR;
	update();
}

void ImageSelector::chooseImage()
{
	mode_ = IMAGE;
	size_ = image_.size();
	emit widthChanged(size_.width());
	emit heightChanged(size_.height());
	updateCache(image_);
	update();
	if(image_.isNull())
		emit noImageSet();
}

/**
 * @brief accept color and image drops
 * @param event event info
 */
void ImageSelector::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasColor() || event->mimeData()->hasImage() ||
			event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

/**
 * @brief handle color and image drops
 * @param event event info
 */
void ImageSelector::dropEvent(QDropEvent *event)
{
	if(event->mimeData()->hasColor()) {
		// Drop color
		QColor col = qvariant_cast<QColor>(event->mimeData()->colorData());
		setColor(col);
		chooseColor();
		//emit colorChanged(col);
		emit colorDropped();
	} else if(event->mimeData()->hasImage()) {
		// Drop image
		QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
		setImage(img);
		chooseImage();
		emit imageDropped();
	} else if(event->mimeData()->hasUrls()) {
		// Drop URL, hopefully to an image
		QList<QUrl> urls = event->mimeData()->urls();
		QImage img(urls.first().toLocalFile());
		if(img.isNull()==false) {
			setImage(img);
			chooseImage();
			emit imageDropped();
		}
	}
}

#ifndef DESIGNER_PLUGIN
}
#endif

