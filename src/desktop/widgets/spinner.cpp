/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "desktop/widgets/spinner.h"

#include <QPainter>
#include <QtMath>

namespace widgets {

Spinner::Spinner(QWidget *parent):
	QWidget(parent), m_dots(8), m_currentDot(0)
{
	startTimer(150);
}

void Spinner::paintEvent(QPaintEvent *)
{
	const int RADIUS = qMin(width(), height()) / 2;
	const int DOT_SIZE = (2 * M_PI * RADIUS) / (2 + m_dots*2);

	QPainter painter(this);

	painter.translate(width()/2, height()/2);
	painter.setPen(Qt::NoPen);
	painter.setRenderHint(QPainter::Antialiasing);

	for(int dot=0;dot<m_dots;++dot) {
		painter.setBrush(dot == m_currentDot ? palette().dark() : palette().mid());
		painter.drawEllipse(QRect { -DOT_SIZE/2, -RADIUS+DOT_SIZE/2, DOT_SIZE, DOT_SIZE });
		painter.rotate(360.0 / m_dots);
	}
}

void Spinner::timerEvent(QTimerEvent *)
{
	++m_currentDot;
	if(m_currentDot >= m_dots)
		m_currentDot = 0;
	update();
}

}

