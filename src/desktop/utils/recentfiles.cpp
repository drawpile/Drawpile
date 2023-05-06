// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStringList>
#include <QFileInfo>
#include <QMenu>

#include "desktop/main.h"
#include "desktop/utils/recentfiles.h"
#include "libshared/util/paths.h"

/**
 * @param filename filename to add
 */
void RecentFiles::addFile(const QString& filename)
{
	auto &settings = dpApp().settings();
	auto files = settings.recentFiles();
	const auto maxrecent = settings.maxRecentFiles();
	files.removeAll(filename);
	files.prepend(filename);
	while (files.size() > maxrecent)
		files.removeLast();
	settings.setRecentFiles(files);
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
	const auto files = dpApp().settings().recentFiles();
	const QList<QAction*> actions = menu->actions();
	for(QAction *act : actions) {
		menu->removeAction(act);
		act->deleteLater();
	}

	// Generate menu contents
	menu->setEnabled(!files.isEmpty());
	int index = 1;
	for(const QString &filename : files) {
		QAction *a = menu->addAction(QString(index<10?"&%1. %2":"%1. %2")
			.arg(index).arg(utils::paths::extractBasename(filename)));
		QString absoluteFilePath = QFileInfo{filename}.absoluteFilePath();
		a->setStatusTip(absoluteFilePath);
		a->setProperty("filepath", absoluteFilePath);
		++index;
	}
}


