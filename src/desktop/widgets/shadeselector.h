// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_SHADESELECTOR_H
#define DESKTOP_WIDGETS_SHADESELECTOR_H
#include <QColor>
#include <QVariantHash>
#include <QVariantList>
#include <QVector>
#include <QWidget>
#include <QtColorWidgets/color_wheel.hpp>
#include <QtColorWidgets/swatch.hpp>

namespace color_widgets {
class ColorPalette;
class Swatch;
}

namespace widgets {

class ShadeSelector final : public color_widgets::Swatch {
	Q_OBJECT
	using ColorPalette = color_widgets::ColorPalette;
	using ColorWheel = color_widgets::ColorWheel;

public:
	enum class Preset {
		None,
		HueRange,
		SaturationRange,
		ValueRange,
		Shades,
		Count,
	};

	struct Row {
		qreal rangeH = 0.0;
		qreal rangeS = 0.0;
		qreal rangeV = 0.0;
		qreal shiftH = 0.0;
		qreal shiftS = 0.0;
		qreal shiftV = 0.0;

		QVector<qreal> toVector() const
		{
			return {rangeH, rangeS, rangeV, shiftH, shiftS, shiftV};
		}
	};

	explicit ShadeSelector(QWidget *parent = nullptr);

	void setConfig(const QVariantList &rowsConfig);
	static QVariantList getDefaultConfig();
	static Row configToRow(const QVariantHash &rowConfig);

	void setRowHeight(int rowHeight);
	void setColumnCount(int columnCount);
	void setBorderThickness(int borderThickness);
	const QColor &color() const { return m_color; }
	void setColor(const QColor &color);
	void forceSetColor(const QColor &color);

	QSize sizeHint() const override;

signals:
	void colorDoubleClicked(const QColor &color);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void updateConfig(const QVariantList &rowsConfig);
	static qreal
	getPresetParam(const QVariantList &preset, int index, qreal fallback);

	void handleDoubleClick(int index);

	void refresh();
	void refreshRow(
		const Row &row, ColorPalette &pal, int columns, qreal h0, qreal s0,
		qreal v0);

	qreal getSpaceH() const;
	qreal getSpaceS() const;
	qreal getSpaceV() const;
	QColor toColorFromSpace(qreal h, qreal s, qreal v) const;
	static qreal fixH(qreal h);
	static qreal fixSV(qreal sv);

	QVector<Row> m_rows;
	int m_columnCount = 10;
	int m_rowHeight = 16;
	int m_borderThickness = 0;
	QColor m_color = Qt::black;
	ColorWheel::ColorSpaceEnum m_colorSpace = ColorWheel::ColorHSV;
};

}

#endif
