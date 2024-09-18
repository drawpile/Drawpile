// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_H
#include <QDialog>

class QButtonGroup;
class QDialogButtonBox;
class QStackedWidget;

namespace desktop {
namespace settings {
class Settings;
}
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

signals:
	void tabletTesterRequested();
	void touchTesterRequested();

private:
	settingsdialog::Shortcuts *activateShortcutsPanel();
	void activatePanel(QWidget *panel, QDialogButtonBox *buttons);
	void addPanel(QWidget *panel);

	desktop::settings::Settings &m_settings;
	QButtonGroup *m_group;
	QStackedWidget *m_stack;
};

}

#endif
