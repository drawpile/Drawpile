// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host.h"
#include "desktop/dialogs/startdialog/host/session.h"
#include <QIcon>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Host::Host(QWidget *parent)
	: Page(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_tabs = new QTabBar;
	m_tabs->setExpanding(false);
	m_tabs->setDrawBase(false);
	layout->addWidget(m_tabs);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_stack, 1);

	m_sessionPage = new host::Session(false);
	m_tabs->addTab(QIcon::fromTheme("network-server"), tr("Session"));
	m_stack->addWidget(m_sessionPage);

	m_tabs->addTab(QIcon::fromTheme("edit-find"), tr("Listing"));
	m_stack->addWidget(new QWidget);

	m_tabs->addTab(QIcon::fromTheme("object-locked"), tr("Permissions"));
	m_stack->addWidget(new QWidget);

	m_tabs->addTab(QIcon::fromTheme("user-group-new"), tr("Roles"));
	m_stack->addWidget(new QWidget);

	m_tabs->addTab(QIcon::fromTheme("im-ban-kick-user"), tr("Bans"));
	m_stack->addWidget(new QWidget);

	connect(
		m_tabs, &QTabBar::currentChanged, m_stack,
		&QStackedWidget::setCurrentIndex);
}

void Host::activate()
{
	emit hideLinks();
	emit showButtons();
	// updateHostEnabled();
}

void Host::accept()
{
	// if(canHost()) {
	// 	bool advanced = m_advancedBox->isChecked();
	// 	emit host(
	// 		m_titleEdit->text().trimmed(), m_passwordEdit->text().trimmed(),
	// 		advanced ? m_idAliasEdit->text() : QString(),
	// 		m_nsfmBox->isChecked(),
	// 		advanced && m_announceBox->isChecked()
	// 			? m_listServerCombo->currentData().toString()
	// 			: QString{},
	// 		m_useGroup->checkedId() == USE_LOCAL ? QString{}
	// 											 : getRemoteAddress());
	// }
}

}
}
