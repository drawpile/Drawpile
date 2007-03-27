/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>

#include "palettewidget.h"
#include "palette.h"

namespace widgets {

PaletteWidget::PaletteWidget(QWidget *parent)
	: QWidget(parent), palette_(0), swatchsize_(13,8), spacing_(3), scroll_(0),
	dragsource_(-1)
{
	setAcceptDrops(true);
	scrollbar_ = new QScrollBar(this);
	connect(scrollbar_, SIGNAL(valueChanged(int)), this, SLOT(scroll(int)));
}

void PaletteWidget::setPalette(::Palette *palette)
{
	palette_ = palette;
	update();
}

void PaletteWidget::resizeEvent(QResizeEvent *event)
{
	scrollbar_->setGeometry(QRect(
			width() - scrollbar_->sizeHint().width(),
			0,
			scrollbar_->sizeHint().width(),
			height()
			)
		);

	const int rows = palette_->count() / columns() * (swatchsize_.height()+spacing_);
	if(rows < height())
		scrollbar_->setMaximum( 0 );
	else
		scrollbar_->setMaximum( rows - height() );
	scrollbar_->setPageStep( height() );
			
}

/**
 * @return number of swatches per row
 */
int PaletteWidget::columns() const {
	const int c = (contentsRect().width()-scrollbar_->width()) /
		(swatchsize_.width()+spacing_);
	return qMax(c, 1);
}

void PaletteWidget::scroll(int pos)
{
	scroll_ = pos;
	update();
}

void PaletteWidget::paintEvent(QPaintEvent *)
{
	if(palette_==0)
		return;
	QPainter painter(this);

	const int col = columns();

	QRect swatch(QPoint(spacing_,spacing_ - scroll_), swatchsize_);
	for(int i=0;i<palette_->count();++i) {
		painter.fillRect(swatch, palette_->color(i));
		swatch.translate(swatchsize_.width() + spacing_, 0);
		if((i+1)%col==0) 
			swatch.moveTo(spacing_, swatch.y() + swatchsize_.height() + spacing_);
	}
}

void PaletteWidget::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton) {
		const int i = indexAt(event->pos());
		if(i!=-1) {
			dragstart_ = event->pos();
			dragsource_ = i;
		}
	}
}

void PaletteWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(dragsource_ != -1 && (event->buttons() & Qt::LeftButton) &&
			(event->pos() - dragstart_).manhattanLength() >
			QApplication::startDragDistance())
	{
		QDrag *drag = new QDrag(this);

		QMimeData *mimedata = new QMimeData;
		QColor color = palette_->color(dragsource_);
		mimedata->setColorData(color);

		drag->setMimeData(mimedata);
		drag->start(Qt::CopyAction);
	}
}

void PaletteWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(dragsource_ != -1)
		emit colorSelected(palette_->color(dragsource_));
}

void PaletteWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color"))
		event->acceptProposedAction();

}

void PaletteWidget::dropEvent(QDropEvent *event)
{
	const int index = indexAt(event->pos());
	if(index != -1) {
		const QColor color = qvariant_cast<QColor>(event->mimeData()->colorData());
		palette_->setColor(index, color);
		update();
	}
}

void PaletteWidget::wheelEvent(QWheelEvent *event)
{
	if(event->orientation() == Qt::Vertical) {
		scrollbar_->setValue(scrollbar_->value() - event->delta()/16);
	}
}

/**
 * @param point coordinates inside the widget
 * @return index number of color swatch at point
 * @retval -1 if point is outside any color swatch
 */
int PaletteWidget::indexAt(const QPoint& point) const
{
	const int xw = spacing_ + swatchsize_.width();
	const int x = point.x() / xw;
	if(point.x() < x * xw + spacing_)
		return -1;

	const int yw = spacing_ + swatchsize_.height();
	const int y = (point.y()+scroll_) / yw;
	if(point.y()+scroll_ < y * yw + spacing_)
		return -1;

	const int index = y * columns() + x;
	if(index >= palette_->count())
		return -1;
	return index;
}

}

