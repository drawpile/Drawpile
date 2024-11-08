// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/artisticcolorwheel.h"
#include "desktop/utils/qtguicompat.h"
#include <QImage>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtColorWidgets/color_utils.hpp>
#include <cmath>

namespace widgets {

ArtisticColorWheel::ArtisticColorWheel(QWidget *parent)
	: QWidget(parent)
{
}

void ArtisticColorWheel::setHueLimit(bool hueLimit)
{
	if(hueLimit != m_hueLimit) {
		m_hueLimit = hueLimit;
		m_wheelCache = QPixmap();
		m_pathCacheValid = false;
		update();
	}
}

void ArtisticColorWheel::setHueCount(int hueCount)
{
	int count = qBound(HUE_COUNT_MIN, hueCount, HUE_COUNT_MAX);
	if(count != m_hueCount) {
		m_hueCount = count;
		if(m_hueLimit) {
			m_wheelCache = QPixmap();
			m_pathCacheValid = false;
			update();
		}
	}
}

void ArtisticColorWheel::setHueAngle(int hueAngle)
{
	qreal angle = normalizeAngle(qreal(hueAngle));
	if(m_hueAngle != angle) {
		m_hueAngle = angle;
		if(m_hueLimit) {
			m_wheelCache = QPixmap();
			m_pathCacheValid = false;
			update();
		}
	}
}

void ArtisticColorWheel::setSaturationLimit(bool saturationLimit)
{
	if(saturationLimit != m_saturationLimit) {
		m_saturationLimit = saturationLimit;
		m_wheelCache = QPixmap();
		m_pathCacheValid = false;
		update();
	}
}

void ArtisticColorWheel::setSaturationCount(int saturationCount)
{
	int count =
		qBound(SATURATION_COUNT_MIN, saturationCount, SATURATION_COUNT_MAX);
	if(count != m_saturationCount) {
		m_saturationCount = count;
		if(m_saturationLimit) {
			m_wheelCache = QPixmap();
			m_pathCacheValid = false;
			update();
		}
	}
}

void ArtisticColorWheel::setValueLimit(bool valueLimit)
{
	if(valueLimit != m_valueLimit) {
		m_valueLimit = valueLimit;
		m_barCache = QPixmap();
		m_pathCacheValid = false;
		update();
	}
}

void ArtisticColorWheel::setValueCount(int valueCount)
{
	int count = qBound(VALUE_COUNT_MIN, valueCount, VALUE_COUNT_MAX);
	if(count != m_valueCount) {
		m_valueCount = count;
		if(m_valueLimit) {
			m_barCache = QPixmap();
			m_pathCacheValid = false;
			update();
		}
	}
}

void ArtisticColorWheel::setColorSpace(ColorSpace colorSpace)
{
	switch(colorSpace) {
	case ColorSpace::ColorHSV:
	case ColorSpace::ColorHSL:
	case ColorSpace::ColorLCH:
		break;
	default:
		qWarning("Unknown color space %d", int(colorSpace));
		return;
	}
	if(colorSpace != m_colorSpace) {
		m_colorSpace = colorSpace;
		setColorInColorSpace(m_color, colorSpace);
		m_barCache = QPixmap();
		m_wheelCache = QPixmap();
		m_pathCacheValid = false;
		update();
	}
}

void ArtisticColorWheel::setColor(const QColor &color)
{
	if(color.isValid() && color != m_color) {
		qreal h = m_hue;
		qreal s = m_saturation;
		qreal v = m_value;
		setColorInColorSpace(color, m_colorSpace);
		if(m_hue != h || m_saturation != s) {
			m_barCache = QPixmap();
			if(m_value != v) {
				m_wheelCache = QPixmap();
			}
		} else if(m_value != v) {
			m_wheelCache = QPixmap();
		}
		m_pathCacheValid = false;
		update();
	}
}

void ArtisticColorWheel::resizeEvent(QResizeEvent *event)
{
	QSize barSize = valueBarRect().size();
	QSize wheelSize = wheelRect().size();
	m_pathCacheValid = false;
	QWidget::resizeEvent(event);
	if(barSize != valueBarRect().size()) {
		m_barCache = QPixmap();
	}
	if(wheelSize != wheelRect().size()) {
		m_wheelCache = QPixmap();
	}
}

void ArtisticColorWheel::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	qreal dpr = painter.device()->devicePixelRatioF();
	painter.setRenderHint(QPainter::Antialiasing);

	QRect r = rect();
	painter.setBrush(m_color);
	painter.setPen(Qt::NoPen);
	painter.drawConvexPolygon(QPolygon(
		{QPoint(r.right() - 64, r.top()), r.topRight(),
		 QPoint(r.right(), r.top() + 64)}));

	QRect vr = valueBarRect();
	updateBarCache(QSize(qRound(vr.width() * dpr), qRound(vr.height() * dpr)));
	painter.drawPixmap(vr, m_barCache, m_barCache.rect());

	QRect wr = wheelRect();
	updateWheelCache(qRound(wr.width() * dpr));
	painter.drawPixmap(wr, m_wheelCache, m_wheelCache.rect());

	painter.setBrush(Qt::NoBrush);
	QColor c = color_widgets::utils::color_lumaF(m_color) < 0.5 ? Qt::white
																: Qt::black;

	updatePathCache(vr, wr);
	if(!m_pathCache.isEmpty()) {
		painter.setPen(QPen(c, 3.0));
		painter.drawPath(m_pathCache);
		painter.setPen(QPen(m_color, 1.0));
		painter.drawPath(m_pathCache);
	}

	painter.setPen(QPen(c, 3.0));
	QPoint p1(vr.x(), vr.y() + vr.height() * (1.0 - m_value));
	QPoint p2(vr.x() + vr.width(), p1.y());
	painter.drawLine(p1, p2);
	painter.drawEllipse(
		QLineF::fromPolar(m_saturation * wr.width() / 2.0, m_hue * 360.0)
			.translated(QRectF(wr).center())
			.p2(),
		6.0, 6.0);
}

void ArtisticColorWheel::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton) {
		QPointF posf = compat::mousePosition(*event);
		if(QRectF vr(valueBarRect()); vr.contains(posf)) {
			m_pressRegion = Region::Bar;
			emit editingStarted();
			handleMouseOnBar(vr, posf);
		} else {
			QRectF wr(wheelRect());
			qreal radius = wr.width() / 2.0;
			QLineF line(wr.center(), posf);
			qreal length = line.length();
			if(length < radius) {
				m_pressRegion = Region::Wheel;
				emit editingStarted();
				handleMouseOnWheel(radius, length, line.angle());
			} else {
				m_pressRegion = Region::None;
			}
		}
	}
}

void ArtisticColorWheel::mouseMoveEvent(QMouseEvent *event)
{
	switch(m_pressRegion) {
	case Region::Bar:
		handleMouseOnBar(QRectF(valueBarRect()), compat::mousePosition(*event));
		break;
	case Region::Wheel: {
		QRectF wr(wheelRect());
		QLineF line(wr.center(), compat::mousePosition(*event));
		handleMouseOnWheel(wr.width() / 2.0, line.length(), line.angle());
		break;
	}
	default:
		break;
	}
}

void ArtisticColorWheel::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_pressRegion != Region::None && event->button() == Qt::LeftButton) {
		emit editingFinished();
		m_pressRegion = Region::None;
	}
}

QRect ArtisticColorWheel::valueBarRect() const
{
	return QRect(0, 0, 32, height()).marginsRemoved(QMargins(2, 2, 2, 2));
}

QRect ArtisticColorWheel::wheelRect() const
{
	QRect barRect = valueBarRect();
	int bw = barRect.width();
	int bh = barRect.height();
	int wheelAreaWidth = width() - bw;
	QRect wr;
	if(wheelAreaWidth < barRect.height()) {
		wr = QRect(
			barRect.x() + bw, barRect.y() + (bh - wheelAreaWidth) / 2,
			wheelAreaWidth, wheelAreaWidth);
	} else {
		wr = QRect(
			barRect.x() + bw + (wheelAreaWidth - bh) / 2.0, barRect.y(), bh,
			bh);
	}
	return wr.marginsRemoved(QMargins(8, 8, 8, 8));
}

void ArtisticColorWheel::handleMouseOnBar(const QRectF &vr, const QPointF &posf)
{
	qreal v = getValueAt(posf.y() / vr.height());
	if(v != m_value) {
		m_value = v;
		m_color = getColor(m_hue, m_saturation, v);
		m_wheelCache = QPixmap();
		m_pathCacheValid = false;
		update();
		emit colorSelected(m_color);
	}
}

void ArtisticColorWheel::handleMouseOnWheel(
	qreal radius, qreal length, qreal angle)
{
	qreal h = getHueAt(angle);
	qreal s = getSaturationAt(length / radius);
	if(h != m_hue || s != m_saturation) {
		m_hue = h;
		m_saturation = s;
		m_color = getColor(h, s, m_value);
		m_barCache = QPixmap();
		m_pathCacheValid = false;
		update();
		emit colorSelected(m_color);
	}
}

void ArtisticColorWheel::updateBarCache(const QSize &size)
{
	if(m_barCache.isNull() || m_barCache.size() != size) {
		m_barCache = QPixmap(size);
		int w = size.width();
		int h = size.height();
		QPainter painter(&m_barCache);
		if(m_valueLimit) {
			painter.setRenderHint(QPainter::Antialiasing, true);
			qreal vals = m_valueCount;
			qreal valueBarSectionHeight = h / vals;
			qreal valueBarSectionY = 0;
			int lastValue = m_valueCount - 1;
			for(int i = 0; i < lastValue; ++i) {
				painter.fillRect(
					QRectF(
						0, valueBarSectionY, w,
						std::ceil(valueBarSectionHeight)),
					getColor(m_hue, m_saturation, getValueSection(i)));
				valueBarSectionY += valueBarSectionHeight;
			}
			painter.fillRect(
				QRectF(0, valueBarSectionY, w, h - valueBarSectionY),
				getColor(m_hue, m_saturation, getValueSection(lastValue)));
		} else {
			painter.setRenderHint(QPainter::Antialiasing, false);
			qreal lastValue = h - 1;
			for(int y = 0; y < h; ++y) {
				painter.fillRect(
					QRect(0, y, w, 1),
					getColor(
						m_hue, m_saturation, getValueAt(qreal(y) / lastValue)));
			}
		}
	}
}

void ArtisticColorWheel::updateWheelCache(int dimension)
{
	if(m_wheelCache.isNull() || m_wheelCache.width() != dimension ||
	   m_wheelCache.height() != dimension) {
		QImage img(dimension, dimension, QImage::Format_ARGB32_Premultiplied);
		QPointF center = QRectF(img.rect()).center();
		qreal radius = qreal(dimension) / 2.0;
		for(int y = 0; y < dimension; ++y) {
			QRgb *pixels = reinterpret_cast<QRgb *>(img.scanLine(y));
			for(int x = 0; x < dimension; ++x) {
				QLineF line(center, QPointF(x, y));
				qreal h = getHueAt(line.angle());
				qreal s = getSaturationAt(line.length() / radius);
				pixels[x] = getColor(h, s, m_value).rgba();
			}
		}

		m_wheelCache = QPixmap(img.size());
		m_wheelCache.fill(Qt::transparent);
		QPainter painter(&m_wheelCache);
		painter.setBrush(img);
		painter.setPen(Qt::NoPen);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.drawEllipse(m_wheelCache.rect());
	}
}

void ArtisticColorWheel::updatePathCache(const QRectF &vr, const QRectF &wr)
{
	if(!m_pathCacheValid) {
		m_pathCacheValid = true;
		m_pathCache.clear();

		if(m_hueLimit) {
			QPointF center = wr.center();
			qreal radius = wr.width() / 2.0;
			qreal angleOffset = 180.0 / m_hueCount - m_hueAngle;
			qreal sectionAngle = 360.0 / m_hueCount;
			int hueSection = qFloor(
				normalizeAngle(m_hue * 360.0 + angleOffset) / 360.0 *
				m_hueCount);
			qreal hueAngle1 = hueSection * sectionAngle - angleOffset;
			qreal hueAngle2 = hueAngle1 + sectionAngle;
			if(m_saturationLimit) {
				int saturationSection = qBound(
					0, qFloor(m_saturation * qreal(m_saturationCount)),
					m_saturationCount - 1);
				qreal r2 = radius / m_saturationCount * (saturationSection + 1);
				qreal d2 = r2 * 2.0;
				if(saturationSection > 0) {
					qreal r1 = radius / m_saturationCount * saturationSection;
					qreal d1 = r1 * 2.0;
					m_pathCache.moveTo(QLineF::fromPolar(r2, hueAngle1)
										   .translated(center)
										   .p2());
					m_pathCache.arcTo(
						center.x() - r2, center.y() - r2, d2, d2, hueAngle1,
						sectionAngle);
					m_pathCache.lineTo(QLineF::fromPolar(r1, hueAngle2)
										   .translated(center)
										   .p2());
					m_pathCache.arcTo(
						center.x() - r1, center.y() - r1, d1, d1, hueAngle2,
						-sectionAngle);
					m_pathCache.closeSubpath();
				} else {
					m_pathCache.moveTo(QLineF::fromPolar(r2, hueAngle1)
										   .translated(center)
										   .p2());
					m_pathCache.arcTo(
						center.x() - r2, center.y() - r2, d2, d2, hueAngle1,
						sectionAngle);
					m_pathCache.lineTo(center);
					m_pathCache.closeSubpath();
				}
			} else {
				qreal diameter = radius * 2.0;
				m_pathCache.moveTo(QLineF::fromPolar(radius, hueAngle1)
									   .translated(center)
									   .p2());
				m_pathCache.arcTo(
					center.x() - radius, center.y() - radius, diameter,
					diameter, hueAngle1, sectionAngle);
				m_pathCache.lineTo(center);
				m_pathCache.closeSubpath();
			}
		} else if(m_saturationLimit) {
			QPointF center = wr.center();
			qreal radius = wr.width() / 2.0;
			int saturationSection = qBound(
				0, qFloor(m_saturation * qreal(m_saturationCount)),
				m_saturationCount - 1);
			if(saturationSection > 0) {
				qreal r1 = radius / m_saturationCount * saturationSection;
				m_pathCache.addEllipse(center, r1, r1);
			}
			qreal r2 = radius / m_saturationCount * (saturationSection + 1);
			m_pathCache.addEllipse(center, r2, r2);
		}

		if(m_valueLimit) {
			int valueSection = qBound(
				0, qFloor((1.0 - m_value) * qreal(m_valueCount)),
				m_valueCount - 1);
			qreal valueSectionHeight = vr.height() / m_valueCount;
			m_pathCache.addRect(QRect(
				vr.left(), qFloor(vr.top() + valueSectionHeight * valueSection),
				vr.width(), qCeil(valueSectionHeight)));
		}
	}
}

qreal ArtisticColorWheel::getHueAt(qreal angle) const
{
	angle = normalizeAngle(angle);
	if(m_hueLimit) {
		qreal hues = m_hueCount;
		qreal angleOffset = 180.0 / hues - m_hueAngle;
		int i = qFloor(normalizeAngle(angle + angleOffset) / 360.0 * hues);
		return std::fmod(
			qreal(qBound(0, i, m_hueCount)) / hues + m_hueAngle / 360.0, 1.0);
	} else {
		return angle / 360.0;
	}
}

qreal ArtisticColorWheel::getSaturationAt(qreal n) const
{
	if(m_saturationLimit) {
		int lastSaturation = m_saturationCount - 1;
		return qreal(qBound(
				   0, qFloor(n * qreal(m_saturationCount)), lastSaturation)) /
			   qreal(lastSaturation);
	} else {
		return qBound(0.0, n, 1.0);
	}
}

qreal ArtisticColorWheel::getValueAt(qreal n) const
{
	if(m_valueLimit) {
		return getValueSection(
			(qBound(0, qFloor(n * qreal(m_valueCount)), m_valueCount - 1)));
	} else {
		return 1.0 - qBound(0.0, n, 1.0);
	}
}

qreal ArtisticColorWheel::getValueSection(int i) const
{
	Q_ASSERT(i >= 0);
	Q_ASSERT(i < m_valueCount);
	return 1.0 - qreal(i) / qreal(m_valueCount - 1);
}

QColor ArtisticColorWheel::getColor(qreal h, qreal s, qreal v) const
{
	return getColorInColorSpace(h, s, v, m_colorSpace);
}

QColor ArtisticColorWheel::getColorInColorSpace(
	qreal h, qreal s, qreal v, ColorSpace colorSpace)
{
	switch(colorSpace) {
	case ColorSpace::ColorHSL:
		return color_widgets::utils::color_from_hsl(h, s, v);
	case ColorSpace::ColorLCH:
		return color_widgets::utils::color_from_lch(h, s, v);
	default:
		return QColor::fromHsvF(h, s, v);
	}
}

void ArtisticColorWheel::setColorInColorSpace(
	const QColor &color, ColorSpace colorSpace)
{
	m_color = color;
	switch(colorSpace) {
	case ColorSpace::ColorHSL:
		m_hue = color.hueF();
		m_saturation = color_widgets::utils::color_HSL_saturationF(color);
		m_value = color_widgets::utils::color_lightnessF(color);
		break;
	case ColorSpace::ColorLCH:
		m_hue = color.hueF();
		m_saturation = color_widgets::utils::color_chromaF(color);
		m_value = color_widgets::utils::color_lumaF(color);
		break;
	default:
		m_hue = color.hueF();
		m_saturation = color.saturationF();
		m_value = color.valueF();
		break;
	}
}

qreal ArtisticColorWheel::normalizeAngle(qreal angle)
{
	qreal n = std::fmod(angle, 360.0);
	return n < 0.0 ? n + 360.0 : n;
}

}
