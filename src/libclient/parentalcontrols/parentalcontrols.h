// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_PARENTALCONTROLS_PARENTALCONTROLS_H
#define LIBCLIENT_PARENTALCONTROLS_PARENTALCONTROLS_H

#include <QObject>
#include <QPointer>

namespace libclient { namespace settings { class Settings; } }

class QString;

namespace parentalcontrols {
Q_NAMESPACE

enum class Level {
	Unrestricted, // Parental controls inactive
	NoList,       // NSFM sessions listings are hidden, but direct join is still possible
	NoJoin,       // Cannot join sessions tagged as NSFM
	Restricted    // Will autodisconnect from sessions that get tagged as NSFM
};

Q_ENUM_NS(Level)

/**
 * Initialize OS parental controls integration
 */
void init(libclient::settings::Settings &settings);

/**
 * @brief Are parental controls active on the operating system level?
 *
 * This overrides any in-application configuration
 */
bool isOSActive();

/**
 * @brief Are parental controls locked?
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
bool isNsfmAlias(const QString &alias);

/**
 * @brief Get the current parental controls level
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

extern QPointer<libclient::settings::Settings> g_settings;

}

#endif // LIBCLIENT_PARENTALCONTROLS_PARENTALCONTROLS_H

