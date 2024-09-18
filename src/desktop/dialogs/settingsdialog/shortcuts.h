// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#include <QItemEditorFactory>
#include <QWidget>

class BrushShortcutModel;
class CustomShortcutModel;
class QLineEdit;
class QStyledItemDelegate;
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
	static constexpr int ACTION_TAB = 0;
	static constexpr int BRUSH_TAB = 1;
	static constexpr int CANVAS_TAB = 2;

	QWidget *initActionShortcuts(
		desktop::settings::Settings &settings,
		QStyledItemDelegate *keySequenceDelegate);

	QWidget *initBrushShortcuts(QStyledItemDelegate *keySequenceDelegate);

	QWidget *initCanvasShortcuts(desktop::settings::Settings &settings);

	void updateTabTexts();

	static QString actionTabText();
	static QString brushTabText();
	static QString canvasTabText();
	static QString searchResultText(const QString &text, int results);

	QLineEdit *m_filterEdit = nullptr;
	QTabWidget *m_tabs = nullptr;
	CustomShortcutModel *m_globalShortcutsModel = nullptr;
	BrushShortcutModel *m_brushShortcutsModel = nullptr;
	ProportionalTableView *m_actionsTable = nullptr;
	ProportionalTableView *m_brushesTable = nullptr;
	ProportionalTableView *m_canvasTable = nullptr;
	QItemEditorFactory m_itemEditorFactory;
};

} // namespace settingsdialog
} // namespace dialogs

#endif
