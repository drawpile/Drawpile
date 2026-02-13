// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_MACMENU_H
#define DESKTOP_WIDGETS_MACMENU_H
#include <QAction>
#include <QMenuBar>

class MainWindow;

namespace dialogs {
class StartDialog;
}

class MacMenu final : public QMenuBar {
	Q_OBJECT
public:
	static MacMenu *instance();

	void addWindow(MainWindow *win);
	void removeWindow(MainWindow *win);
	void updateWindow(MainWindow *win);

	QMenu *windowMenu() { return m_windows; }

public slots:
	void newDocument();
	void openDocument();
	void startDocument();
	void hostSession();
	void joinSession();
	void browseSessions();
	void checkForUpdates();
	void quitAll();

private slots:
	void openRecent(QAction *action);
	void showSettings();
	void winMinimize();
	void winSelect(QAction *a);
	void updateWinMenu();

private:
	MacMenu();
	QAction *makeAction(
		QMenu *menu, const char *name, const QString &text,
		const QKeySequence &shortcut = QKeySequence(),
		QAction::MenuRole menuRole = QAction::NoRole);
	QAction *makeAction(
		QMenu *menu, const char *name, const QString &text,
		QAction::MenuRole menuRole);
	dialogs::StartDialog *showStartDialogOnPage(int page);

	QMenu *m_recent;
	QMenu *m_windows;
};

#endif
