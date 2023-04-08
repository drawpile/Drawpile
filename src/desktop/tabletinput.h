// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TABLETINPUT_H
#define TABLETINPUT_H

#include <QString>

class QApplication;
class QSettings;

namespace tabletinput {

void init(QApplication *app, const QSettings &cfg);

void update(const QSettings &cfg);

QString current();

}

#endif
