// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_ARTISTICCOLORWHEEL_H
#define DESKTOP_WIDGETS_ARTISTICCOLORWHEEL_H
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>
#include <QtColorWidgets/ColorWheel>

namespace widgets {

class ArtisticColorWheel : public QWidget {
	Q_OBJECT
public:
	using ColorSpace = color_widgets::ColorWheel::ColorSpaceEnum;
	static constexpr int HUE_COUNT_MIN = 2;
	static constexpr int HUE_COUNT_DEFAULT = 12;
	static constexpr int HUE_COUNT_MAX = 48;
	static constexpr int SATURATION_COUNT_MIN = 2;
	static constexpr int SATURATION_COUNT_DEFAULT = 8;
	static constexpr int SATURATION_COUNT_MAX = 48;
	static constexpr int VALUE_COUNT_MIN = 2;
	static constexpr int VALUE_COUNT_DEFAULT = 10;
	static constexpr int VALUE_COUNT_MAX = 32;

	explicit ArtisticColorWheel(QWidget *parent = nullptr);

	void setHueLimit(bool hueLimit);
	void setHueCount(int hueCount);
	void setHueAngle(int hueAngle);
	void setSaturationLimit(bool saturationLimit);
	void setSaturationCount(int saturationCount);
	void setValueLimit(bool valueLimit);
	void setValueCount(int valueCount);

	ColorSpace colorSpace() const { return m_colorSpace; }
	void setColorSpace(ColorSpace colorSpace);

	const QColor &color() const { return m_color; }
	void setColor(const QColor &color);

signals:
	void colorSelected(const QColor &color);
	void editingStarted();
	void editingFinished();

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	enum class Region { None, Bar, Wheel };

	QRect valueBarRect() const;
	QRect wheelRect() const;
	void handleMouseOnBar(const QRectF &vr, const QPointF &posf);
	void handleMouseOnWheel(qreal radius, qreal length, qreal angle);
	void updateBarCache(const QSize &size);
	void updateWheelCache(int dimension);
	void updatePathCache(const QRectF &vr, const QRectF &wr);

	qreal getHueAt(qreal angle) const;
	qreal getSaturationAt(qreal n) const;
	qreal getValueAt(qreal n) const;
	qreal getValueSection(int i) const;

	QColor getColor(qreal h, qreal s, qreal v) const;

	static QColor
	getColorInColorSpace(qreal h, qreal s, qreal v, ColorSpace colorSpace);
	void setColorInColorSpace(const QColor &color, ColorSpace colorSpace);

	static qreal normalizeAngle(qreal angle);

	bool m_hueLimit = false;
	bool m_saturationLimit = false;
	bool m_valueLimit = false;
	bool m_pathCacheValid = false;
	int m_hueCount = HUE_COUNT_DEFAULT;
	int m_saturationCount = SATURATION_COUNT_DEFAULT;
	int m_valueCount = VALUE_COUNT_DEFAULT;
	Region m_pressRegion = Region::None;
	QColor m_color = Qt::black;
	qreal m_hueAngle = 0.0;
	qreal m_hue = 0.0;
	qreal m_saturation = 0.0;
	qreal m_value = 0.0;
	ColorSpace m_colorSpace = ColorSpace::ColorHSV;
	QPixmap m_barCache;
	QPixmap m_wheelCache;
	QPainterPath m_pathCache;
};

}

#endif
