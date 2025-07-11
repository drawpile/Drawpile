// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_BRUSHSETTINGS_H
#define DESKTOP_TOOLWIDGETS_BRUSHSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/brushes/brushpresetmodel.h"

class KisSliderSpinBox;
class QAction;

namespace tools {

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings final : public ToolSettings {
	Q_OBJECT
public:
	static constexpr int BRUSH_SLOT_COUNT = 9;
	static constexpr int TOTAL_SLOT_COUNT = BRUSH_SLOT_COUNT + 1;
	static constexpr int ERASER_SLOT_INDEX = BRUSH_SLOT_COUNT;

	enum BrushMode : int { NormalMode, EraseMode, AlphaLockMode, UnknownMode };

	BrushSettings(ToolController *ctrl, QObject *parent = nullptr);
	~BrushSettings() override;

	void setActions(
		QAction *reloadPreset, QAction *reloadPresetSlots,
		QAction *reloadAllPresets, QAction *nextSlot, QAction *previousSlot,
		QAction *automaticAlphaPreserve, QAction *maskSelection);
	void connectBrushPresets(brushes::BrushPresetModel *brushPresets);

	QString toolType() const override { return QStringLiteral("brush"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }

	void setActiveTool(tools::Tool::Type tool) override;
	void setForeground(const QColor &color) override;
	void setBackground(const QColor &color) override;
	void setCompatibilityMode(bool compatibilityMode) override;
	void quickAdjust1(qreal adjustment) override;
	void quickAdjust2(qreal adjustment) override;
	void quickAdjust3(qreal adjustment) override;
	void stepAdjust1(bool increase) override;
	void stepAdjust2(bool increase) override;
	void stepAdjust3(bool increase) override;

	int getSize() const override;
	bool getSubpixelMode() const override;
	bool isSquare() const override;
	QPointF getOffset() const override;
	int getBlendMode() const;

	BrushMode getBrushMode() const;
	void resetBrushMode();

	void triggerUpdate();

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setCurrentBrushDetached(const brushes::ActiveBrush &brush);
	void setCurrentBrushPreset(const brushes::Preset &p);
	void changeCurrentBrush(const brushes::ActiveBrush &brush);
	void setBrushDetachedInSlot(const brushes::ActiveBrush &brush, int i);
	void setBrushPresetInSlot(const brushes::Preset &p, int i);
	brushes::ActiveBrush currentBrush() const;
	int currentPresetId() const;
	const QString &currentPresetName() const;
	const QString &currentPresetDescription() const;
	const QPixmap &currentPresetThumbnail() const;
	bool isCurrentPresetAttached() const;
	bool isCurrentSlotUpdateInProgress() const;
	void clearCurrentDetachedPresetChanges() const;

	int currentBrushSlot() const;
	bool isCurrentEraserSlot() const;

	int brushSlotCount() const;
	void setBrushSlotCount(int count);

	void setShareBrushSlotColor(bool sameColor);

	bool brushPresetsAttach() const;
	void setBrushPresetsAttach(bool brushPresetsAttach);

	QWidget *getHeaderWidget() override;

	bool isLocked() override;
	void setMyPaintAllowed(bool myPaintAllowed);
	void setSlowModesAllowed(bool slowModesAllowed);
	void setBrushSizeLimit(int brushSizeLimit);

	void selectBrushSlot(int i);
	void selectEraserSlot(bool eraser);
	void selectNextSlot();
	void selectPreviousSlot();
	void swapWithSlot(int i);
	void setGlobalSmoothing(int smoothing);
	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;
	void toggleBlendMode(int blendMode) override;
	void setEraserMode(bool erase);
	void resetPreset();
	void resetPresetsInAllSlots();

	void changeCurrentPresetName(const QString &name);
	void changeCurrentPresetDescription(const QString &description);
	void changeCurrentPresetThumbnail(const QPixmap &thumbnail);

signals:
	void presetIdChanged(int presetId, bool attached);
	void colorChanged(const QColor &color);
	void backgroundColorChanged(const QColor &color);
	void eraseModeChanged(bool erase);
	void subpixelModeChanged(bool subpixel, bool square);
	void blendModeChanged(int blendMode);
	void brushModeChanged(int brushMode); // See enum BrushMode above.
	void pixelSizeChanged(int size);
	void offsetChanged(const QPointF &offset);
	void newBrushRequested();
	void editBrushRequested();
	void overwriteBrushRequested();
	void deleteBrushRequested();

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	enum class BrushType { PixelRound, PixelSquare, SoftRound, MyPaint };
	enum class Lock { None, MyPaintPermission, SlowModesPermission };

	void changeBrushType(const QAction *action);
	void changePaintMode(const QAction *action);
	void changeSizeSetting(int size);
	void changeRadiusLogarithmicSetting(int radiusLogarithmic);
	void updateBlendMode(int blendMode, bool eraseMode);
	void updateFromUi();
	void updateFromUiWith(bool updateShared);
	void updateStabilizationSettingVisibility();
	void
	quickAdjustOn(KisSliderSpinBox *box, qreal adjustment, qreal &quickAdjustN);
	void handlePresetChanged(
		int presetId, const QString &name, const QString &description,
		const QPixmap &thumbnail, const brushes::ActiveBrush &brush);
	void handlePresetRemoved(int presetId);
	void detachCurrentSlot();

	void changePresetBrush(const brushes::ActiveBrush &brush);
	void updateChangesInCurrentBrushPreset();
	void updateChangesInBrushPresetInSlot(int i);

	void resetBrushInSlot(int i);
	brushes::ActiveBrush changeBrushInSlot(brushes::ActiveBrush brush, int i);

	void updateMenuActions();
	void setPaintModeInUi(int paintMode, bool directOnly);
	void updateUi();

	Lock getLock();
	static QString getLockDescription(Lock lock);

	void adjustSettingVisibilities(bool softmode, bool mypaintmode);
	void emitOffsetChanged();
	void emitBlendModeChanged();
	void emitBrushModeChanged();

	void adjustSlider(KisSliderSpinBox *slider, int value);
	void updateRadiusLogarithmicLimit();
	int clampBrushSize(int size) const;
	static float myPaintRadiusToRadiusLogarithmic(float myPaintRadius);
	static float radiusLogarithmicToMyPaintRadius(int radiusLogarithmic);
	static float myPaintRadiusToPixelSize(float myPaintRadius);
	static float pixelSizeToMyPaintRadius(float pixelSize);
	static double radiusLogarithmicToPixelSize(int radiusLogarithmic);

	static int translateBrushSlotConfigIndex(int i);
	static int getDefaultPresetIdForSlot(int i);
	static QByteArray getDefaultBrushForSlot(int i);

	struct Private;
	Private *d;
};

}

#endif
