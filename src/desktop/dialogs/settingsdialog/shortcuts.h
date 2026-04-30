// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTS_H
#include "libclient/utils/debouncetimer.h"
#include <QItemEditorFactory>
#include <QWidget>

class BrushShortcutFilterProxyModel;
class BrushShortcutModel;
class CanvasShortcutsModel;
class CustomShortcutModel;
class QCommandLinkButton;
class QStyledItemDelegate;
class QTabWidget;
class QVBoxLayout;

namespace config {
class Config;
}

namespace widgets {
class CommandLinkButton;
}

namespace dialogs {
namespace settingsdialog {

class ProportionalTableView;
class ShortcutFilterInput;

class Shortcuts final : public QWidget {
	Q_OBJECT
public:
	Shortcuts(config::Config *cfg, QWidget *parent = nullptr);

	void initiateFixShortcutConflicts();
	void initiateBrushShortcutChange(int presetId);

	void showActionTab();
	void showCanvasTab();
	void showBrushTab();

public slots:
	void finishEditing();

private:
	static constexpr int OVERVIEW_TAB = 0;
	static constexpr int ACTION_TAB = 1;
	static constexpr int BRUSH_TAB = 2;
	static constexpr int CANVAS_TAB = 3;

	QWidget *initActionShortcuts(
		config::Config *cfg, QStyledItemDelegate *keySequenceDelegate);

	QWidget *initBrushShortcuts(QStyledItemDelegate *keySequenceDelegate);

	QWidget *initCanvasShortcuts(config::Config *cfg);

	QWidget *initOverview();

	void updateConflicts();
	void updateTabTexts();

	static QString actionTabText();
	static QString brushTabText();
	static QString canvasTabText();
	static QString searchResultText(const QString &text, int results);

	ShortcutFilterInput *m_filter = nullptr;
	QTabWidget *m_tabs = nullptr;
	widgets::CommandLinkButton *m_actionButton = nullptr;
	widgets::CommandLinkButton *m_brushButton = nullptr;
	widgets::CommandLinkButton *m_canvasButton = nullptr;
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

}
}

#endif
