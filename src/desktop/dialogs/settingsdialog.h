// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_H
#include <QDialog>

class QButtonGroup;
class QStackedWidget;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {

class SettingsDialog final : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(bool singleSession, QWidget *parent = nullptr);
	~SettingsDialog() override;

	void activateShortcutsPanel();

private:
	void activatePanel(QWidget *panel);
	void addPanel(QWidget *panel);

	desktop::settings::Settings &m_settings;
	QButtonGroup *m_group;
	QStackedWidget *m_stack;
};

}

#endif
