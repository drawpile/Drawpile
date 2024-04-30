// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RULERWIDGET_H
#define RULERWIDGET_H

#include <QWidget>

#include <cmath>

namespace widgets {

class RulerWidget final : public QWidget {
	Q_OBJECT
public:
	explicit RulerWidget(QWidget *parent = nullptr);

	void setIsRotated(bool isRotated);
	void setViewBounds(const QRectF &viewBounds);
	// Maps from a pixel on the canvas to a pixel on the ruler.
	void setCanvasToRulerTransform(qreal scale, const QRectF &viewBounds);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	int longSize() const { return std::max(width(), height()); }
	int shortSize() const { return std::min(width(), height()); }
	bool isHorizontal() const { return width() > height(); }

	QPoint adjustPoint(int longPos, int shortPos)
	{
		if(isHorizontal())
			return {longPos, shortPos};
		return {shortPos, longPos};
	}

	int canvasToRuler(qreal pos)
	{
		return std::lround((pos - m_lMin) * m_canvasToRulerScale);
	}

	qreal m_lMin = 0.;
	qreal m_lMax = 0.;
	qreal m_canvasToRulerScale = 1.;
	bool m_isRotated = false;
};

} // namespace widgets

#endif // RULERWIDGET_H
