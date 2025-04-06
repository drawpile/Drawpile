// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BRUSHEDITORDIALOG_H
#define BRUSHEDITORDIALOG_H
#include "desktop/widgets/mypaintinput.h"
#include "libclient/brushes/brush.h"
#include <QDialog>
#include <QPixmap>
#include <mypaint-brush-settings.h>

class KisSliderSpinBox;
class QComboBox;
class QGraphicsView;
class QKeySequence;
class QLineEdit;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace utils {
class KineticScroller;
}

namespace dialogs {

class BrushPresetForm final : public QWidget {
	Q_OBJECT
public:
	explicit BrushPresetForm(QWidget *parent = nullptr);

	QString presetName() const;
	void setPresetName(const QString &presetName);

	QString presetDescription() const;
	void setPresetDescription(const QString &presetName);

	QPixmap presetThumbnail() const;
	void setPresetThumbnail(const QPixmap &presetThumbnail);

	void setPresetShortcut(const QKeySequence &presetShortcut);

	void setChangeShortcutEnabled(bool enabled);

signals:
	void requestShortcutChange();
	void presetNameChanged(const QString &name);
	void presetDescriptionChanged(const QString &name);
	void presetThumbnailChanged(const QPixmap &thumbnail);

private:
	void choosePresetThumbnailFile();
	void showPresetThumbnail(const QPixmap &thumbnail);
	void renderPresetThumbnail();
	QPixmap applyPresetThumbnailLabel(const QString &label);
	void emitPresetDescriptionChanged();

	QPushButton *m_presetShortcutButton;
	QLineEdit *m_presetShortcutEdit;
	QGraphicsView *m_presetThumbnailView;
	QPushButton *m_presetThumbnailButton;
	QLineEdit *m_presetLabelEdit;
	QLineEdit *m_presetNameEdit;
	QPlainTextEdit *m_presetDescriptionEdit;
	QPixmap m_presetThumbnail;
	QPixmap m_scaledPresetThumbnail;
};

class BrushSettingsDialog final : public QDialog {
	Q_OBJECT
public:
	explicit BrushSettingsDialog(QWidget *parent = nullptr);
	~BrushSettingsDialog() override;

	void showPresetPage();
	void showGeneralPage();

	bool isPresetAttached() const;
	int presetId() const;

signals:
	void presetNameChanged(const QString &presetName);
	void presetDescriptionChanged(const QString &presetDescription);
	void presetThumbnailChanged(const QPixmap &presetThumbnail);
	void brushSettingsChanged(const brushes::ActiveBrush &brush);
	void newBrushRequested();
	void overwriteBrushRequested();
	void shortcutChangeRequested(int presetId);

public slots:
	void setPresetAttached(bool presetAttached, int presetId);
	void setPresetName(const QString &presetName);
	void setPresetDescription(const QString &presetDescription);
	void setPresetThumbnail(const QPixmap &presetThumbnail);
	void setPresetShortcut(const QKeySequence &presetShortcut);
	void setForceEraseMode(bool forceEraseMode);
	void setStabilizerUseBrushSampleCount(bool useBrushSampleCount);
	void setGlobalSmoothing(int smoothing);
	void updateUiFromActiveBrush(const brushes::ActiveBrush &brush);

private slots:
	void categoryChanged(QListWidgetItem *current, QListWidgetItem *);

private:
	struct Dynamics {
		QComboBox *typeCombo;
		KisSliderSpinBox *velocitySlider;
		KisSliderSpinBox *distanceSlider;
		QPushButton *applyVelocityToAllButton;
		QPushButton *applyDistanceToAllButton;
	};

	enum class MyPaintCondition {
		AlwaysEnabled,
		IndirectDisabled,
		BlendOrIndirectDisabled,
		ComparesAlphaOrIndirectDisabled,
	};

	struct Private;
	Private *d;

	void buildDialogUi();
	QWidget *buildPresetPageUi();
	QWidget *buildGeneralPageUi();
	QWidget *buildClassicSizePageUi();
	QWidget *buildClassicOpacityPageUi();
	QWidget *buildClassicHardnessPageUi();
	QWidget *buildClassicSmudgingPageUi();
	Dynamics buildClassicDynamics(
		QVBoxLayout *layout,
		void (brushes::ClassicBrush::*setType)(DP_ClassicBrushDynamicType),
		void (brushes::ClassicBrush::*setVelocity)(float),
		void (brushes::ClassicBrush::*setDistance)(float));
	void buildClassicApplyToAllButton(widgets::CurveWidget *curve);
	QWidget *buildMyPaintPageUi(int setting);
	widgets::MyPaintInput *buildMyPaintInputUi(
		int setting, int input, const MyPaintBrushSettingInfo *settingInfo,
		utils::KineticScroller *kineticScroller);

	void applyCurveToAllClassicSettings(const KisCubicCurve &curve);

	void
	addCategory(const QString &text, const QString &toolTip, int pageIndex);
	void addClassicCategories(bool withHardness);
	void addMyPaintCategories();

	void updateUiFromClassicBrush();
	bool updateClassicBrushDynamics(
		Dynamics &dynamics, const DP_ClassicBrushDynamic &brush);
	void updateUiFromMyPaintBrush();
	void updateMyPaintSettingPage(int setting);
	void updateStabilizerExplanationText();
	void emitChange();

	std::function<void()>
	makeBrushChangeCallback(const std::function<void()> &fn)
	{
		return [this, fn]() {
			if(!isUpdating()) {
				fn();
			}
		};
	}

	template <typename T>
	std::function<void(T)>
	makeBrushChangeCallbackArg(const std::function<void(T)> &fn)
	{
		return [this, fn](T value) {
			if(!isUpdating()) {
				fn(value);
			}
		};
	}

	bool isUpdating() const;

	static void setComboBoxIndexByData(QComboBox *combo, int data);
	static bool shouldIncludeMyPaintSetting(int setting);
	static MyPaintCondition getMyPaintCondition(int setting);

	static QString getMyPaintInputTitle(int input);
	static QString getMyPaintInputDescription(int input);
	static QString getMyPaintSettingTitle(int setting);
	static QString getMyPaintSettingDescription(int setting);

	void requestShortcutChange();
};

}

#endif
