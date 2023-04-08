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
		//! The default maximum number of recent files to remember
		static const int DEFAULT_MAXFILES = 6;

		//! Add a new entry to the list of recent files
		static void addFile(const QString& file);

		//! Set the how many files to store
		static void setMaxFileCount(int max);

		//! Get how many files to store
		static int getMaxFileCount();

		//! Fill a QMenu with filenames
		static void initMenu(QMenu *menu);
};

#endif

