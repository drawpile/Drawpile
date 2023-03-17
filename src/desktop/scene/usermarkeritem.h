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
#ifndef USERMARKERITEM_H
#define USERMARKERITEM_H

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPixmap>
#include <QBrush>
#include <QPen>

namespace drawingboard {

class UserMarkerItem final : public QGraphicsItem
{
public:
	enum { Type = UserType + 12 };

	UserMarkerItem(int id, QGraphicsItem *parent=nullptr);

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

	void fadein();
	void fadeout();

	void setTargetPos(qreal x, qreal y, bool force);

	void animationStep(double dt);

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *) override;

private:
	void updateFullText();
	int m_id;

	QPointF m_targetPos;
	QRectF m_bounds;
	QBrush m_bgbrush;
	QPen m_textpen;
	QPainterPath m_bubble;
	QRectF m_textRect;
	QRectF m_avatarRect;
	double m_fadeout;
	qint64 m_lastMoved;

	QString m_text1, m_text2;
	QString m_fulltext;
	QPixmap m_avatar;

	bool m_showText;
	bool m_showSubtext;
	bool m_showAvatar;
};

}

#endif // USERMARKERITEM_H
