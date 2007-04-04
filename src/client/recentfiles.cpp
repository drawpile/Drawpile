/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QStringList>
#include <QFileInfo>
#include <QSettings>
#include <QMenu>

#include "recentfiles.h"

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
	cfg.beginGroup("history");
	cfg.setValue("maxrecentfiles",max);
}

/**
 * @return maximum number of filenames stored
 */
int RecentFiles::getMaxFileCount()
{
	QSettings cfg;
	cfg.beginGroup("history");
	int maxrecent = cfg.value("maxrecentfiles").toInt();
	if(maxrecent<=0)
		maxrecent = 6;
	return maxrecent;
}

/**
 * The full path is stored in the property "path"
 * @param menu QMenu to fill
 */
void RecentFiles::initMenu(QMenu *menu)
{
	QSettings cfg;
	cfg.beginGroup("history");

	const QStringList files = cfg.value("recentfiles").toStringList();

	menu->clear();
	foreach(QString filename, files) {
		const QFileInfo file(filename);
		QAction *a = menu->addAction(file.fileName());
		a->setProperty("path",file.absoluteFilePath());
	}
}


