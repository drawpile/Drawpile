// SPDX-License-Identifier: GPL-3.0-or-later

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
 *
 * If the pc/autotag setting is set to false (default is true), this will
 * always return false
 */
bool isNsfmTitle(const QString &title);

/**
 * @brief Get the current parental control level
 *
 * If isOSActive() is true, the minimum level is NoJoin
 */
Level level();

/**
 * @brief Is the uncensoring of layers blocked?
 *
 * Note: this also means the user can't censor any layers.
 */
bool isLayerUncensoringBlocked();

}

#endif // PARENTALCONTROLS_H

