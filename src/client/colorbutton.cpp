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
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "colorbutton.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

ColorButton::ColorButton(QWidget *parent,const QColor& color)
	: QWidget(parent), color_(color), isdown_(false)
{
	setAcceptDrops(true);
}

void ColorButton::setColor(const QColor& color)
{
	color_ = color;
	update();
}

/**
 * Draw widget contents on screen
 * @param event event info
 */
void ColorButton::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);

	QStyleOptionButton option;
	option.initFrom(this);
	option.state = isdown_ ? QStyle::State_Sunken : QStyle::State_Raised;

	painter.drawControl(QStyle::CE_PushButtonBevel,option);
	const int adj = qMin(width(),height()) / 5;
	QRect rect = contentsRect().adjusted(adj, adj, -adj, -adj);
	painter.fillRect(rect, color_);
}

/**
 * @brief Handle mouse press
 * @param event event info
 */
void ColorButton::mousePressEvent(QMouseEvent *)
{
	isdown_ = true;
	update();
}

/**
 * @brief Handle mouse release
 * @param event event info
 */
void ColorButton::mouseReleaseEvent(QMouseEvent *)
{
	isdown_ = false;
	update();
#ifndef DESIGNER_PLUGIN
	bool ok;
	const QRgb col = QColorDialog::getRgba(color_.rgba(), &ok, this);
	if(ok && col != color_.rgba()) {
		setColor(QColor::fromRgba(col));
		emit colorChanged(color_);
	}
#endif
}

/**
 * @brief accept color drops
 * @param event event info
 */
void ColorButton::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color"))
	event->acceptProposedAction();
}

/**
 * @brief handle color drops
 * @param event event info
 */
void ColorButton::dropEvent(QDropEvent *event)
{
	const QColor col = qvariant_cast<QColor>(event->mimeData()->colorData());
	setColor(col);
	emit colorChanged(col);
}

#ifndef DESIGNER_PLUGIN
}
#endif
