// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_MACMENU_H
#define DESKTOP_WIDGETS_MACMENU_H
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
	void joinSession();
	void browseSessions();
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
		const QKeySequence &shortcut);
	dialogs::StartDialog *showStartDialog();

	QMenu *m_recent;
	QMenu *m_windows;
};

#endif
