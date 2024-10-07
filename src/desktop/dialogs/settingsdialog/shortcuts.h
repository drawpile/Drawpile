// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#include <QItemEditorFactory>
#include <QWidget>

class CustomShortcutModel;
class QLineEdit;
class QTabWidget;
class QVBoxLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class ProportionalTableView;

class Shortcuts final : public QWidget {
	Q_OBJECT
public:
	Shortcuts(desktop::settings::Settings &settings, QWidget *parent = nullptr);

public slots:
	void finishEditing();

private:
	QWidget *initActionShortcuts(
		desktop::settings::Settings &settings, QLineEdit *filter);

	QWidget *initCanvasShortcuts(
		desktop::settings::Settings &settings, QLineEdit *filter);

	QWidget *initTouchShortcuts(desktop::settings::Settings &settings);

	QTabWidget *m_tabs;
	CustomShortcutModel *m_globalShortcutsModel;
	QItemEditorFactory m_itemEditorFactory;
	ProportionalTableView *m_shortcutsTable;
};

} // namespace settingsdialog
} // namespace dialogs

#endif
