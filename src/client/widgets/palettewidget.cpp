/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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

#include "palettewidget.h"
#include "utils/palette.h"

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

#include <Color_Dialog>

namespace widgets {

PaletteWidget::PaletteWidget(QWidget *parent)
	: QWidget(parent), _palette(0),
	  _swatchsize(13,8), _columns(16), _spacing(1), _leftMargin(0),
	  _scroll(0), _selection(-1), _dialogsel(-2)
{
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	_outline = new QRubberBand(QRubberBand::Rectangle, this);

	_contextmenu = new QMenu(this);
	_contextmenu->addAction(tr("Add"), this, SLOT(addColor()));
	_contextmenu->addAction(tr("Modify"), this, SLOT(editCurrentColor()));
	_contextmenu->addAction(tr("Remove"), this, SLOT(removeColor()));

	_scrollbar = new QScrollBar(this);
	connect(_scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scroll(int)));

	_colordlg = new Color_Dialog(this);
	_colordlg->setWindowTitle(tr("Set palette color"));

	connect(_colordlg, SIGNAL(colorSelected(QColor)),
			this, SLOT(setCurrentColor(QColor)));

	connect(_colordlg, SIGNAL(finished(int)),
			this, SLOT(dialogDone()));
}

void PaletteWidget::setPalette(Palette *palette)
{
	if(_palette) {
		// Disconnect previous palette
		disconnect(_palette, SIGNAL(colorsChanged()), this, SLOT(update()));
	}

	_palette = palette;
	if(palette) {
		_columns = palette->columns();
		connect(_palette, SIGNAL(colorsChanged()), this, SLOT(update()));
	} else {
		_columns = 1;
	}
	Q_ASSERT(_columns>0);
	_selection = -1;
	_dialogsel = -2;
	resizeEvent(0);
	update();
}

void PaletteWidget::setSpacing(int spacing)
{
	Q_ASSERT(spacing>=0);
	_spacing = spacing;
	resizeEvent(0);
	update();
}

QSize PaletteWidget::calcSwatchSize(int availableWidth) const
{
	QSize s;
	s.setWidth(qMax(1+_spacing*2, (availableWidth-_spacing)/_columns - _spacing));
	s.setHeight(s.width() * 0.75 + 0.5);
	return s;
}

namespace {
	int divRoundUp(int a, int b) {
		return a/b + (a%b ? 1 : 0);
	}
}
void PaletteWidget::resizeEvent(QResizeEvent*)
{
	int rowsHeight = 0;

	int contentWidth = width();

	if(_palette) {
		// First calculate required space without scrollbar
		_swatchsize = calcSwatchSize(contentWidth);
		rowsHeight = (_palette->count() / _columns + 1) * (_swatchsize.height()+_spacing);
	}

	if(rowsHeight <= height()) {
		_scrollbar->setVisible(false);
		_scrollbar->setMaximum(0);

	} else {
		// Recalculate width taking scrollbar in account
		contentWidth -= _scrollbar->sizeHint().width();
		_swatchsize = calcSwatchSize(contentWidth);
		rowsHeight = divRoundUp(_palette->count(), _columns) * (_swatchsize.height()+_spacing);

		_scrollbar->setGeometry(QRect(
				contentWidth,
				0,
				_scrollbar->sizeHint().width(),
				height()
				)
			);

		_scrollbar->setMaximum(rowsHeight - this->height() + 1);
		_scrollbar->setPageStep(height());
		_scrollbar->setVisible(true);
	}

	_scrollbar->setMinimum(0);

	_leftMargin = (contentWidth - _spacing - (_swatchsize.width()+_spacing) * _columns) / 2;

	if(_outline->isVisible())
		_outline->setGeometry(swatchRect(_selection).adjusted(-1,-1,1,1));
}

void PaletteWidget::scroll(int pos)
{
	_scroll = pos;
	if(_outline->isVisible())
		_outline->setGeometry(swatchRect(_selection).adjusted(-1,-1,1,1));

	update();
}

/**
 * Pop up a dialog to add a new color
 */
void PaletteWidget::addColor()
{
	if(_dialogsel<-1) {
		_dialogsel = -1;
		_colordlg->setColor(Qt::black);
		_colordlg->show();
	}
}

void PaletteWidget::removeColor()
{
	Q_ASSERT(_palette);
	_palette->removeColor(_selection);
	if(_selection >= _palette->count()) {
		_outline->hide();
		_selection = -1;
	}
	update();
}

/**
 * Pop up a dialog to edit the currently selected color
 */
void PaletteWidget::editCurrentColor()
{
	Q_ASSERT(_palette);
	if(_dialogsel<-1 && _selection >= 0) {
		_dialogsel = _selection;
		_colordlg->setColor(_palette->color(_selection).color);
		_colordlg->show();
	}
}

/**
 * Current color was set in the selection dialog.
 * If selection was the special value -1, add a new color.
 * @pre dialogsel >= -1 (a valid selection value)
 */
void PaletteWidget::setCurrentColor(const QColor& color)
{
	Q_ASSERT(_palette);
	Q_ASSERT(_dialogsel>-2);
	if(_dialogsel==-1) {
		if(_selection == -1)
			_selection = _palette->count();
		_palette->insertColor(_selection, color);
	} else {
		_palette->setColor(_selection, color);
	}
	update();
}

/**
 * Dialog was finished, mark dialog selection as unselected
 * so the dialog can be opened again.
 */
void PaletteWidget::dialogDone()
{
	_dialogsel = -2;
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
			const PaletteColor c = _palette->color(index);
			QToolTip::showText(
					mapToGlobal(pos),
					tr("%1\nRed: %2\nGreen: %3\nBlue: %4\nHex: %5")
						.arg(c.name)
						.arg(c.color.red())
						.arg(c.color.green())
						.arg(c.color.blue())
						.arg(c.color.name()),
					this,
					swatchRect(index)
					);
			event->accept();
			return true;
		}
	}
	return QWidget::event(event);
}

void PaletteWidget::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	//painter.fillRect(event->rect(), QColor("#646464"));

	if(!_palette || _palette->count()==0)
		return;

	painter.fillRect(
		QRectF(
			_leftMargin, 0,
			qMin(_columns, _palette->count()) * (_swatchsize.width() + _spacing) + _spacing,
			divRoundUp(_palette->count(), _columns) * (_swatchsize.height() + _spacing) + _spacing
		),
		QColor("#646464")
	);

	for(int i=0;i<_palette->count();++i) {
		QRect swatch = swatchRect(i);
		painter.fillRect(swatch, _palette->color(i).color);
	}
}

void PaletteWidget::mousePressEvent(QMouseEvent *event)
{
	_dragstart = event->pos();
	_selection = indexAt(event->pos());
	if(_selection!=-1) {
		_outline->setGeometry(swatchRect(_selection).adjusted(-1,-1,1,1));
		_outline->show();
	} else {
		_outline->hide();
	}
}

void PaletteWidget::contextMenuEvent(QContextMenuEvent *event)
{
	if(_palette && !_palette->isReadonly())
		_contextmenu->popup(mapToGlobal(event->pos()));
}

void PaletteWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(_selection != -1 && (event->buttons() & Qt::LeftButton) &&
			(event->pos() - _dragstart).manhattanLength() >
			QApplication::startDragDistance())
	{
		QDrag *drag = new QDrag(this);

		QMimeData *mimedata = new QMimeData;
		const QColor color = _palette->color(_selection).color;
		mimedata->setColorData(color);

		drag->setMimeData(mimedata);
		drag->start(Qt::CopyAction|Qt::MoveAction);
	}
}

void PaletteWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(_selection != -1) {
		if(event->button()==Qt::LeftButton)
			emit colorSelected(_palette->color(_selection).color);
	}
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	if(_palette && !_palette->isReadonly()) {
		if(_selection > -1)
			editCurrentColor();
		else
			addColor();
	}
}

void PaletteWidget::keyReleaseEvent(QKeyEvent *event)
{
	if(_selection != -1 && event->key() == Qt::Key_Delete)
		removeColor();
}

void PaletteWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color") && _palette != 0) {
		if(event->source() == this)
			event->setDropAction(Qt::MoveAction);
		event->accept();
		_outline->show();
	}
}

void PaletteWidget::dragMoveEvent(QDragMoveEvent *event)
{
	const int index = indexAt(event->pos(), true);
	if(index != -1) {
		_outline->setGeometry(swatchRect(index));
	} else {
		_outline->setGeometry(betweenRect(nearestAt(event->pos())).adjusted(-1,-1,1,1));
	}
}

void PaletteWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
	Q_UNUSED(event);
	if(_selection!=-1 && hasFocus()) {
		_outline->setGeometry(swatchRect(_selection).adjusted(-1,-1,1,1));
	} else {
		_outline->hide();
	}
}

void PaletteWidget::focusInEvent(QFocusEvent*)
{
	if(_selection!=-1)
		_outline->setGeometry(swatchRect(_selection).adjusted(-1,-1,1,1));
}

void PaletteWidget::focusOutEvent(QFocusEvent*)
{
	_outline->hide();
}

void PaletteWidget::dropEvent(QDropEvent *event)
{
	int index = indexAt(event->pos(), true);
	if(index != -1) {
		if(event->source() == this) {
			// Switch colors
			_palette->setColor(_selection, _palette->color(index));
		}
		_palette->setColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	} else {
		index = nearestAt(event->pos());
		if(event->source() == this) {
			// Move color
			_palette->removeColor(_selection);
			if(index >= _selection)
				--index;
		}
		_palette->insertColor(
				index,
				qvariant_cast<QColor>(event->mimeData()->colorData())
				);
	}
	_selection = index;
	update();
}

void PaletteWidget::wheelEvent(QWheelEvent *event)
{
	if(event->orientation() == Qt::Vertical) {
		_scrollbar->setValue(_scrollbar->value() - event->delta()/16);
	}
}

/**
 * @param point coordinates inside the widget
 * @param extrapadding if true, extra room is added between swatches
 * @return index number of color swatch at point
 * @retval -1 if point is outside any color swatch
 */
int PaletteWidget::indexAt(const QPoint& point, bool extraPadding) const
{
	if(!_palette)
		return -1;

	const int normalizedX = point.x() - _leftMargin - _spacing;
	const int ix = normalizedX / (_swatchsize.width() + _spacing);
	const int nearestX = ix * (_swatchsize.width() + _spacing);
	int padding = extraPadding ? 4 : 0;
	int x = normalizedX - nearestX;
	if(x < _spacing+padding || x> _swatchsize.width()+_spacing)
		return -1;

	const int yw = _spacing + _swatchsize.height();
	const int y = (point.y()+_scroll) / yw;
	if(point.y()+_scroll < y * yw + _spacing)
		return -1;

	const int index = (y * _columns) + ix;
	if(index >= _palette->count())
		return -1;

	return index;
}

/**
 * Return the index nearest to the point. If the position is after
 * the last palette entry, Palette::count() is returned.
 * @param point coordinates inside the widget
 * @return index of the color swatch nearest
 * @pre _palette != 0
 */
int PaletteWidget::nearestAt(const QPoint& point) const
{
	const int x = (point.x()-_leftMargin-_spacing) / (_spacing + _swatchsize.width());
	const int y = (point.y()+_scroll) / (_spacing + _swatchsize.height());

	return qMin(y * _columns + x, _palette->count());
}

/**
 * Get the position of the color swatch
 * @param index of the color swatch
 * @return swatch geometry
 * @pre 0 <= index
 */
QRect PaletteWidget::swatchRect(int index) const
{
	Q_ASSERT(index>=0);
	return QRect(
		_leftMargin + _spacing + (_swatchsize.width()+_spacing) * (index%_columns),
		_spacing + (_swatchsize.height()+_spacing) * (index/_columns) - _scroll,
		_swatchsize.width(),
		_swatchsize.height()
	);
}

/**
 * Get a rectangle between two color swatches.
 * @param index swatch index after the space
 * @return separator area between two swatches
 */
QRect PaletteWidget::betweenRect(int index) const
{
	return QRect(
			_leftMargin + _spacing/2 + (_swatchsize.width()+_spacing) * (index%_columns),
			_spacing + (_swatchsize.height()+_spacing) * (index/_columns) - _scroll,
			_spacing/2,
			_swatchsize.height()
			);
}

}

