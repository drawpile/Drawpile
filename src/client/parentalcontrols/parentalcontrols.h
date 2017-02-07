/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef PARENTALCONTROLS_H
#define PARENTALCONTROLS_H

class QString;

namespace parentalcontrols {

enum class Level {
	Unrestricted, // parental controls inactive
	NoList,       // NSFM sessions listings are hidden, but direct join is still possible
	NoJoin,       // Cannot join sessions tagged as NSFM
	Restricted    // Will autodisconnect from sessions that get tagged as NSFM
};

/**
 * Initialize OS parental control integration
 */
void init();

/**
 * @brief Are parental controls active on the operating system level?
 *
 * This overrides any in-application configuration
 */
bool isOSActive();

/**
 * @brief Are parental control settings locked?
 *
 * This means options to access material tagged as "Not Safe For Minors" should be disabled.
 */
bool isLocked();

/**
 * @brief Get the default NSFM word list
 */
QString defaultWordList();

/**
 * @brief Check if the given title contains any words on the NSFM list
 */
bool isNsfmTitle(const QString &title);

/**
 * @brief Get the current parental control level
 *
 * If isOSActive() is true, the minimum level is NoJoin
 */
Level level();


}

#endif // PARENTALCONTROLS_H

