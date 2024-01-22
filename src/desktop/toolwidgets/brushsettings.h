// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_BRUSHSETTINGS_H
#define DESKTOP_TOOLWIDGETS_BRUSHSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QSpinBox;

namespace brushes {
class ActiveBrush;
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
	enum BrushMode : int { NormalMode, EraseMode, AlphaLockMode, UnknownMode };

	BrushSettings(ToolController *ctrl, QObject *parent = nullptr);
	~BrushSettings() override;

	QString toolType() const override { return QStringLiteral("brush"); }

	void setActiveTool(tools::Tool::Type tool) override;
	void setForeground(const QColor &color) override;
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	int getSize() const override;
	bool getSubpixelMode() const override;
	bool isSquare() const override;

	BrushMode getBrushMode() const;
	void resetBrushMode();

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setCurrentBrush(brushes::ActiveBrush brush);
	brushes::ActiveBrush currentBrush() const;

	int currentBrushSlot() const;
	bool isCurrentEraserSlot() const;

	void setShareBrushSlotColor(bool sameColor);

	QWidget *getHeaderWidget() override;

	bool isLocked() override;
	bool keepTitleBarButtonSpace() const override { return true; }
	void setMyPaintAllowed(bool myPaintAllowed);
	void setCompatibilityMode(bool compatibilityMode);

public slots:
	void selectBrushSlot(int i);
	void selectEraserSlot(bool eraser);
	void swapWithSlot(int i);
	void setGlobalSmoothing(int smoothing);
	void toggleEraserMode() override;
	void toggleRecolorMode() override;
	void setEraserMode(bool erase);
	void reloadPreset();

signals:
	void colorChanged(const QColor &color);
	void eraseModeChanged(bool erase);
	void subpixelModeChanged(bool subpixel, bool square);
	void brushModeChanged(int brushMode); // See enum BrushMode above.
	void pixelSizeChanged(int size);
	void brushSettingsDialogRequested();

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private slots:
	void changeBrushType();
	void changeSizeSetting(int size);
	void changeRadiusLogarithmicSetting(int radiusLogarithmic);
	void selectBlendMode(int);
	void updateFromUi();
	void updateFromUiWith(bool updateShared);
	void updateStabilizationSettingVisibility();
	void quickAdjustOn(QSpinBox *box, qreal adjustment);

private:
	enum class Lock { None, MyPaintPermission, MyPaintCompat, IndirectCompat };

	void updateUi();

	Lock getLock();
	static QString getLockDescription(Lock lock);

	void adjustSettingVisibilities(bool softmode, bool mypaintmode);
	void emitBrushModeChanged();
	static double radiusLogarithmicToPixelSize(int radiusLogarithmic);

	static QByteArray getDefaultBrushForSlot(int i);

	struct Private;
	Private *d;
};

}

#endif
