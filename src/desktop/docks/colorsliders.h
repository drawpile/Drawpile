// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLORSLIDERDOCK_H
#define COLORSLIDERDOCK_H

#include "desktop/docks/dockbase.h"
#include <QtColorWidgets/color_wheel.hpp>

namespace color_widgets {
class ColorPalette;
}

namespace docks {

class ColorSliderDock final : public DockBase {
	Q_OBJECT
public:
	explicit ColorSliderDock(QWidget *parent);
	~ColorSliderDock() override;

public slots:
	void setColor(const QColor &color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);
	void adjustColor(int channel, int amount);

private slots:
	void updateFromRgbSliders();
	void updateFromRgbSpinbox();
	void updateFromHsvSliders();
	void updateFromHsvSpinbox();
	void updateFromLineEdit(const QColor &color);
	void updateFromLineEditFinished(const QColor &color);
	void setColorSpace(color_widgets::ColorWheel::ColorSpaceEnum colorSpace);
	void setMode(int mode);
	void updateWidgetVisibilities();

signals:
	void colorSelected(const QColor &color);

private:
	static constexpr int SLIDER_COLORS = 3;

	void updateColor(const QColor &color, bool fromHsvSelection, bool selected);
	void updateHueSlider(const QColor &color, bool fromHsvSelection);
	void updateSaturationSlider(const QColor &color, bool fromHsvSelection);
	void updateValueSlider(const QColor &color, bool fromHsvSelection);

	QColor getColorSpaceColor(int h, int s, int v) const;
	static QColor fixHue(int h, const QColor &color);

	void setSwatchFlags(int flags);

	struct Private;
	Private *d;
};

}

#endif
