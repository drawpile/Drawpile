// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_LARGEICONMENU_H
#define DESKTOP_WIDGETS_LARGEICONMENU_H
#include <QMenu>

namespace widgets {

class LargeIconMenu final : public QMenu {
	Q_OBJECT
public:
	LargeIconMenu(QWidget *parent = nullptr);
};

}

#endif
