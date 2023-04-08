// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

class QPalette;
class QString;

namespace colorscheme {

QPalette loadFromFile(const QString &filename);

bool saveToFile(const QString &filename, const QPalette &palette);

}

#endif

