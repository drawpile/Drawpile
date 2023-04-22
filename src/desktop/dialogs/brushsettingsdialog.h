// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BRUSHEDITORDIALOG_H
#define BRUSHEDITORDIALOG_H

#include "desktop/widgets/mypaintinput.h"
#include "libclient/brushes/brush.h"
#include <QDialog>
#include <dpengine/libmypaint/mypaint-brush-settings.h>

class QComboBox;
class QListWidgetItem;

namespace dialogs {

class BrushSettingsDialog final : public QDialog {
	Q_OBJECT
public:
	explicit BrushSettingsDialog(QWidget *parent = nullptr);
	~BrushSettingsDialog() override;

signals:
	void brushSettingsChanged(const brushes::ActiveBrush &brush);

public slots:
	void setForceEraseMode(bool forceEraseMode);
	void setStabilizerUseBrushSampleCount(bool useBrushSampleCount);
	void updateUiFromActiveBrush(const brushes::ActiveBrush &brush);

private slots:
	void categoryChanged(QListWidgetItem *current, QListWidgetItem *);

private:
	struct Private;
	Private *d;

	void buildDialogUi();
	QWidget *buildGeneralPageUi();
	QWidget *buildClassicSizePageUi();
	QWidget *buildClassicOpacityPageUi();
	QWidget *buildClassicHardnessPageUi();
	QWidget *buildClassicSmudgingPageUi();
	void buildClassicApplyToAllButton(widgets::CurveWidget *curve);
	QWidget *buildMyPaintPageUi(int setting);
	widgets::MyPaintInput *buildMyPaintInputUi(
		int setting, int input, const MyPaintBrushSettingInfo *settingInfo);

	void applyCurveToAllClassicSettings(const KisCubicCurve &curve);

	void
	addCategory(const QString &text, const QString &toolTip, int pageIndex);
	void addClassicCategories(bool withHardness);
	void addMyPaintCategories();

	void updateUiFromClassicBrush();
	void updateUiFromMyPaintBrush();
	void updateMyPaintSettingPage(int setting);
	void emitChange();

	static void setComboBoxIndexByData(QComboBox *combo, int data);
	static bool shouldIncludeMyPaintSetting(int setting);
	static bool disableIndirectMyPaintSetting(int setting);
	static bool disableIndirectMyPaintInputs(int setting);

	static QString getMyPaintInputTitle(int input);
	static QString getMyPaintInputDescription(int input);
	static QString getMyPaintSettingTitle(int setting);
	static QString getMyPaintSettingDescription(int setting);
};

}

#endif
