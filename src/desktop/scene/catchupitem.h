// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_CATCHUPITEM_H
#define DESKTOP_CATCHUPITEM_H
#include <QGraphicsItem>

namespace drawingboard {

class CatchupItem final : public QGraphicsItem {
public:
	enum { Type = UserType + 17 };

	explicit CatchupItem(const QString &text, QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setCatchupProgress(int percent);

	bool animationStep(qreal dt);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	void updateBarText();
	void updateBounds();

	QRectF m_textBounds;
	QRectF m_barBounds;
	QRectF m_bounds;
	QString m_text;
	QString m_barText;
	int m_percent;
	qreal m_fade;
};

}

#endif
