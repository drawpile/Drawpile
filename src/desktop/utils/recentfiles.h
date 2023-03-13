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

