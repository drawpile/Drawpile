// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/dockbase.h"

namespace docks {

DockBase::DockBase(
	const QString &fullTitle, const QString &shortTitle, QWidget *parent)
	: QDockWidget(shortTitle.isEmpty() ? fullTitle : shortTitle, parent)
	, m_fullTitle(fullTitle)
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
