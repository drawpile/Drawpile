// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_UTILS_HTML_H
#define DP_UTILS_HTML_H

#include <QString>

namespace htmlutils {

/**
 * @brief Convert newlines to br:s
 * @param input
 * @return
 */
QString newlineToBr(const QString &input);

/**
 * @brief Take an input string and wrap all links in <a> tags
 *
 * @param input text to linkify
 * @param extra additional link tag attributes
 * @return text with links wrapped in a tags
 */
QString linkify(const QString &input, const QString &extra=QString());

}

#endif
