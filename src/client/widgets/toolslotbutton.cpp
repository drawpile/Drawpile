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
}

void ToolSlotButton::setColors(const QColor &fg, const QColor &bg)
{
	_fg = fg;
	_bg = bg;
	update();
}

void ToolSlotButton::paintEvent(QPaintEvent *)
{
	QStylePainter p(this);
	QStyleOptionToolButton opt;
	initStyleOption(&opt);

	// Remove icon: we draw that ourself
	opt.icon = QIcon();
	opt.text = QString();

	p.drawComplexControl(QStyle::CC_ToolButton, opt);

	// Draw colors and icon
	static const int PADDING = 3;

	p.drawPixmap(QRect(
		width() / 2 - opt.iconSize.width()/2,
		PADDING,
		opt.iconSize.width(),
		opt.iconSize.height()
	), icon().pixmap(opt.iconSize));

	const int w = width() - PADDING*2;
	const int h = height() - PADDING*3 - opt.iconSize.height();
	const int x = PADDING;
	const int y = PADDING*2 + opt.iconSize.height();
	if(h>0) {
		p.fillRect(x, y, w, h, _bg);
		QPoint tri[] = {
			{x+1, y+1}, {x+w-1, y+1}, {x+1, y+h-1}
		};

		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(_fg);
		p.drawConvexPolygon(tri, sizeof(tri)/sizeof(QPoint));
	}
}

}
