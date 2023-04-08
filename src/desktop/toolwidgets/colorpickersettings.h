// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_COLORPICKER_H
#define TOOLSETTINGS_COLORPICKER_H

#include "desktop/toolwidgets/toolsettings.h"

namespace color_widgets {
	class Swatch;
}
class QCheckBox;
class QSpinBox;

namespace tools {

/**
 * @brief Color picker history
 */
class ColorPickerSettings final : public ToolSettings {
Q_OBJECT
public:
	ColorPickerSettings(ToolController *ctrl, QObject *parent=nullptr);
	~ColorPickerSettings() override;

	QString toolType() const override { return QStringLiteral("picker"); }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setForeground(const QColor &color) override { Q_UNUSED(color); }
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return false; }

public slots:
	void addColor(const QColor &color);
	void pushSettings() override;
	void openColorDialog();
	void selectColorFromDialog(const QColor &color);

signals:
	void colorSelected(const QColor &color);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	color_widgets::Swatch *m_palettewidget;
	QCheckBox *m_layerpick;
	QSpinBox *m_size;
	qreal m_quickAdjust1 = 0.0;
};

}

#endif


