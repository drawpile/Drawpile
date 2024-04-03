// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/usermarkeritem.h"
#include <QApplication>
#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <cmath>

namespace drawingboard {

UserMarkerItem::UserMarkerItem(int id, int persistence, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_id(id)
	, m_persistence(qMax(-1, persistence))
{
	setFlag(ItemIgnoresTransformations);
	m_bgbrush.setStyle(Qt::SolidPattern);
	setZValue(9999);
	setColor(Qt::black);
}

void UserMarkerItem::setPersistence(int persistence)
{
	int p = qMax(-1, persistence);
	if(p != m_persistence) {
		if(p < 0) {
			m_moveTimer.setRemainingTime(-1);
			// Resurrect faded markers, since the user wants no fading at all.
			m_fadeout = 0.0;
			setOpacity(1.0);
			show();
			refresh();
		} else if(m_persistence < 0) {
			m_moveTimer.setRemainingTime(0);
		} else {
			int remainingTime = m_moveTimer.remainingTime();
			if(remainingTime > 0) {
				int delta = p - m_persistence;
				m_moveTimer.setRemainingTime(qMax(0, remainingTime + delta));
			}
		}
		m_persistence = p;
	}
}

void UserMarkerItem::setColor(const QColor &color)
{
	m_bgbrush.setColor(color);
	m_textpen = QPen(color.lightness() < 128 ? Qt::white : Qt::black);
	refresh();
}

const QColor &UserMarkerItem::color() const
{
	return m_bgbrush.color();
}

void UserMarkerItem::setText(const QString &text)
{
	if(m_text1 != text) {
		m_text1 = text;
		if(m_showText) {
			updateFullText();
		}
	}
}

void UserMarkerItem::setSubtext(const QString &text)
{
	if(m_text2 != text) {
		m_text2 = text;
		if(m_showSubtext) {
			updateFullText();
		}
	}
}

void UserMarkerItem::setAvatar(const QPixmap &avatar)
{
	m_avatar = avatar;
	if(m_showAvatar) {
		updateFullText();
	}
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

void UserMarkerItem::setShowAvatar(bool show)
{
	if(m_showAvatar != show) {
		m_showAvatar = show;
		updateFullText();
	}
}

void UserMarkerItem::setEvadeCursor(bool evadeCursor)
{
	if(m_evadeCursor != evadeCursor) {
		updateCursor([&]() {
			m_evadeCursor = evadeCursor;
		});
	}
}

void UserMarkerItem::setCursorPosValid(bool cursorPosValid)
{
	if(m_cursorPosValid != cursorPosValid) {
		updateCursor([&]() {
			m_cursorPosValid = cursorPosValid;
		});
	}
}

void UserMarkerItem::setCursorPos(const QPointF &cursorPos)
{
	if(m_cursorPos != cursorPos) {
		updateCursor([&]() {
			m_cursorPos = cursorPos;
		});
	}
}

void UserMarkerItem::updateFullText()
{
	refreshGeometry();

	m_fulltext = m_showText ? m_text1 : QString();

	if(m_showSubtext && !m_text2.isEmpty()) {
		if(!m_fulltext.isEmpty()) {
			m_fulltext += QStringLiteral("\n[");
		}
		m_fulltext += m_text2;
		m_fulltext += ']';
	}

	// Make a new bubble for the text and avatar
	const QRect textrect =
		m_fulltext.isEmpty() ? QRect()
							 : QFontMetrics(QFont()).boundingRect(
								   QRect(0, 0, 0xffff, 0xffff), 0, m_fulltext);
	const bool showAvatar = m_showAvatar && !m_avatar.isNull();

	const qreal round = 3;
	const qreal padding = 5;
	const qreal width = qMax(
		qMax((ARROW + round) * 2, textrect.width() + 2 * padding),
		showAvatar ? m_avatar.width() + 2 * padding : 0);
	const qreal rad = width / 2.0;
	const qreal avatarHeight =
		showAvatar ? m_avatar.height() + (textrect.height() > 0 ? padding : 0)
				   : 0;
	const qreal height = textrect.height() + avatarHeight + ARROW + 2 * padding;

	m_bounds = QRectF(-rad, -height, width, height);

	m_avatarRect = QRectF(
		m_bounds.width() / 2 + m_bounds.left() - m_avatar.width() / 2,
		m_bounds.top() + padding, m_avatar.width(), m_avatar.height());
	m_textRect =
		m_bounds.adjusted(padding, padding + avatarHeight, -padding, -padding);

	m_bubble = QPainterPath(QPointF(0, 0));

	m_bubble.lineTo(-ARROW, -ARROW);
	m_bubble.lineTo(-rad + round, -ARROW);

	m_bubble.quadTo(-rad, -ARROW, -rad, -ARROW - round);
	m_bubble.lineTo(-rad, -height + round);
	m_bubble.quadTo(-rad, -height, -rad + round, -height);

	m_bubble.lineTo(rad - round, -height);
	m_bubble.quadTo(rad, -height, rad, -height + round);
	m_bubble.lineTo(rad, -ARROW - round);

	m_bubble.quadTo(rad, -ARROW, rad - round, -ARROW);
	m_bubble.lineTo(ARROW, -ARROW);

	m_bubble.closeSubpath();
}

QRectF UserMarkerItem::boundingRect() const
{
	return m_bounds;
}

void UserMarkerItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	qreal evadeScale = getEvadeScale();
	if(evadeScale > 0.0) {
		if(evadeScale < 1.0) {
			QTransform tf = painter->transform();
			tf.scale(evadeScale, evadeScale);
			painter->setTransform(tf);
		}

		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);

		painter->setBrush(m_bgbrush);
		painter->drawPath(m_bubble);

		painter->setFont(qApp->font());
		painter->setPen(m_textpen);
		painter->drawText(
			m_textRect, Qt::AlignHCenter | Qt::AlignTop, m_fulltext);

		if(!m_avatar.isNull() && m_showAvatar) {
			painter->drawPixmap(m_avatarRect.topLeft(), m_avatar);
		}
	}
}

void UserMarkerItem::fadein()
{
	m_moveTimer.setRemainingTime(m_persistence);
	if(m_fadeout > 0.0 || !isVisible()) {
		m_fadeout = 0.0;
		setOpacity(1.0);
		show();
		refresh();
	}
}

void UserMarkerItem::fadeout()
{
	m_fadeout = 1.0;
}

void UserMarkerItem::setTargetPos(qreal x, qreal y, bool force)
{
	m_targetPos = QPointF(x, y);
	if(force || !isVisible()) {
		updatePosition(m_targetPos);
	}
}

void UserMarkerItem::animationStep(qreal dt)
{
	if(isVisible()) {
		// Smoothing to avoid crazy jerking with spread out MyPaint brushes.
		updatePosition(
			QLineF(pos(), m_targetPos).pointAt(qMin(dt * 20.0, 1.0)));
		if(m_fadeout > 0.0) {
			m_fadeout -= dt;
			if(m_fadeout <= 0.0) {
				hide();
			} else if(m_fadeout < 1.0) {
				setOpacity(m_fadeout);
			}
			refresh();
		} else if(m_moveTimer.hasExpired()) {
			fadeout();
		}
	}
}

void UserMarkerItem::updateCursor(const std::function<void()> &block)
{
	qreal scaleBefore = getEvadeScale();
	block();
	if(getEvadeScale() != scaleBefore) {
		refresh();
	}
}

qreal UserMarkerItem::getEvadeScale()
{
	if(m_evadeCursor && m_cursorPosValid) {
		QRectF r = m_bounds.translated(scenePos());
		qreal px = m_cursorPos.x();
		qreal py = m_cursorPos.y();
		qreal dx = qMax(0.0, qMax(r.left() - px, px - r.right()));
		qreal dy = qMax(0.0, qMax(r.top() - py, py - r.bottom()));
		qreal d = std::sqrt(dx * dx + dy * dy);
		if(d <= EVADE_HIDE) {
			return 0;
		} else if(d < EVADE_FADE) {
			return (d - EVADE_HIDE) / (EVADE_FADE - EVADE_HIDE);
		}
	}
	return 1.0;
}

}
