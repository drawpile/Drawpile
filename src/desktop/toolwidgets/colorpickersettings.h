// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_COLORPICKER_H
#define TOOLSETTINGS_COLORPICKER_H

#include "desktop/toolwidgets/toolsettings.h"
#include <QWidget>

class QCheckBox;
class QSpinBox;

namespace widgets {
class GroupedToolButton;
class PaletteWidget;
}

namespace tools {

namespace colorpickersettings {

class HeaderWidget final : public QWidget {
	Q_OBJECT
public:
	HeaderWidget(QWidget *parent);
	void pickFromScreen();
	void stopPickingFromScreen();

signals:
	void colorPicked(const QColor &color);
	void pickingChanged(bool picking);

protected:
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	bool m_picking = false;
};

}

/**
 * @brief Color picker history
 */
class ColorPickerSettings final : public ToolSettings {
	Q_OBJECT
public:
	ColorPickerSettings(ToolController *ctrl, QObject *parent = nullptr);
	~ColorPickerSettings() override;

	QString toolType() const override { return QStringLiteral("picker"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setForeground(const QColor &color) override { Q_UNUSED(color); }
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	int getSize() const override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

public slots:
	void addColor(const QColor &color);
	void pushSettings() override;
	void selectColor(const QColor &color);
	void startPickFromScreen();
	void cancelPickFromScreen();

signals:
	void colorSelected(const QColor &color);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private slots:
	void pickFromScreen();

private:
	colorpickersettings::HeaderWidget *m_headerWidget = nullptr;
	widgets::GroupedToolButton *m_pickButton = nullptr;
	widgets::PaletteWidget *m_palettewidget;
	QCheckBox *m_layerpick;
	QSpinBox *m_size;
	qreal m_quickAdjust1 = 0.0;
	bool m_pickingFromScreen = false;
};

}

#endif
