/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "thinsrv/gui/subheaderwidget.h"

#include <QPainter>

namespace server {
namespace gui {

SubheaderWidget::SubheaderWidget(const QString &text, int level, QWidget *parent)
	: QLabel(text, parent)
{
	QFont f = font();
	f.setBold(true);

	if(level<=1)
		f.setPointSizeF(f.pointSizeF()*2);
	else if(level==2)
		f.setPointSizeF(f.pointSizeF()*1.3);

	setFont(f);
	setContentsMargins(5, 5, 5, 5);
}

void SubheaderWidget::paintEvent(QPaintEvent *e)
{
	QLabel::paintEvent(e);
	QPainter p(this);
	p.setPen(palette().color(QPalette::Highlight));
	p.drawLine(0, height()-3, width(), height()-3);
}

}
}
