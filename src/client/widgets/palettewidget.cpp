/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QRubberBand>
#include <QHelpEvent>
#include <QToolTip>
#include <QMenu>
#include <QDrag>
#include <QMimeData>

#include "dialogs/colordialog.h"
#include "palettewidget.h"
#include "utils/palette.h"

namespace widgets {

PaletteWidget::PaletteWidget(QWidget *parent)
	: QWidget(parent), palette_(0), swatchsize_(13,8), spacing_(1), scroll_(0),
	selection_(-1), dialogsel_(-2)
{
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	outline_ = new QRubberBand(QRubberBand::Rectangle, this);

	contextmenu_ = new QMenu(this);
	QAction *add= contextmenu_->addAction(tr("Add"));
	QAction *edit = contextmenu_->addAction(tr("Modify"));
	QAction *remove = contextmenu_->addAction(tr("Remove"));
	connect(add, SIGNAL(triggered()), this, SLOT(addColor()));
	connect(edit, SIGNAL(triggered()), this, SLOT(editCurrentColor()));
	connect(remove, SIGNAL(triggered()), this, SLOT(removeColor()));

	scrollbar_ = new QScrollBar(this);
	connect(scrollbar_, SIGNAL(valueChanged(int)), this, SLOT(scroll(int)));

	colordlg_ = new dialogs::ColorDialog(this, tr("Set palette color"));

	connect(colordlg_, SIGNAL(colorSelected(QColor)),
			this, SLOT(setCurrentColor(QColor)));

	connect(colordlg_, SIGNAL(finished(int)),
			this, SLOT(dialogDone()));
}

void PaletteWidget::setSwatchSize(int width, int height)
{
	swatchsize_ = QSize(width, height);
	update();
}

void PaletteWidget::setSpacing(int spacing)
{
	spacing_ = spacing;
	update();
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
	scrollbar_->setVisible(false);
	if(palette_) {
		// First calculate required space without scrollbar
		rows = (palette_->count() / columns()+1) * (swatchsize_.height()+spacing_);
	}

	if(rows <= height()) {
		scrollbar_->setMaximum( 0 );
	} else {
		// not enough room even without scrollbar, recalculate with scrollbar
		scrollbar_->setVisible(true);
		rows = (palette_->count() / columns()+1) * (swatchsize_.height()+spacing_);
		scrollbar_->setMaximum( rows - height() );
	}
	scrollbar_->setPageStep( height() );
			
}

/**
 * @return number of swatches per row
 */
int PaletteWidget::columns() const {
	const int c = (contentsRect().width()-(scrollbar_->isVisible()?scrollbar_->width():0)) / (swatchsize_.width()+spacing_);
	return qMax(c, 1);
}

void PaletteWidget::scroll(int pos)
{
	scroll_ = pos;
	update();
}

/**
 * Pop up a dialog to add a new color
 */
void PaletteWidget::addColor()
{
	if(dialogsel_<-1) {
		dialogsel_ = -1;
		colordlg_->setColor(Qt::black);
		colordlg_->show();
	}
}

void PaletteWidget::removeColor()
{
	Q_ASSERT(palette_);
	palette_->removeColor(selection_);
	if(selection_ >= palette_->count()) {
		outline_->hide();
		selection_ = -1;
	}
	update();
}

/**
 * Pop up a dialog to edit the currently selected color
 */
void PaletteWidget::editCurrentColor()
{
	Q_ASSERT(palette_);
	if(dialogsel_<-1 && selection_ >= 0) {
		dialogsel_ = selection_;
		colordlg_->setColor(palette_->color(selection_));
		colordlg_->show();
	}
}

/**
 * Current color was set in the selection dialog.
 * If selection was the special value -1, add a new color.
 * @pre dialogsel >= -1 (a valid selection value)
 */
void PaletteWidget::setCurrentColor(const QColor& color)
{
	Q_ASSERT(palette_);
	Q_ASSERT(dialogsel_>-2);
	if(dialogsel_==-1) {
		if(selection_ == -1)
			selection_ = palette_->count();
		palette_->insertColor(selection_, color);
	} else {
		palette_->setColor(selection_, color);
	}
	update();
}

/**
 * Dialog was finished, mark dialog selection as unselected
 * so the dialog can be opened again.
 */
void PaletteWidget::dialogDone()
{
	dialogsel_ = -2;
}

/**
 * Handle special events.
 * Currently only tooltip event is handled here. Display a tooltip
 * that describes the color under the pointer.
 */
bool PaletteWidget::event(QEvent *event)
{
	if(event->type() == QEvent::ToolTip) {
		const QPoint pos = (static_cast<const QHelpEvent*>(event))->pos();
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
	selection_ = indexAt(event->pos());
	if(selection_!=-1) {
		outline_->setGeometry(swatchRect(selection_).adjusted(-1,-1,1,1));
		outline_->show();
	} else {
		outline_->hide();
	}
}

void PaletteWidget::contextMenuEvent(QContextMenuEvent *event)
{
	if(palette_)
		contextmenu_->popup(mapToGlobal(event->pos()));
}

void PaletteWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(selection_ != -1 && (event->buttons() & Qt::LeftButton) &&
			(event->pos() - dragstart_).manhattanLength() >
			QApplication::startDragDistance())
	{
		QDrag *drag = new QDrag(this);

		QMimeData *mimedata = new QMimeData;
		const QColor color = palette_->color(selection_);
		mimedata->setColorData(color);

		drag->setMimeData(mimedata);
		drag->start(Qt::CopyAction|Qt::MoveAction);
	}
}

void PaletteWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(selection_ != -1) {
		if(event->button()==Qt::LeftButton)
			emit colorSelected(palette_->color(selection_));
	}
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	if(selection_ > -1)
		editCurrentColor();
	else
		addColor();
}

void PaletteWidget::keyReleaseEvent(QKeyEvent *event)
{
	if(selection_ != -1 && event->key() == Qt::Key_Delete)
		removeColor();
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
	const int index = indexAt(event->pos());
	if(index != -1) {
		outline_->setGeometry(swatchRect(index));
	} else {
		outline_->setGeometry(betweenRect(nearestAt(event->pos())).adjusted(-1,-1,1,1));
	}
}

void PaletteWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
	if(selection_!=-1 && hasFocus()) {
		outline_->setGeometry(swatchRect(selection_).adjusted(-1,-1,1,1));
	} else {
		outline_->hide();
	}
}

void PaletteWidget::focusInEvent(QFocusEvent*)
{
	if(selection_!=-1)
		outline_->setGeometry(swatchRect(selection_).adjusted(-1,-1,1,1));
}

void PaletteWidget::focusOutEvent(QFocusEvent*)
{
	outline_->hide();
}

void PaletteWidget::dropEvent(QDropEvent *event)
{
	int index = indexAt(event->pos());
	if(index != -1) {
		if(event->source() == this) {
			// Switch colors
			palette_->setColor(selection_, palette_->color(index));
		}
		palette_->setColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	} else {
		index = nearestAt(event->pos());
		if(event->source() == this) {
			// Move color
			palette_->removeColor(selection_);
			if(index >= selection_)
				--index;
		}
		palette_->insertColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	}
	selection_ = index;
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

