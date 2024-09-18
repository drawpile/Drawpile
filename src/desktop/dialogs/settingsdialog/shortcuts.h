// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#include "libclient/utils/debouncetimer.h"
#include <QItemEditorFactory>
#include <QWidget>

class BrushShortcutModel;
class BrushShortcutFilterProxyModel;
class CanvasShortcutsModel;
class CustomShortcutModel;
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
class ShortcutFilterInput;

class Shortcuts final : public QWidget {
	Q_OBJECT
public:
	Shortcuts(desktop::settings::Settings &settings, QWidget *parent = nullptr);

	void initiateFixShortcutConflicts();
	void initiateBrushShortcutChange(int presetId);

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

	void updateConflicts();
	void updateTabTexts();

	static QString actionTabText();
	static QString brushTabText();
	static QString canvasTabText();
	static QString searchResultText(const QString &text, int results);

	ShortcutFilterInput *m_filter = nullptr;
	QTabWidget *m_tabs = nullptr;
	CustomShortcutModel *m_actionShortcutsModel = nullptr;
	CanvasShortcutsModel *m_canvasShortcutsModel = nullptr;
	BrushShortcutModel *m_brushShortcutsModel = nullptr;
	BrushShortcutFilterProxyModel *m_brushShortcutsFilterModel = nullptr;
	ProportionalTableView *m_actionsTable = nullptr;
	ProportionalTableView *m_brushesTable = nullptr;
	ProportionalTableView *m_canvasTable = nullptr;
	QItemEditorFactory m_itemEditorFactory;
	DebounceTimer m_shortcutConflictDebounce;
	bool m_updatingConflicts = false;
};

} // namespace settingsdialog
} // namespace dialogs

#endif
