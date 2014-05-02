/*
   DrawPile - a collaborative drawing program.

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

#include "widgets/imageselector.h"
#include "ora/orareader.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QList>
#include <QUrl>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

ImageSelector::ImageSelector(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), _color(Qt::white), _size(30,20), _mode(ORIGINAL)
{
	setAcceptDrops(true);
}

QImage ImageSelector::image() const {
	switch(_mode) {
	case ORIGINAL:
		return _original;
	case IMAGE:
	case IMAGEFILE:
		return _image;
	case COLOR:
		QImage img(_size, QImage::Format_RGB32);
		img.fill(_color.rgb());
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
	const int w = frameWidth();
	const QRect bounds = frameRect().adjusted(w,w,-w,-w);
	QPainter painter(this);
	if(_mode == COLOR) {
		// Draw a rectangle of solid color
		QSize s = _size;
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
				QBrush(_color)
				);
	} else {
		const QPoint p(
				w + bounds.width()/2 - _cache.width()/2,
				w + bounds.height()/2 - _cache.height()/2
				);
		painter.drawPixmap(p, _cache);
	}
}

void ImageSelector::resizeEvent(QResizeEvent *)
{
	if(_mode == ORIGINAL)
		updateCache(_original);
	else if(_mode == IMAGE || _mode == IMAGEFILE)
		updateCache(_image);
}

void ImageSelector::updateCache(const QImage& src)
{
	const int w = frameWidth();
	const QRect bounds = frameRect().adjusted(w,w,-w,-w);
	if(src.width() > bounds.width() || src.height() > bounds.height()) {
		_cache = QPixmap::fromImage(
				src.scaled(bounds.size(), Qt::KeepAspectRatio)
				);
	} else {
		_cache = QPixmap::fromImage(src);
	}
}

void ImageSelector::setOriginal(const QImage& image)
{
	_original = image;
	if(_mode == ORIGINAL) {
		_size = _original.size();
		emit widthChanged(_size.width());
		emit heightChanged(_size.height());
		updateCache(_original);
		update();
	}
}

void ImageSelector::setImage(const QImage& image)
{
	_image = image;
	_imagefile = QString();
	if(_mode == IMAGE || _mode == IMAGEFILE) {
		_size = _image.size();
		emit widthChanged(_size.width());
		emit heightChanged(_size.height());
		updateCache(_image);
		update();
	}
}

void ImageSelector::setImage(const QString &filename)
{
	QImage img;

#ifndef DESIGNER_PLUGIN
	// Special handling for OpenRaster files, as we don't have a loader that integrates with QImage
	if(filename.endsWith(".ora", Qt::CaseInsensitive))
		img = openraster::Reader::loadThumbnail(filename);

	else
#endif
		img.load(filename);

	if(!img.isNull()) {
		setImage(img);
		_imagefile = filename;
	}
}

void ImageSelector::setColor(const QColor& color)
{
	_color = color;
	if(_mode == COLOR)
		update();
}

void ImageSelector::setWidth(int w)
{
	_size.setWidth(w);
	if(_mode == COLOR)
		update();
}

void ImageSelector::setHeight(int h)
{
	_size.setHeight(h);
	if(_mode == COLOR)
		update();
}

void ImageSelector::chooseOriginal()
{
	_mode = ORIGINAL;
	_size = _original.size();
	emit widthChanged(_size.width());
	emit heightChanged(_size.height());
	updateCache(_original);
	update();
}

void ImageSelector::chooseColor()
{
	_mode = COLOR;
	update();
}

void ImageSelector::chooseImage()
{
	if(_image.isNull()) {
		emit noImageSet();
		return;
	}

	_mode = _imagefile.isEmpty() ? IMAGE : IMAGEFILE;
	_size = _image.size();
	emit widthChanged(_size.width());
	emit heightChanged(_size.height());
	updateCache(_image);
	update();
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
		const QColor col = qvariant_cast<QColor>(event->mimeData()->colorData());
		setColor(col);
		chooseColor();
		emit colorDropped();
	} else if(event->mimeData()->hasImage()) {
		// Drop image
		const QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
		setImage(img);
		chooseImage();
		emit imageDropped();
	} else if(event->mimeData()->hasUrls()) {
		// Drop URL, hopefully to an image
		const QList<QUrl> urls = event->mimeData()->urls();
		QString filename = urls.first().toLocalFile();
		setImage(filename);
		if(_imagefile == filename) {
			chooseImage();
			emit imageDropped();
		}
	}
}

#ifndef DESIGNER_PLUGIN
}
#endif

