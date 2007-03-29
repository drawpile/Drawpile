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
#include <QRubberBand>
#include <QHelpEvent>
#include <QToolTip>
#include <QMenu>

#include "palettewidget.h"
#include "palette.h"

namespace widgets {

PaletteWidget::PaletteWidget(QWidget *parent)
	: QWidget(parent), palette_(0), swatchsize_(13,8), spacing_(3), scroll_(0),
	dragsource_(-1)
{
	setAcceptDrops(true);
	outline_ = new QRubberBand(QRubberBand::Rectangle, this);

	contextmenu_ = new QMenu(this);
	QAction *remove = contextmenu_->addAction(tr("Remove"));
	connect(remove, SIGNAL(triggered()), this, SLOT(removeColor()));

	scrollbar_ = new QScrollBar(this);
	connect(scrollbar_, SIGNAL(valueChanged(int)), this, SLOT(scroll(int)));
}

void PaletteWidget::setPalette(Palette *palette)
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

	int rows = 0;
	if(palette_)
		rows = palette_->count() / columns() * (swatchsize_.height()+spacing_);

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

void PaletteWidget::removeColor()
{
	palette_->removeColor(dragsource_);
	outline_->hide();
	update();
}

bool PaletteWidget::event(QEvent *event)
{
	if(event->type() == QEvent::ToolTip) {
		QPoint pos = (static_cast<const QHelpEvent*>(event))->pos();
		const int index = indexAt(pos);
		if(index != -1) {
			const QColor c = palette_->color(index);
			QToolTip::showText(
					mapToGlobal(pos),
					tr("Red: %1\nGreen: %2\nBlue: %3").arg(c.red()).arg(c.green()).arg(c.blue()),
					this,
					swatchRect(index)
					);
			event->accept();
			return true;
		}
	}
	return QWidget::event(event);
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
	dragstart_ = event->pos();
	dragsource_ = indexAt(event->pos());
	if(dragsource_!=-1) {
		outline_->setGeometry(swatchRect(dragsource_));
		outline_->show();
		if(event->button()==Qt::RightButton)
			contextmenu_->popup(mapToGlobal(event->pos()));
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
		drag->start(Qt::CopyAction|Qt::MoveAction);
	}
}

void PaletteWidget::mouseReleaseEvent(QMouseEvent *event)
{
	outline_->hide();
	if(dragsource_ != -1) {
		if(event->button()==Qt::LeftButton)
			emit colorSelected(palette_->color(dragsource_));
	}
}

void PaletteWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color")
			&& palette_ != 0) {
		if(event->source() == this)
			event->setDropAction(Qt::MoveAction);
		event->accept();
		outline_->show();
	}
}

void PaletteWidget::dragMoveEvent(QDragMoveEvent *event)
{
	int index = indexAt(event->pos());
	if(index != -1) {
		outline_->setGeometry(swatchRect(index));
	} else {
		outline_->setGeometry(betweenRect(nearestAt(event->pos())));
	}
}

void PaletteWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
	outline_->hide();
}


void PaletteWidget::dropEvent(QDropEvent *event)
{
	int index = indexAt(event->pos());
	outline_->hide();
	if(index != -1) {
		if(event->source() == this) {
			// Switch colors
			palette_->setColor(dragsource_, palette_->color(index));
		}
		palette_->setColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	} else {
		index = nearestAt(event->pos());
		if(event->source() == this) {
			// Move color
			palette_->removeColor(dragsource_);
			if(index >= dragsource_)
				--index;
		}
		palette_->insertColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	}
	update();
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
	if(palette_ == 0)
		return -1;
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

/**
 * Return the index nearest to the point. If the position is after
 * the last palette entry, Palette::count() is returned.
 * @param point coordinates inside the widget
 * @return index of the color swatch nearest
 * @pre palette_ != 0
 */
int PaletteWidget::nearestAt(const QPoint& point) const
{
	const int x = point.x() / (spacing_ + swatchsize_.width());
	const int y = (point.y()+scroll_) / (spacing_ + swatchsize_.height());

	const int index = y * columns() + x;
	if(index > palette_->count())
		return palette_->count();

	return index;
}

/**
 * Get the position of the color swatch
 * @param index of the color swatch
 * @return swatch geometry
 * @pre 0 <= index
 */
QRect PaletteWidget::swatchRect(int index) const
{
	const int cols = columns();
	return QRect(
			spacing_ + (swatchsize_.width()+spacing_) * (index%cols),
			spacing_ + (swatchsize_.height()+spacing_) * (index/cols) - scroll_,
			swatchsize_.width(),
			swatchsize_.height()
			);
}

/**
 * Get a rectangle between two color swatches.
 * @param index swatch index after the space
 * @return separator area between two swatches
 */
QRect PaletteWidget::betweenRect(int index) const
{
	const int cols = columns();
	return QRect(
			spacing_/2 + (swatchsize_.width()+spacing_) * (index%cols),
			spacing_ + (swatchsize_.height()+spacing_) * (index/cols) - scroll_,
			spacing_/2,
			swatchsize_.height()
			);
}

}

