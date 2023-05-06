// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RECENTFILES_H
#define RECENTFILES_H

class QMenu;
class QString;

/**
 * The RecentFiles class keeps a global list of recently opened and
 * saved files.
 */
class RecentFiles {
	public:
		//! Add a new entry to the list of recent files
		static void addFile(const QString& file);

		//! Fill a QMenu with filenames
		static void initMenu(QMenu *menu);
};

#endif

