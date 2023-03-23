// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/chat/chatbox.h"
#include "desktop/chat/chatwidget.h"
#include "desktop/chat/chatwindow.h"
#include "desktop/chat/useritemdelegate.h"
#include "libclient/document.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/drawdance/message.h"
#include "libclient/net/client.h"

#include <QResizeEvent>
#include <QSplitter>
#include <QListView>
#include <QVBoxLayout>
#include <QMetaObject>

namespace widgets {

ChatBox::ChatBox(Document *doc, QWidget *parent)
	: QWidget(parent)
{
	QSplitter *chatsplitter = new QSplitter(Qt::Horizontal, this);
	chatsplitter->setChildrenCollapsible(false);
	m_chatWidget = new ChatWidget(this);
	chatsplitter->addWidget(m_chatWidget);

	m_userList = new QListView(this);
	m_userList->setSelectionMode(QListView::NoSelection);
	m_userItemDelegate = new UserItemDelegate(this);
	m_userItemDelegate->setDocument(doc);
	m_userList->setItemDelegate(m_userItemDelegate);
	chatsplitter->addWidget(m_userList);

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

	connect(doc, &Document::canvasChanged, this, &ChatBox::onCanvasChanged);
	connect(doc, &Document::serverLoggedIn, this, &ChatBox::onServerLogin);

	connect(doc, &Document::sessionPreserveChatChanged, m_chatWidget, &ChatWidget::setPreserveMode);
	connect(doc->client(), &net::Client::serverMessage, m_chatWidget, &ChatWidget::systemMessage);
	connect(doc->client(), &net::Client::youWereKicked, m_chatWidget, &ChatWidget::kicked);

	connect(m_userItemDelegate, &widgets::UserItemDelegate::opCommand, doc->client(), &net::Client::sendMessage);
	connect(m_userItemDelegate, &widgets::UserItemDelegate::requestPrivateChat, m_chatWidget, &ChatWidget::openPrivateChat);
	connect(m_userItemDelegate, &widgets::UserItemDelegate::requestUserInfo, this, &ChatBox::requestUserInfo);
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
