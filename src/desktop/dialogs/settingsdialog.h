// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_H

#include <QDialog>

class QStackedWidget;

namespace desktop { namespace settings { class Settings; } }

namespace dialogs {

class SettingsDialog final : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(QWidget *parent=nullptr);

protected:
	bool event(QEvent *event) override;

private:
	void activatePanel(QWidget *panel);
	void addPanel(QWidget *panel);

	desktop::settings::Settings &m_settings;
#ifdef Q_OS_MACOS
	QWidget *m_current = nullptr;
#else
	QStackedWidget *m_stack;
#endif
};

}

#endif

