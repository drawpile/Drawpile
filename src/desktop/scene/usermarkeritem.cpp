/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2021 Calle Laakkonen

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

#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QDateTime>

#include "usermarkeritem.h"

namespace drawingboard {


namespace {
static const int ARROW = 10;

}
UserMarkerItem::UserMarkerItem(int id, QGraphicsItem *parent)
	: QGraphicsItem(parent),
	  m_id(id), m_fadeout(0), m_lastMoved(0),
	  m_showText(true), m_showSubtext(false), m_showAvatar(true)
{
	setFlag(ItemIgnoresTransformations);
	m_bgbrush.setStyle(Qt::SolidPattern);
	QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect;
	shadow->setOffset(0);
	shadow->setBlurRadius(10);
	setGraphicsEffect(shadow);
	setZValue(9999);
	setColor(Qt::black);
}

void UserMarkerItem::setColor(const QColor &color)
{
	m_bgbrush.setColor(color);

	if ((color.red() * 30) + (color.green() * 59) + (color.blue() * 11) > 12800)
		m_textpen = QPen(Qt::black);
	else
		m_textpen = QPen(Qt::white);

	update();
}

const QColor &UserMarkerItem::color() const
{
	return m_bgbrush.color();
}

void UserMarkerItem::setText(const QString &text)
{
	if(m_text1 != text) {
		m_text1 = text;
		if(m_showText)
			updateFullText();
	}
}

void UserMarkerItem::setSubtext(const QString &text)
{
	if(m_text2 != text) {
		m_text2 = text;
		if(m_showSubtext)
			updateFullText();
	}
}

void UserMarkerItem::setAvatar(const QPixmap &avatar)
{
	m_avatar = avatar;
	if(m_showAvatar)
		updateFullText();
}

void UserMarkerItem::setShowText(bool show)
{
	if(m_showText != show) {
		m_showText = show;
		updateFullText();
	}
}

void UserMarkerItem::setShowSubtext(bool show)
{
	if(m_showSubtext != show) {
		m_showSubtext = show;
		updateFullText();
	}
}

void UserMarkerItem::setShowAvatar(bool show) {
	if(m_showAvatar != show) {
		m_showAvatar = show;
		updateFullText();
	}
}

void UserMarkerItem::updateFullText()
{
	prepareGeometryChange();

	m_fulltext = m_showText ? m_text1 : QString();

	if(m_showSubtext && !m_text2.isEmpty()) {
		if(!m_fulltext.isEmpty())
			m_fulltext += QStringLiteral("\n[");
		m_fulltext += m_text2;
		m_fulltext += ']';
	}

	// Make a new bubble for the text and avatar
	const QRect textrect = m_fulltext.isEmpty() ? QRect() : QFontMetrics(QFont()).boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_fulltext);
	const bool showAvatar = m_showAvatar && !m_avatar.isNull();

	const qreal round = 3;
	const qreal padding = 5;
	const qreal width = qMax(qMax((ARROW+round)*2, textrect.width() + 2*padding), showAvatar ? m_avatar.width() + 2*padding : 0);
	const qreal rad = width / 2.0;
	const qreal avatarHeight = showAvatar ? m_avatar.height() + (textrect.height()>0 ? padding : 0) : 0;
	const qreal height = textrect.height() + avatarHeight + ARROW + 2 * padding;

	m_bounds = QRectF(-rad, -height, width, height);

	m_avatarRect = QRectF(
		m_bounds.width()/2 + m_bounds.left() - m_avatar.width() / 2,
		m_bounds.top() + padding,
		m_avatar.width(),
		m_avatar.height()
		);
	m_textRect = m_bounds.adjusted(padding, padding + avatarHeight, -padding, -padding);

	m_bubble = QPainterPath(QPointF(0, 0));

	m_bubble.lineTo(-ARROW, -ARROW);
	m_bubble.lineTo(-rad+round, -ARROW);

	m_bubble.quadTo(-rad, -ARROW, -rad, -ARROW-round);
	m_bubble.lineTo(-rad, -height+round);
	m_bubble.quadTo(-rad, -height, -rad+round, -height);

	m_bubble.lineTo(rad-round, -height);
	m_bubble.quadTo(rad, -height, rad, -height+round);
	m_bubble.lineTo(rad, -ARROW-round);

	m_bubble.quadTo(rad, -ARROW, rad-round, -ARROW);
	m_bubble.lineTo(ARROW, -ARROW);

	m_bubble.closeSubpath();
}

QRectF UserMarkerItem::boundingRect() const
{
	return m_bounds;
}

void UserMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(Qt::NoPen);

	painter->setBrush(m_bgbrush);
	painter->drawPath(m_bubble);

	painter->setFont(qApp->font());
	painter->setPen(m_textpen);
	painter->drawText(m_textRect, Qt::AlignHCenter|Qt::AlignTop, m_fulltext);

	if(!m_avatar.isNull() && m_showAvatar)
		painter->drawPixmap(m_avatarRect.topLeft(), m_avatar);
}

void UserMarkerItem::fadein()
{
	m_fadeout = 0;
	setOpacity(1);
	show();
	m_lastMoved = QDateTime::currentMSecsSinceEpoch();
}

void UserMarkerItem::fadeout()
{
	m_fadeout = 1.0;
}

void UserMarkerItem::setTargetPos(qreal x, qreal y, bool force)
{
	m_targetPos = QPointF(x, y);
	if(force || !isVisible()) {
		setPos(x, y);
	}
}

void UserMarkerItem::animationStep(qreal dt)
{
	if(isVisible()) {
		// Smoothing to avoid crazy jerking with spread out MyPaint brushes.
		setPos(QLineF(pos(), m_targetPos).pointAt(dt * 10.0));
		if(m_fadeout>0) {
			m_fadeout -= dt;
			if(m_fadeout <= 0.0) {
				hide();
			} else if(m_fadeout < 1.0)
				setOpacity(m_fadeout);
		} else if(m_lastMoved < QDateTime::currentMSecsSinceEpoch() - 1000) {
			fadeout();
		}
	}
}

}
