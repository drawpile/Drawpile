// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/shadeselector.h"
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QVBoxLayout>
#include <QtColorWidgets/color_palette.hpp>
#include <QtColorWidgets/color_utils.hpp>
#include <cmath>

namespace widgets {

ShadeSelector::ShadeSelector(QWidget *parent)
	: color_widgets::Swatch(parent)
{
	setReadOnly(true);
	setBorderThickness(1);
	setMaxColorSize(QSize(99999, 99999));
	connect(
		this, &ShadeSelector::doubleClicked, this,
		&ShadeSelector::handleDoubleClick);
}

void ShadeSelector::setConfig(const QVariantList &rowsConfig)
{
	if(rowsConfig.isEmpty()) {
		updateConfig(getDefaultConfig());
	} else {
		updateConfig(rowsConfig);
	}
}

QVariantList ShadeSelector::getDefaultConfig()
{
	return QVariantList({
		QVariantHash({
			{QStringLiteral("p"), QVariantList({int(Preset::HueRange), 0.2})},
			{QStringLiteral("rh"), 0.2},
		}),
		QVariantHash({
			{QStringLiteral("p"),
			 QVariantList({int(Preset::Shades), 1.0, 1.0})},
			{QStringLiteral("rs"), 1.0},
			{QStringLiteral("rv"), 1.0},
		}),
		QVariantHash({
			{QStringLiteral("p"),
			 QVariantList({int(Preset::Shades), -1.0, 1.0})},
			{QStringLiteral("rs"), -1.0},
			{QStringLiteral("rv"), 1.0},
		}),
	});
}

ShadeSelector::Row ShadeSelector::configToRow(const QVariantHash &rowConfig)
{
	QVariantList preset = rowConfig.value(QStringLiteral("p")).toList();
	if(!preset.isEmpty()) {
		int type = preset[0].toInt();
		switch(type) {
		case int(Preset::None):
			break;
		case int(Preset::HueRange): {
			Row row;
			row.rangeH = getPresetParam(preset, 1, 1.0);
			return row;
		}
		case int(Preset::SaturationRange): {
			Row row;
			row.rangeS = getPresetParam(preset, 1, 1.0);
			return row;
		}
		case int(Preset::ValueRange): {
			Row row;
			row.rangeV = getPresetParam(preset, 1, 1.0);
			return row;
		}
		case int(Preset::Shades): {
			Row row;
			row.rangeS = getPresetParam(preset, 1, 1.0);
			row.rangeV = getPresetParam(preset, 2, 1.0);
			return row;
		}
		default:
			qWarning("Unknown shade selector preset type %d", type);
			break;
		}
	}

	return Row{
		rowConfig.value(QStringLiteral("rh")).toReal(),
		rowConfig.value(QStringLiteral("rs")).toReal(),
		rowConfig.value(QStringLiteral("rv")).toReal(),
		rowConfig.value(QStringLiteral("sh")).toReal(),
		rowConfig.value(QStringLiteral("ss")).toReal(),
		rowConfig.value(QStringLiteral("sv")).toReal(),
	};
}

void ShadeSelector::setRowHeight(int rowHeight)
{
	if(rowHeight != m_rowHeight) {
		m_rowHeight = rowHeight;
		refresh();
	}
}

void ShadeSelector::setColumnCount(int columnCount)
{
	if(columnCount != m_columnCount) {
		m_columnCount = columnCount;
		refresh();
	}
}

void ShadeSelector::setBorderThickness(int borderThickness)
{
	if(borderThickness != m_borderThickness) {
		m_borderThickness = borderThickness;
		if(borderThickness <= 0) {
			setBorder(Qt::NoPen);
		} else {
			setBorder(QPen(
				QWidget::palette().window().color(), borderThickness,
				Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
		}
	}
}

void ShadeSelector::setColor(const QColor &color)
{
	if(color.isValid() && color != m_color) {
		forceSetColor(color);
	}
}

void ShadeSelector::forceSetColor(const QColor &color)
{
	m_color = color;
	refresh();
}

QSize ShadeSelector::sizeHint() const
{
	return QSize();
}

void ShadeSelector::paintEvent(QPaintEvent *e)
{
	color_widgets::Swatch::paintEvent(e);
	QPainter painter(this);
	bool middleOnColor = m_borderThickness == 0 || (m_columnCount % 2) != 0;
	QColor referenceColor = middleOnColor ? m_color : border().color();
	int fudge = !middleOnColor && m_borderThickness % 2 == 0 ? 1 : 0;
	painter.setPen(QPen(
		referenceColor.lightnessF() < 0.5 ? Qt::white : Qt::black, 1 + fudge,
		Qt::DotLine));
	QRect r = rect();
	int middle = (r.left() + r.right()) / 2 + fudge;
	painter.drawLine(middle, r.top() + 1, middle, r.bottom() - 1);
}

void ShadeSelector::updateConfig(const QVariantList &rowsConfig)
{
	m_rows.clear();
	m_rows.reserve(rowsConfig.size());
	for(const QVariant &v : rowsConfig) {
		QVariantHash rowConfig = v.toHash();
		m_rows.append(configToRow(rowConfig));
	}
	refresh();
}

qreal ShadeSelector::getPresetParam(
	const QVariantList &preset, int index, qreal fallback)
{
	if(index < preset.size()) {
		bool ok;
		qreal value = preset[index].toReal(&ok);
		if(ok && std::isfinite(value)) {
			return value;
		}
	}
	return fallback;
}

void ShadeSelector::handleDoubleClick(int index)
{
	if(index >= 0) {
		emit colorDoubleClicked(palette().colorAt(index));
	}
}

void ShadeSelector::refresh()
{
	int columns = qMax(m_columnCount, 2);
	ColorPalette pal;
	qreal h0 = getSpaceH();
	qreal s0 = getSpaceS();
	qreal v0 = getSpaceV();
	for(const Row &row : m_rows) {
		refreshRow(row, pal, columns, h0, s0, v0);
	}

	int rowCount = m_rows.size();
	clearSelection();
	setPalette(pal);
	setForcedRows(rowCount);
	setForcedColumns(columns);
	setFixedHeight(m_rowHeight * rowCount);
}

void ShadeSelector::refreshRow(
	const Row &row, ColorPalette &pal, int columns, qreal h0, qreal s0,
	qreal v0)
{
	for(int i = 0; i < columns; ++i) {
		qreal k = qreal(i) / qreal(columns - 1);
		qreal h = h0 - row.rangeH / 2.0 + row.rangeH * k + row.shiftH;
		qreal s = s0 - row.rangeS / 2.0 + row.rangeS * k + row.shiftS;
		qreal v = v0 - row.rangeV / 2.0 + row.rangeV * k + row.shiftV;
		pal.appendColor(toColorFromSpace(h, s, v));
	}
}

qreal ShadeSelector::getSpaceH() const
{
	return m_color.hueF();
}

qreal ShadeSelector::getSpaceS() const
{
	switch(m_colorSpace) {
	case ColorWheel::ColorHSL:
		return color_widgets::utils::color_HSL_saturationF(m_color);
	case ColorWheel::ColorLCH:
		return color_widgets::utils::color_chromaF(m_color);
	default:
		return m_color.saturationF();
	}
}

qreal ShadeSelector::getSpaceV() const
{
	switch(m_colorSpace) {
	case ColorWheel::ColorHSL:
		return color_widgets::utils::color_lightnessF(m_color);
	case ColorWheel::ColorLCH:
		return color_widgets::utils::color_lumaF(m_color);
	default:
		return m_color.valueF();
	}
}

QColor ShadeSelector::toColorFromSpace(qreal h, qreal s, qreal v) const
{
	h = fixH(h);
	s = fixSV(s);
	v = fixSV(v);
	switch(m_colorSpace) {
	case ColorWheel::ColorHSL:
		return color_widgets::utils::color_from_hsl(h, s, v);
	case ColorWheel::ColorLCH:
		return color_widgets::utils::color_from_lch(h, s, v);
	default:
		return QColor::fromHsvF(h, s, v);
	}
}

qreal ShadeSelector::fixH(qreal h)
{
	if(std::isfinite(h)) {
		qreal m = std::fmod(h, 1.0);
		return m < 0.0 ? 1.0 + m : m;
	} else {
		return 0.0;
	}
}

qreal ShadeSelector::fixSV(qreal sv)
{
	if(std::isfinite(sv)) {
		return qBound(0.0, sv, 1.0);
	} else {
		return 0.0;
	}
}

}
