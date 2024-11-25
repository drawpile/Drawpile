// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/dockbase.h"
#include <QMainWindow>
#include <QTabBar>
#include <QVariant>

namespace docks {

DockBase::DockBase(
	const QString &fullTitle, const QString &shortTitle, QWidget *parent)
	: QDockWidget(parent)
	, m_fullTitle(fullTitle)
	, m_shortTitle(shortTitle.isEmpty() ? fullTitle : shortTitle)
{
	setWindowTitle(m_shortTitle);
#ifdef Q_OS_MACOS
	initConnection();
#endif
	connect(this, &DockBase::topLevelChanged, this, &DockBase::adjustTitle);
}

void DockBase::makeTabCurrent(bool toggled)
{
	if(toggled && !isFloating()) {
		int i;
		QTabBar *tabBar = searchTab(i);
		if(tabBar) {
			tabBar->setCurrentIndex(i);
		}
	}
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

QTabBar *DockBase::searchTab(int &outIndex)
{
	QWidget *widget = parentWidget();
	if(widget && qobject_cast<QMainWindow *>(widget)) {
		quintptr thisValue = quintptr(qobject_cast<QDockWidget *>(this));
		for(QTabBar *tabBar : widget->findChildren<QTabBar *>(
				QString(), Qt::FindDirectChildrenOnly)) {
			int count = tabBar->count();
			for(int i = 0; i < count; ++i) {
				quintptr value = tabBar->tabData(i).value<quintptr>();
				if(value == thisValue) {
					outIndex = i;
					return tabBar;
				}
			}
		}
	}
	return nullptr;
}

}
