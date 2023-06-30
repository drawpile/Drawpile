// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_NOTICEITEM_H
#define DESKTOP_NOTICEITEM_H

#include <QGraphicsItem>

namespace drawingboard {

class NoticeItem final : public QGraphicsItem {
public:
	enum { Type = UserType + 15 };

	NoticeItem(const QString &text, QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setText(const QString &text);

    void setPersist(qreal seconds);

    bool animationStep(qreal dt);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
    static constexpr qreal FADEOUT = 0.1;

	void updateBounds();

	QRectF m_textBounds;
	QRectF m_bounds;
	QString m_text;
    qreal m_persist;
};

}

#endif
