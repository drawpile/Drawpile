// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/chat/chatbox.h"
#include "desktop/chat/chatwidget.h"
#include "desktop/chat/chatwindow.h"
#include "desktop/chat/useritemdelegate.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/document.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/drawdance/message.h"
#include "libclient/net/client.h"

#include <QAction>
#include <QResizeEvent>
#include <QSplitter>
#include <QListView>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QPushButton>

namespace widgets {

ChatBox::ChatBox(Document *doc, QWidget *parent)
	: QWidget(parent)
{
	QSplitter *chatsplitter = new QSplitter(Qt::Horizontal, this);
	chatsplitter->setChildrenCollapsible(false);
	m_chatWidget = new ChatWidget(this);
	chatsplitter->addWidget(m_chatWidget);

	QWidget *sidebar = new QWidget{this};
	QVBoxLayout *sidebarLayout = new QVBoxLayout;
	sidebarLayout->setContentsMargins(0, 0, 0, 0);
	sidebarLayout->setSpacing(0);
	sidebar->setLayout(sidebarLayout);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	buttonsLayout->setSpacing(0);
	sidebarLayout->addLayout(buttonsLayout);

	buttonsLayout->addStretch();

	m_inviteButton = new GroupedToolButton{GroupedToolButton::GroupLeft, this};
	m_inviteButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_inviteButton->setText(tr("Invite"));
	buttonsLayout->addWidget(m_inviteButton);

	m_sessionSettingsButton = new GroupedToolButton{GroupedToolButton::GroupCenter, this};
	m_sessionSettingsButton->setIcon(QIcon::fromTheme("configure"));
	m_sessionSettingsButton->setText(tr("Session"));
	buttonsLayout->addWidget(m_sessionSettingsButton);

	m_chatMenuButton = new GroupedToolButton{GroupedToolButton::GroupRight, this};
	m_chatMenuButton->setIcon(QIcon::fromTheme("edit-comment"));
	m_chatMenuButton->setText(tr("Chat"));
	m_chatMenuButton->setStatusTip(tr("Show chat options"));
	m_chatMenuButton->setToolTip(m_chatMenuButton->statusTip());
	m_chatMenuButton->setPopupMode(QToolButton::InstantPopup);
	m_chatMenuButton->setMenu(m_chatWidget->externalMenu());
	buttonsLayout->addWidget(m_chatMenuButton);

	buttonsLayout->addStretch();

	m_userList = new QListView(this);
	m_userList->setSelectionMode(QListView::NoSelection);
	m_userItemDelegate = new UserItemDelegate(this);
	m_userItemDelegate->setDocument(doc);
	m_userList->setItemDelegate(m_userItemDelegate);
	sidebarLayout->addWidget(m_userList);

	chatsplitter->addWidget(sidebar);

	chatsplitter->setStretchFactor(0, 5);
	chatsplitter->setStretchFactor(1, 1);

	auto *layout = new QVBoxLayout;
	layout->addWidget(chatsplitter);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	connect(m_chatWidget, &ChatWidget::message, this, &ChatBox::message);
	connect(m_chatWidget, &ChatWidget::detachRequested, this, &ChatBox::detachFromParent);
	connect(m_chatWidget, &ChatWidget::expandRequested, this, [this]() {
		if(m_state == State::Collapsed) {
			emit expandPlease();
		}
	});
	connect(m_chatWidget, &ChatWidget::muteChanged, this, &ChatBox::muteChanged);

	connect(doc, &Document::canvasChanged, this, &ChatBox::onCanvasChanged);
	connect(doc, &Document::serverLoggedIn, this, &ChatBox::onServerLogin);
	connect(doc, &Document::compatibilityModeChanged, this, &ChatBox::onCompatibilityModeChanged);

	connect(doc, &Document::sessionPreserveChatChanged, m_chatWidget, &ChatWidget::setPreserveMode);
	connect(doc->client(), &net::Client::serverMessage, m_chatWidget, &ChatWidget::systemMessage);
	connect(doc->client(), &net::Client::youWereKicked, m_chatWidget, &ChatWidget::kicked);

	connect(m_userItemDelegate, &widgets::UserItemDelegate::opCommand, doc->client(), &net::Client::sendMessage);
	connect(m_userItemDelegate, &widgets::UserItemDelegate::requestPrivateChat, m_chatWidget, &ChatWidget::openPrivateChat);
	connect(m_userItemDelegate, &widgets::UserItemDelegate::requestUserInfo, this, &ChatBox::requestUserInfo);
}

void ChatBox::setActions(QAction *inviteAction, QAction *sessionSettingsAction)
{
	const QPair<QAction *, QAbstractButton *> pairs[] = {
		{inviteAction, m_inviteButton},
		{sessionSettingsAction, m_sessionSettingsButton},
	};
	for(const QPair<QAction *, QAbstractButton *> &pair : pairs) {
		QAction *action = pair.first;
		QAbstractButton *button = pair.second;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		connect(action, &QAction::enabledChanged, button, &QWidget::setEnabled);
#else
		connect(action, &QAction::changed, this, [=]{
			button->setEnabled(action->isEnabled());
		});
#endif
		connect(button, &QAbstractButton::clicked, action, &QAction::trigger);
		button->setIcon(action->icon());
		button->setStatusTip(action->statusTip());
		button->setToolTip(action->statusTip());
		button->setEnabled(action->isEnabled());
	}
}

void ChatBox::onCanvasChanged(canvas::CanvasModel *canvas)
{
	m_userList->setModel(canvas->userlist()->onlineUsers());
	m_chatWidget->setUserList(canvas->userlist());

	connect(canvas, &canvas::CanvasModel::chatMessageReceived, m_chatWidget, &ChatWidget::receiveMessage);
	connect(canvas, &canvas::CanvasModel::pinnedMessageChanged, m_chatWidget, &ChatWidget::setPinnedMessage);
	connect(canvas, &canvas::CanvasModel::userJoined, m_chatWidget, &ChatWidget::userJoined);
	connect(canvas, &canvas::CanvasModel::userLeft, m_chatWidget, &ChatWidget::userParted);
}

void ChatBox::onServerLogin()
{
	m_chatWidget->loggedIn(static_cast<Document*>(sender())->client()->myId());
}

void ChatBox::onCompatibilityModeChanged(bool compatibilityMode)
{
	m_userItemDelegate->setCompatibilityMode(compatibilityMode);
}

void ChatBox::focusInput()
{
	m_chatWidget->focusInput();
}

void ChatBox::detachFromParent()
{
	if(!parent() || qobject_cast<ChatWindow*>(parent()))
		return;

	m_state = State::Detached;

	const auto siz = size();

	QObject *oldParent = parent();

	m_chatWidget->setAttached(false);

	auto *window = new ChatWindow(this);
	connect(window, &ChatWindow::closing, this, &ChatBox::reattachNowPlease);
	connect(window, &ChatWindow::closing, this, &ChatBox::reattachToParent);
	connect(oldParent, &QObject::destroyed, window, &QObject::deleteLater);

	window->show();
	window->resize(siz);
}

void ChatBox::reattachToParent()
{
	m_state = State::Expanded;
	m_chatWidget->setAttached(true);
}

void ChatBox::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if(event->size().height() == 0) {
		if(m_state == State::Expanded) {
			emit expandedChanged(false);
			m_state = State::Collapsed;
		}

	} else if(m_state == State::Collapsed) {
		m_state = State::Expanded;
		emit expandedChanged(true);
	}
}

}
