// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_H
#include <QDialog>

class QButtonGroup;
class QDialogButtonBox;
class QStackedWidget;

namespace config {
class Config;
}

namespace dialogs {

namespace settingsdialog {
class Shortcuts;
}

class SettingsDialog final : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(
		bool singleSession, bool smallScreenMode, QWidget *parent = nullptr);
	~SettingsDialog() override;

	void initiateFixShortcutConflicts();
	void initiateBrushShortcutChange(int presetId);
	void activateUserInterfacePanel();
	void activateNetworkPanel();

signals:
	void scalingChangeRequested();
	void tabletTesterRequested();
	void touchTesterRequested();

private:
	settingsdialog::Shortcuts *activateShortcutsPanel();
	void activatePanel(QWidget *panel, QDialogButtonBox *buttons);
	void hidePanelButtons(QDialogButtonBox *buttons);
	void addPanel(QWidget *panel);

	config::Config *m_cfg;
	QButtonGroup *m_group;
	QStackedWidget *m_stack;
};

}

#endif
