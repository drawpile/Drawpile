/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#include "desktop/widgets/filmstrip.h"

#include <QDebug>
#include <QScrollBar>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <limits>

namespace widgets {

static const int MARGIN = 22;
static const int FRAME_MARGIN = 4;

Filmstrip::Filmstrip(QWidget *parent)
	: QWidget(parent), m_cursor(0), m_length(100)
{
	setMinimumHeight(MARGIN*4);
	m_scrollbar = new QScrollBar(Qt::Horizontal, this);
	m_scrollbar->resize(width(), 16);
	setFrames(1);
	connect(m_scrollbar, &QScrollBar::valueChanged, [this]() { this->update(); });
}

Filmstrip::~Filmstrip()
{
}

void Filmstrip::setLength(int len)
{
	m_length = len;
}

void Filmstrip::setFrames(size_t f)
{
	Q_ASSERT(f>0 && f<=size_t(std::numeric_limits<int>().max()));
	m_frames = int(f);
	m_scrollbar->setMaximum(qMax(0, (frameSize().width()+FRAME_MARGIN) * int(f) - width()));
}

void Filmstrip::setCursor(int c) {
	if(c != m_cursor && c>=0 && c <= m_length) {
		m_cursor = c;
		m_scrollbar->setValue(cursorPos() - width()/2);
		update();
	}
}

void Filmstrip::resizeEvent(QResizeEvent *e)
{ 
	Q_UNUSED(e);
	m_scrollbar->setGeometry(0, height() - m_scrollbar->height(), width(), m_scrollbar->height());
	m_scrollbar->setMaximum(qMax((frameSize().width()+FRAME_MARGIN) * m_frames - width() + FRAME_MARGIN*2, 0));
}

void Filmstrip::mouseDoubleClickEvent(QMouseEvent *e)
{
	const int x = e->pos().x() + m_scrollbar->value();
	emit doubleClicked(x / qreal((frameSize().width()+FRAME_MARGIN) * m_frames) * m_length);
}

void Filmstrip::wheelEvent(QWheelEvent *e)
{
	m_scrollbar->event(e);
}

void Filmstrip::paintEvent(QPaintEvent *event)
{
	const int w = width();
	const int h = height() - m_scrollbar->height();
	int x;

	if(!QRect(0, 0, w, h).intersects(event->rect()))
		return;

	QPainter painter(this);
	painter.setClipRect(event->rect());
	painter.fillRect(QRect(0, 0, w, h), QColor(35, 38, 41));

	// Paint the film perforations
	painter.setRenderHint(QPainter::Antialiasing);

	const int perfMargin = MARGIN / 4;
	const int perfInterval = MARGIN * 1.3;
	const qreal perfRound = 4;
	painter.setBrush(QColor(252, 252, 252));
	x = -m_scrollbar->value() % perfInterval;
	while(x < w) {
		QRectF perf(x + perfMargin, 2, MARGIN - 2*perfMargin, MARGIN - 4);
		painter.drawRoundedRect(perf, perfRound, perfRound);
		perf.translate(0, h - MARGIN);
		painter.drawRoundedRect(perf, perfRound, perfRound);
		x += perfInterval;
	}

	// Paint frames
	QSize fSize = frameSize();
	const int frameInterval = fSize.width() + FRAME_MARGIN;
	x = -m_scrollbar->value() % frameInterval;
	int frameIdx = m_scrollbar->value() / frameInterval;
	while(x < w && frameIdx < m_frames) {
		QRect frame(QPoint(x + FRAME_MARGIN, MARGIN), fSize);
		painter.drawRect(frame);
		painter.drawPixmap(frame, getFrame(frameIdx));
		x += frameInterval;
		++frameIdx;
	}

	// Paint position indicator
	if(m_length > 0) {
		x = cursorPos() - m_scrollbar->value();
		painter.setRenderHint(QPainter::Antialiasing, false);
		painter.setPen(Qt::black);
		painter.drawLine(x, MARGIN, x, h-MARGIN);
		painter.setPen(Qt::white);
		painter.drawLine(x+1, MARGIN, x+1, h-MARGIN);
	}
}

QSize Filmstrip::frameSize() const
{
	const int h = qMax(MARGIN, (height() - m_scrollbar->height()) - 2*MARGIN);
	const int w = h * 1.377;
	return QSize(w, h);
}

//! Get cursor position in pixels
int Filmstrip::cursorPos() const
{
	return m_cursor / qreal(m_length) * ((frameSize().width()+FRAME_MARGIN) * m_frames);
}

QPixmap Filmstrip::getFrame(int idx) const
{
	if(!m_cache.contains(idx)) {
		if(!m_loadimagefn)
			return QPixmap();
		m_cache.insert(idx, new QPixmap(QPixmap::fromImage(m_loadimagefn(idx))));
	}
	return *m_cache[idx];
}

}

