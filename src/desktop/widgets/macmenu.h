/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef MACMENU_H
#define MACMENU_H

#include <QMenuBar>

class MainWindow;

class MacMenu final : public QMenuBar
{
	Q_OBJECT
public:
	static MacMenu *instance();

	void updateRecentMenu();

	void addWindow(MainWindow *win);
	void removeWindow(MainWindow *win);
	void updateWindow(MainWindow *win);

	QMenu *windowMenu() { return _windows; }

signals:

public slots:
	void newDocument();
	void openDocument();
	void joinSession();
	void quitAll();

private slots:
	void openRecent(QAction *action);

	void winMinimize();
	void winSelect(QAction *a);
	void updateWinMenu();

private:
	MacMenu();
	QAction *makeAction(QMenu *menu, const char *name, const QString &text, const QKeySequence &shortcut);

	QMenu *_recent;
	QMenu *_windows;
};

#endif // MACMENU_H
