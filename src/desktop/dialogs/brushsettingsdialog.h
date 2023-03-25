#ifndef BRUSHEDITORDIALOG_H
#define BRUSHEDITORDIALOG_H

#include "libclient/brushes/brush.h"
#include "desktop/widgets/mypaintinput.h"
#include <dpengine/libmypaint/mypaint-brush-settings.h>
#include <QDialog>

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
	QWidget *buildMyPaintPageUi(int setting);
	widgets::MyPaintInput *buildMyPaintInputUi(
		int setting, int input, const MyPaintBrushSettingInfo *settingInfo);

	void
	addCategory(const QString &text, const QString &toolTip, int pageIndex);
	void addClassicCategories(bool withHardness);
	void addMyPaintCategories();

	void updateUiFromClassicBrush();
	void updateUiFromMyPaintBrush();
	void updateMyPaintSettingPage(int setting);
	void emitChange();

	static bool shouldIncludeMyPaintSetting(int setting);
	static bool shouldIncludeMyPaintInput(int input);

	static QString getMyPaintInputTitle(int input);
	static QString getMyPaintInputDescription(int input);
	static QString getMyPaintSettingTitle(int setting);
	static QString getMyPaintSettingDescription(int setting);
};

}

#endif
