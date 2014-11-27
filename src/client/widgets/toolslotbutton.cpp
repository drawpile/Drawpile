/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "toolslotbutton.h"

#include <QStylePainter>
#include <QStyleOptionToolButton>

namespace widgets {

ToolSlotButton::ToolSlotButton(QWidget *parent) :
	QToolButton(parent), _isHovering(false)
{
	//setCursor(Qt::PointingHandCursor);
}

void ToolSlotButton::setColors(const QColor &fg, const QColor &bg)
{
	_fg = fg;
	_bg = bg;
	update();
}

void ToolSlotButton::setHighlightColor(const QColor &c)
{
	_highlight = c;
	update();
}

void ToolSlotButton::setHoverColor(const QColor &c)
{
	_hover = c;
	update();
}

void ToolSlotButton::paintEvent(QPaintEvent *)
{
	//const int UNDERLINE = 3;
	const int RADIUS = qMin(width(), height())/2 - 4;
	QRectF rect(
		(width()/2 - RADIUS) + 0.5,
		(height()/2 - RADIUS) + 0.5,// - UNDERLINE + 1,
		RADIUS*2,
		RADIUS*2
	);
	QPainter p(this);

	// Draw background tab (if selected)
	if(isChecked() || _isHovering) {
		const int R = 3;
		p.setPen(Qt::NoPen);
		p.setBrush(isChecked() ? _highlight : _hover);
		p.drawRoundedRect(QRectF(0, 0, width(), height()+R), R, R);
	}

	// Draw foreground and background colors
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(Qt::NoPen);

	p.setBrush(_bg);
	p.drawEllipse(rect.translated(2, 2));
	p.setBrush(_fg);
	p.drawEllipse(rect.translated(-1, -1));

	// Draw icon
	int iconSize = qMin(rect.width(), rect.height()) * 0.5 * 1.414;
	QPixmap pixmap = icon().pixmap(iconSize, iconSize);
	p.drawPixmap(QRect(
		rect.left() + (rect.width() - pixmap.width())/2,
		rect.top() + (rect.height() - pixmap.height())/2,
		pixmap.width(),
		pixmap.height()
	), pixmap);

#if 0
	// Draw outline
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(Qt::NoBrush);
	p.setPen(QColor(0, 0, 0, 128));
	p.drawEllipse(rect);
#endif
}

void ToolSlotButton::enterEvent(QEvent *e)
{
	_isHovering = true;
	QToolButton::enterEvent(e);
}

void ToolSlotButton::leaveEvent(QEvent *e)
{
	_isHovering = false;
	QToolButton::leaveEvent(e);
}

}
