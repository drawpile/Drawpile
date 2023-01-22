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

#include "tablettest.h"
#include "utils/qtguicompat.h"

#include <QPaintEvent>
#include <QPainter>

namespace widgets {

TabletTester::TabletTester(QWidget *parent)
	: QWidget(parent), m_mouseDown(false), m_tabletDown(false)
{
}

void TabletTester::clear()
{
	m_mousePath.clear();
	m_tabletPath.clear();
	update();
}

void TabletTester::paintEvent(QPaintEvent *e)
{
	Q_UNUSED(e);
	const int w = width();
	const int h = height();
	QPainter p(this);
	p.fillRect(0, 0, w, h, QColor(200, 200, 200));
	p.setPen(QColor(128, 128, 128));

	// Draw grid
	for(int i=w/8;i<w;i+=w/8)
		p.drawLine(i, 0, i, h);
	for(int i=h/8;i<h;i+=h/8)
		p.drawLine(0, i, w, i);

	// Draw paths
	if(!m_mousePath.isEmpty()) {
		p.setPen(QPen(Qt::red, 3));
		p.drawPolyline(m_mousePath);
	}
	if(!m_tabletPath.isEmpty()) {
		p.setPen(QPen(Qt::blue, 2));
		p.drawPolyline(m_tabletPath);
	}
}

void TabletTester::mousePressEvent(QMouseEvent *e)
{
	const auto mousePos = compat::mousePos(*e);
	emit eventReport(QString("Mouse press X=%1 Y=%2 B=%3").arg(mousePos.x()).arg(mousePos.y()).arg(e->button()));
	m_mouseDown = true;
	m_mousePath.clear();
	update();
}

void TabletTester::mouseMoveEvent(QMouseEvent *e)
{
	const auto mousePos = compat::mousePos(*e);
	emit eventReport(QString("Mouse move X=%1 Y=%2 B=%3").arg(mousePos.x()).arg(mousePos.y()).arg(e->buttons()));
	m_mousePath << e->pos();
	update();
}

void TabletTester::mouseReleaseEvent(QMouseEvent *e)
{
	Q_UNUSED(e);
	emit eventReport(QString("Mouse release"));
	m_mouseDown = false;
}

void TabletTester::tabletEvent(QTabletEvent *e)
{
	e->accept();

	QString msg;

	auto device = compat::tabDevice(*e);

	switch(device) {
		case compat::DeviceType::Stylus: msg = "Stylus"; break;
		default: {
			msg = QString("Device(%1)").arg(int(device));
			break;
		}
	}

	switch(e->type()) {
		case QEvent::TabletMove:
			msg += " move";
			break;
		case QEvent::TabletPress:
			msg += " press";
			m_tabletPath.clear();
			m_tabletDown = true;
			break;
		case QEvent::TabletRelease:
			msg += " release";
			m_tabletDown = false;
			break;
		default:
			msg += QString(" event-%1").arg(e->type());
			break;
	}

	const auto posF = compat::tabPosF(*e);
	msg += QString(" X=%1 Y=%2 B=%3 P=%4%")
		.arg(posF.x(), 0, 'f', 2)
		.arg(posF.y(), 0, 'f', 2)
		.arg(e->buttons())
		.arg(e->pressure()*100, 0, 'f', 1)
		;

	if(e->type() == QEvent::TabletMove) {
		if(m_tabletDown) {
			msg += " (DRAW)";
			m_tabletPath << compat::tabPosF(*e).toPoint();
			update();
		} else {
			msg += " (HOVER)";
		}
	}

	emit eventReport(msg);
}

}

