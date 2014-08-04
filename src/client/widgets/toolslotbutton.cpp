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
	QToolButton(parent)
{
	setCursor(Qt::PointingHandCursor);
}

void ToolSlotButton::setColors(const QColor &fg, const QColor &bg)
{
	_fg = fg;
	_bg = bg;
	update();
}

void ToolSlotButton::paintEvent(QPaintEvent *)
{
	const int UNDERLINE = 3;
	const int RADIUS = qMin(width(), height()-UNDERLINE) - 2;
	QRectF rect(
		(width() - RADIUS) / 2 + 0.5,
		(height() - RADIUS) / 2 + 0.5 - UNDERLINE + 1,
		RADIUS,
		RADIUS
	);
	QPainter p(this);

	// Draw foreground and background colors
	p.setPen(Qt::NoPen);
	p.setBrush(_fg);
	p.drawChord(rect, 45*16, 180*16);

	p.setBrush(_bg);
	p.drawChord(rect, 225*16, 180*16);

	// Draw icon
	int iconSize = qMin(rect.width(), rect.height()) * 0.5 * 1.414;
	QPixmap pixmap = icon().pixmap(iconSize, iconSize);
	p.drawPixmap(QRect(
		rect.left() + (rect.width() - pixmap.width())/2,
		rect.top() + (rect.height() - pixmap.height())/2,
		pixmap.width(),
		pixmap.height()
	), pixmap);

	// Draw outline
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(Qt::NoBrush);
	p.setPen(QColor(0, 0, 0, 128));
	p.drawEllipse(rect);

	// Draw selection indicator
	if(isChecked()) {
		p.setRenderHint(QPainter::Antialiasing, false);
		p.setPen(Qt::NoPen);
		p.setBrush(palette().brush(QPalette::Highlight));
		p.drawRect(0, height() - UNDERLINE, width(), UNDERLINE);
	}
}

}
