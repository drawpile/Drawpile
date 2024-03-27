// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef USERMARKERITEM_H
#define USERMARKERITEM_H
#include "desktop/scene/baseitem.h"
#include <QBrush>
#include <QDeadlineTimer>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

namespace drawingboard {

class UserMarkerItem final : public BaseItem {
public:
	enum { Type = UserMarkerType };
	UserMarkerItem(int id, QGraphicsItem *parent = nullptr);

	int id() const { return m_id; }
	QRectF boundingRect() const override;
	int type() const override { return Type; }

	void setColor(const QColor &color);
	const QColor &color() const;

	void setText(const QString &text);
	void setShowText(bool show);

	void setSubtext(const QString &text);
	void setShowSubtext(bool show);

	void setAvatar(const QPixmap &avatar);
	void setShowAvatar(bool show);

	bool penUp() const { return m_penUp; }
	void setPenUp(bool penUp) { m_penUp = penUp; }
	bool clearPenUp()
	{
		if(m_penUp) {
			m_penUp = false;
			return true;
		} else {
			return false;
		}
	}

	void fadein();
	void fadeout();

	void setTargetPos(qreal x, qreal y, bool force);

	void animationStep(qreal dt);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *) override;

private:
	static constexpr int ARROW = 10;

	void updateFullText();
	int m_id;

	QPointF m_targetPos;
	QRectF m_bounds;
	QBrush m_bgbrush;
	QPen m_textpen;
	QPainterPath m_bubble;
	QRectF m_textRect;
	QRectF m_avatarRect;
	double m_fadeout = 0.0;
	QDeadlineTimer m_moveTimer;

	QString m_text1, m_text2;
	QString m_fulltext;
	QPixmap m_avatar;

	bool m_showText = true;
	bool m_showSubtext = false;
	bool m_showAvatar = true;
	bool m_penUp = false;
};

}

#endif
