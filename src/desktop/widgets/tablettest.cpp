// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/tablettest.h"
#include "desktop/utils/qtguicompat.h"
#include <QPaintEvent>
#include <QPainter>

namespace widgets {

TabletTester::TabletTester(QWidget *parent)
	: QWidget(parent)
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
	for(int i = w / 8; i < w; i += w / 8)
		p.drawLine(i, 0, i, h);
	for(int i = h / 8; i < h; i += h / 8)
		p.drawLine(0, i, w, i);

	p.setPen(Qt::NoPen);
	p.setBrush(Qt::red);
	for(const QPointF &point : m_mousePath) {
		p.drawEllipse(point, 3.0, 3.0);
	}

	for(const canvas::Point &point : m_tabletPath) {
		// Timestamp is 1 for a spontaneous (system) event and 0 for a
		// non-spontaneous (application-induced, probably by KisTablet) event.
		p.setBrush(point.timeMsec() == 0 ? Qt::blue : Qt::darkGreen);
		qreal radius = point.pressure() * 5.0;
		p.drawEllipse(point, radius, radius);
	}
}

void TabletTester::mousePressEvent(QMouseEvent *e)
{
	const auto mousePos = compat::mousePos(*e);
	emit eventReport(QString("Mouse press X=%1 Y=%2 B=%3")
						 .arg(mousePos.x())
						 .arg(mousePos.y())
						 .arg(e->button()));
	m_mouseDown = true;
	m_mousePath.clear();
	update();
}

void TabletTester::mouseMoveEvent(QMouseEvent *e)
{
	const auto mousePos = compat::mousePos(*e);
	emit eventReport(QString("Mouse move X=%1 Y=%2 B=%3")
						 .arg(mousePos.x())
						 .arg(mousePos.y())
						 .arg(e->buttons()));
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
	case compat::DeviceType::Stylus:
		msg = "Stylus";
		break;
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
	msg += QString(" X=%1 Y=%2 B=%3 P=%4% %5")
			   .arg(posF.x(), 0, 'f', 2)
			   .arg(posF.y(), 0, 'f', 2)
			   .arg(e->buttons())
			   .arg(e->pressure() * 100, 0, 'f', 1)
			   .arg(e->spontaneous() ? "SYS" : "APP");

	if(e->type() == QEvent::TabletMove) {
		if(m_tabletDown) {
			msg += " (DRAW)";
			m_tabletPath << canvas::Point(
				e->spontaneous() ? 1 : 0, compat::tabPosF(*e), e->pressure(),
				e->xTilt(), e->yTilt(), e->rotation());
			update();
		} else {
			msg += " (HOVER)";
		}
	}

	emit eventReport(msg);
}

}
