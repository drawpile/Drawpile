// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/colorpopup.h"
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

namespace widgets {

constexpr Qt::WindowFlags WINDOW_FLAGS =
#if defined(Q_OS_MACOS) || defined(Q_OS_ANDROID)
	Qt::Popup |
#else
	Qt::Window |
#endif
	Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint |
	Qt::BypassWindowManagerHint;

ColorPopup::ColorPopup(QWidget *parent)
	: QWidget(parent, WINDOW_FLAGS)
{
}

void ColorPopup::setSelectedColor(const QColor &selectedColor)
{
	QColor c = sanitizeColor(selectedColor);
	if(c != m_selectedColor) {
		m_selectedColor = c;
		update();
	}
}

void ColorPopup::setPreviousColor(const QColor &previousColor)
{
	QColor c = sanitizeColor(previousColor);
	if(c != m_previousColor) {
		m_previousColor = c;
		update();
	}
}

void ColorPopup::setLastUsedColor(const QColor &lastUsedColor)
{
	QColor c = sanitizeColor(lastUsedColor);
	if(c != m_lastUsedColor) {
		m_lastUsedColor = c;
		update();
	}
}

void ColorPopup::showPopup(
	QWidget *p, QWidget *windowOrNull, int yOffset, bool vcenter)
{
	Q_ASSERT(p);
	int size = qMax(100, qMin(p->width(), p->height()) / 2);
	QRect r = fitIntoWindow(
		getGlobalGeometry(p),
		windowOrNull ? getGlobalGeometry(windowOrNull) : getScreenGeometry(p),
		size, size, yOffset, vcenter);
	setGeometry(r);
	show();
}

void ColorPopup::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	int w = width();
	int h = height();
	int w1 = w / 2;
	int w2 = w - w1;
	int h1 = h / 2;
	int h2 = h - h1;
	QPainter painter(this);
	painter.fillRect(QRect(0, 0, w, h1), m_selectedColor);
	painter.fillRect(QRect(0, h1, w1, h2), m_previousColor);
	painter.fillRect(QRect(w1, h1, w2, h2), m_lastUsedColor);
}

QColor ColorPopup::sanitizeColor(QColor c)
{
	if(c.isValid() && c.alpha() > 0) {
		c.setAlpha(255);
		return c;
	} else {
		return Qt::black;
	}
}

QRect ColorPopup::fitIntoWindow(
	const QRect &pr, const QRect &wr, int w, int h, int yOffset, bool vcenter)
{
	int x;
	if(pr.left() - wr.left() < wr.right() - pr.right()) {
		x = pr.right() + 1;
	} else {
		x = pr.left() - w;
	}

	int y;
	int prTop = pr.top() + yOffset;
	if(prTop - wr.top() < wr.bottom() - pr.bottom()) {
		y = pr.bottom() + 1;
	} else {
		y = prTop - h;
	}

	if(vcenter) {
		if(QRect r(x, prTop + (pr.bottom() - prTop) / 2 - h / 2, w, h);
		   wr.contains(r)) {
			return r;
		}
	}
	if(QRect r(x, prTop, w, h); wr.contains(r)) {
		return r;
	}
	if(QRect r(x, pr.bottom() - h, w, h); wr.contains(r)) {
		return r;
	}
	if(QRect r(pr.left() + (pr.right() - pr.left()) / 2 - w / 2, y, w, h);
	   wr.contains(r)) {
		return r;
	}
	if(QRect r(pr.left(), y, w, h); wr.contains(r)) {
		return r;
	}
	if(QRect r(pr.right() - w, y, w, h); wr.contains(r)) {
		return r;
	}
	return QRect(x, y, w, h);
}

QRect ColorPopup::getGlobalGeometry(const QWidget *w)
{
	QRect r = w->rect();
	return QRect(w->mapToGlobal(r.topLeft()), w->mapToGlobal(r.bottomRight()));
}

QRect ColorPopup::getScreenGeometry(const QWidget *w)
{
	if(w) {
		QScreen *screen = w->screen();
		if(screen) {
			return screen->availableGeometry();
		}
	}
	return qApp->primaryScreen()->availableGeometry();
}

}
