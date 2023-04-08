// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_BRUSHES_H
#define TOOLSETTINGS_BRUSHES_H

#include "libclient/canvas/pressure.h"
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QSpinBox;

namespace brushes {
	class ActiveBrush;
}

namespace input {
	struct Preset;
}

namespace tools {

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings final : public ToolSettings {
	Q_OBJECT
	friend class AdvancedBrushSettings;
public:
	BrushSettings(ToolController *ctrl, QObject *parent=nullptr);
	~BrushSettings() override;

	QString toolType() const override { return QStringLiteral("brush"); }

	void setActiveTool(tools::Tool::Type tool) override;
	void setForeground(const QColor& color) override;
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	int getSize() const override;
	bool getSubpixelMode() const override;
	bool isSquare() const override;

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setCurrentBrush(brushes::ActiveBrush brush);
	brushes::ActiveBrush currentBrush() const;

	int currentBrushSlot() const;
	bool isCurrentEraserSlot() const;

	void setShareBrushSlotColor(bool sameColor);

	int getSmoothing() const;
	PressureMapping getPressureMapping() const;

	QWidget *getHeaderWidget() override;

public slots:
	void selectBrushSlot(int i);
	void selectEraserSlot(bool eraser);
	void toggleEraserMode() override;
	void toggleRecolorMode() override;

signals:
	void colorChanged(const QColor &color);
	void eraseModeChanged(bool erase);
	void subpixelModeChanged(bool subpixel, bool square);
	void smoothingChanged(int smoothing);
	void pressureMappingChanged(const PressureMapping &pressureMapping);
	void pixelSizeChanged(int size);
	void brushSettingsDialogRequested();

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private slots:
	void changeBrushType();
	void changeSizeSetting(int size);
	void changeRadiusLogarithmicSetting(int radiusLogarithmic);
	void selectBlendMode(int);
	void setEraserMode(bool erase);
	void updateUi();
	void updateFromUi();
	void updateFromUiWith(bool updateShared);
	void chooseInputPreset(int index);
	void quickAdjustOn(QSpinBox *box, qreal adjustment);

private:
	void adjustSettingVisibilities(bool softmode, bool mypaintmode);
	void emitPresetChanges(const input::Preset *preset);
	static double radiusLogarithmicToPixelSize(int radiusLogarithmic);

	struct Private;
	Private *d;
};

}

#endif

