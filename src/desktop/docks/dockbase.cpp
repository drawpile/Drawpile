// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/dockbase.h"

namespace docks {

DockBase::DockBase(QWidget *parent)
	: QDockWidget{parent}
{
#ifdef Q_OS_MACOS
	initConnection();
#endif
}

DockBase::DockBase(QString title, QWidget *parent)
	: QDockWidget{title, parent}
{
#ifdef Q_OS_MACOS
	initConnection();
#endif
}

#ifdef Q_OS_MACOS
void DockBase::addWindowDecorations(bool topLevel)
{
	if(topLevel) {
		setWindowFlag(Qt::FramelessWindowHint, false);
		show();
	}
}

void DockBase::initConnection()
{
	connect(
		this, &QDockWidget::topLevelChanged, this,
		&DockBase::addWindowDecorations);
}
#endif

}
