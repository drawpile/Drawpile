// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_NONALTSTEALINGMENUBAR_H
#define DESKTOP_WIDGETS_NONALTSTEALINGMENUBAR_H
#include <QMenuBar>

namespace widgets {

// A menu bar that doesn't take focus when the Alt key is pressed. We handle
// menu bar shortcuts as action shortcuts instead.
class NonAltStealingMenuBar : public QMenuBar {
    Q_OBJECT
public:
	explicit NonAltStealingMenuBar(QWidget *parent = nullptr);
};

}

#endif
