/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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

#include <QStringList>
#include <QFileInfo>
#include <QSettings>
#include <QMenu>

#include "desktop/utils/recentfiles.h"

/**
 * @param filename filename to add
 */
void RecentFiles::addFile(const QString& filename)
{
	const int maxrecent = getMaxFileCount();

	QSettings cfg;
	cfg.beginGroup("history");

	QStringList files = cfg.value("recentfiles").toStringList();
	files.removeAll(filename);
	files.prepend(filename);
	while (files.size() > maxrecent)
		files.removeLast();

	cfg.setValue("recentfiles", files);

}

/**
 * @param max maximum number of filenames that can be stored
 */
void RecentFiles::setMaxFileCount(int max)
{
	QSettings cfg;
	cfg.setValue("history/maxrecentfiles",max);
}

/**
 * @return maximum number of filenames stored
 */
int RecentFiles::getMaxFileCount()
{
	QSettings cfg;
	int maxrecent = cfg.value("history/maxrecentfiles").toInt();
	if(maxrecent<=0)
		maxrecent = DEFAULT_MAXFILES;
	return maxrecent;
}

/**
 * The full path is stored in the property "filepath".
 * If the list of recent files is empty, the menu is disabled. Actions in
 * the menu marked with the attribute "deletelater" are deleted later in
 * the event loop instead of immediately.
 * @param menu QMenu to fill
 */
void RecentFiles::initMenu(QMenu *menu)
{
	QSettings cfg;
	const QStringList files = cfg.value("history/recentfiles").toStringList();

	const QList<QAction*> actions = menu->actions();
	for(QAction *act : actions) {
		menu->removeAction(act);
		act->deleteLater();
	}

	// Generate menu contents
	menu->setEnabled(!files.isEmpty());
	int index = 1;
	for(const QString &filename : files) {
		const QFileInfo file(filename);
		QAction *a = menu->addAction(QString(index<10?"&%1. %2":"%1. %2").arg(index).arg(file.fileName()));
		a->setStatusTip(file.absoluteFilePath());
		a->setProperty("filepath",file.absoluteFilePath());
		++index;
	}
}


