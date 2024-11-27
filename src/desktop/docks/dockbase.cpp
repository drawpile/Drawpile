// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/dockbase.h"
#include <QAction>
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
	toggleViewAction()->setText(m_fullTitle);
#ifdef Q_OS_MACOS
	initConnection();
#endif
	connect(this, &DockBase::topLevelChanged, this, &DockBase::adjustTitle);
	connect(
		this, &DockBase::windowTitleChanged, this,
		&DockBase::fixViewToggleActionTitle, Qt::QueuedConnection);
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

void DockBase::showEvent(QShowEvent *event)
{
	QDockWidget::showEvent(event);
	if(isFloating()) {
		adjustTitle(true);
	}
}

void DockBase::hideEvent(QHideEvent *event)
{
	QDockWidget::hideEvent(event);
	adjustTitle(false);
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

void DockBase::adjustTitle(bool floating)
{
	setWindowTitle(floating ? m_fullTitle : m_shortTitle);
}

void DockBase::fixViewToggleActionTitle(const QString &title)
{
	if(title != m_fullTitle) {
		toggleViewAction()->setText(m_fullTitle);
	}
}

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
