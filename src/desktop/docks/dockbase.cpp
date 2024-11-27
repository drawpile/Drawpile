// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/dockbase.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QTabBar>
#include <QVariant>

namespace docks {

DockBase::DockBase(
	const QString &fullTitle, const QString &shortTitle, const QIcon &tabIcon,
	QWidget *parent)
	: QDockWidget(parent)
	, m_fullTitle(fullTitle)
	, m_shortTitle(shortTitle.isEmpty() ? fullTitle : shortTitle)
	, m_tabIcon(tabIcon)
{
	setWindowTitle(m_shortTitle);
	toggleViewAction()->setText(m_fullTitle);
#ifdef Q_OS_MACOS
	initConnection();
#endif
	connect(
		this, &DockBase::dockLocationChanged, this,
		&DockBase::tabUpdateRequested);
	connect(
		this, &DockBase::topLevelChanged, this,
		&DockBase::handleTopLevelChanged);
	connect(
		this, &DockBase::windowTitleChanged, this,
		&DockBase::fixViewToggleActionTitle, Qt::QueuedConnection);
}

void DockBase::setShowIcons(bool showIcons)
{
	if(showIcons != m_showIcons) {
		m_showIcons = showIcons;
		if(!isFloating()) {
			adjustTitle(false);
		}
	}
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

QWidget *DockBase::actualTitleBarWidget() const
{
	return m_originalTitleBarWidget ? m_originalTitleBarWidget
									: titleBarWidget();
}

void DockBase::setArrangeMode(bool arrangeMode)
{
	if(arrangeMode && !m_originalTitleBarWidget) {
		m_originalTitleBarWidget = titleBarWidget();
		QWidget *arrangeWidget = new QWidget;
		QHBoxLayout *arrangeLayout = new QHBoxLayout(arrangeWidget);
		arrangeLayout->setSpacing(0);
		arrangeLayout->setContentsMargins(2, 2, 1, 2);

		QLabel *arrangeLabel = new QLabel(tr("Drag here to arrange"));
		arrangeLabel->setAlignment(Qt::AlignCenter);
		arrangeLayout->addWidget(arrangeLabel, 1);

		widgets::GroupedToolButton *arrangeButton =
			new widgets::GroupedToolButton(
				widgets::GroupedToolButton::NotGrouped);
		arrangeButton->setIcon(QIcon::fromTheme("checkbox"));
		arrangeButton->setToolTip(tr("Finish arranging"));
		arrangeLayout->addWidget(arrangeButton);
		connect(
			arrangeButton, &widgets::GroupedToolButton::clicked, this,
			&DockBase::arrangingFinished);

		setTitleBarWidget(arrangeWidget);
	} else if(!arrangeMode && m_originalTitleBarWidget) {
		QWidget *arrangeWidget = titleBarWidget();
		setTitleBarWidget(m_originalTitleBarWidget);
		m_originalTitleBarWidget = nullptr;
		if(arrangeWidget) {
			arrangeWidget->deleteLater();
		}
	}
}

void DockBase::showEvent(QShowEvent *event)
{
	QDockWidget::showEvent(event);
	if(isFloating()) {
		adjustTitle(true);
	} else {
		emit tabUpdateRequested();
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

void DockBase::handleTopLevelChanged(bool floating)
{
	adjustTitle(floating);
	if(!floating) {
		emit tabUpdateRequested();
	}
}

void DockBase::adjustTitle(bool floating)
{
	setWindowTitle(
		floating	  ? m_fullTitle
		: m_showIcons ? QString()
					  : m_shortTitle);
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
