/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "chatlineedit.h"
#include "chatwidgetpinnedarea.h"
#include "chatwidget.h"
#include "utils/html.h"
#include "utils/funstuff.h"
#include "notifications.h"

#include "../libshared/net/meta.h"
#include "canvas/userlist.h"

#include <QResizeEvent>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QTextBlock>
#include <QScrollBar>
#include <QTabBar>
#include <QIcon>
#include <QMenu>
#include <QSettings>

#include <memory>

namespace widgets {

struct Chat {
	QTextDocument *doc;
	int lastAppendedId = 0;
	qint64 lastMessageTs = 0;
	int scrollPosition = 0;

	Chat() : doc(nullptr) { }
	explicit Chat(QObject *parent)
		: doc(new QTextDocument(parent))
	{
		doc->setDefaultStyleSheet(
			".sep { background: #4d4d4d }"
			".notification { background: #232629 }"
			".message, .notification {"
				"color: #eff0f1;"
				"margin: 1px 0 1px 0"
			"}"
			".shout { background: #34292c }"
			".shout .tab { background: #da4453 }"
			".action { font-style: italic }"
			".username { font-weight: bold }"
			".trusted { color: #27ae60 }"
			".registered { color: #16a085 }"
			".op { color: #f47750 }"
			".mod { color: #ed1515 }"
			".timestamp { color: #8d8d8d }"
			"a:link { color: #1d99f3 }"
		);
	}

	void appendSeparator(QTextCursor &cursor);
	void appendMessage(int userId, const QString &usernameSpan, const QString &message, bool shout);
	void appendMessageCompact(int userId, const QString &usernameSpan, const QString &message, bool shout);
	void appendAction(const QString &usernameSpan, const QString &message);
	void appendNotification(const QString &message);
};

struct ChatWidget::Private {
	Private(ChatWidget *parent) : chatbox(parent) { }

	ChatWidget * const chatbox;
	QTextBrowser *view = nullptr;
	ChatLineEdit *myline = nullptr;
	ChatWidgetPinnedArea *pinned = nullptr;
	QTabBar *tabs = nullptr;

	QList<int> announcedUsers;
	canvas::UserListModel *userlist = nullptr;
	QHash<int, Chat> chats;

	int myId = 0;
	int currentChat = 0;

	bool preserveChat = true;
	bool compactMode = false;
	bool isAttached = true;

	QString usernameSpan(int userId);

	bool isAtEnd() const {
		return view->verticalScrollBar()->value() == view->verticalScrollBar()->maximum();
	}

	void scrollToEnd(int ifCurrentId) {
		if(ifCurrentId == tabs->tabData(tabs->currentIndex()).toInt())
			view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
	}

	inline Chat &publicChat()
	{
		Q_ASSERT(chats.contains(0));
		return chats[0];
	}

	bool ensurePrivateChatExists(int userId, QObject *parent);

	void updatePreserveModeUi();
};

ChatWidget::ChatWidget(QWidget *parent)
	: QWidget(parent), d(new Private(this))
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setMargin(0);

	d->tabs = new QTabBar(this);
	d->tabs->addTab(QString());
	d->tabs->setTabText(0, tr("Public"));
	d->tabs->setAutoHide(true);
	d->tabs->setDocumentMode(true);
	d->tabs->setTabsClosable(true);
	d->tabs->setMovable(true);
	d->tabs->setTabData(0, 0); // context id 0 is used for the public chat

	// The public chat cannot be closed
	if(d->tabs->tabButton(0, QTabBar::LeftSide)) {
		d->tabs->tabButton(0, QTabBar::LeftSide)->deleteLater();
		d->tabs->setTabButton(0, QTabBar::LeftSide, nullptr);
	}
	if(d->tabs->tabButton(0, QTabBar::RightSide)) {
		d->tabs->tabButton(0, QTabBar::RightSide)->deleteLater();
		d->tabs->setTabButton(0, QTabBar::RightSide, nullptr);
	}

	connect(d->tabs, &QTabBar::currentChanged, this, &ChatWidget::chatTabSelected);
	connect(d->tabs, &QTabBar::tabCloseRequested, this, &ChatWidget::chatTabClosed);
	layout->addWidget(d->tabs, 0);

	d->pinned = new ChatWidgetPinnedArea(this);
	layout->addWidget(d->pinned, 0);

	d->view = new QTextBrowser(this);
	d->view->setOpenExternalLinks(true);

	d->view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(d->view, &QTextBrowser::customContextMenuRequested, this, &ChatWidget::showChatContextMenu);

	layout->addWidget(d->view, 1);

	d->myline = new ChatLineEdit(this);
	layout->addWidget(d->myline);

	setLayout(layout);

	connect(d->myline, &ChatLineEdit::returnPressed, this, &ChatWidget::sendMessage);

	d->chats[0] = Chat(this);
	d->view->setDocument(d->chats[0].doc);

	setPreserveMode(false);

	d->compactMode = QSettings().value("history/compactchat").toBool();
}

ChatWidget::~ChatWidget()
{
	delete d;
}

void ChatWidget::setAttached(bool isAttached)
{
	d->isAttached = isAttached;
}

void ChatWidget::Private::updatePreserveModeUi()
{
	const bool preserve = preserveChat && currentChat == 0;

	QString placeholder, color;
	if(preserve) {
		placeholder = tr("Chat (recorded)...");
		color = "#da4453";
	} else {
		placeholder = tr("Chat...");
		color = "#1d99f3";
	}

	// Set placeholder text and window style based on the mode
	myline->setPlaceholderText(placeholder);

	chatbox->setStyleSheet(
		QStringLiteral(
		"QTextEdit, QLineEdit {"
			"background-color: #232629;"
			"border: none;"
			"color: #eff0f1"
		"}"
		"QLineEdit {"
			"border-top: 1px solid %1;"
			"padding: 4px"
		"}"
		).arg(color)
	);

}
void ChatWidget::setPreserveMode(bool preservechat)
{
	d->preserveChat = preservechat;
	d->updatePreserveModeUi();
}

void ChatWidget::loggedIn(int myId)
{
	d->myId = myId;
	d->announcedUsers.clear();
}

void ChatWidget::focusInput()
{
	d->myline->setFocus();
}

void ChatWidget::setUserList(canvas::UserListModel *userlist)
{
	 d->userlist = userlist;
}

void ChatWidget::clear()
{
	Chat &chat = d->chats[d->currentChat];
	chat.doc->clear();
	chat.lastAppendedId = 0;

	// Re-add avatars
	if(d->userlist) {
		for(const auto &u : d->userlist->users()) {
			chat.doc->addResource(
				QTextDocument::ImageResource,
				QUrl(QStringLiteral("avatar://%1").arg(u.id)),
				u.avatar
			);
		}
	}
}

bool ChatWidget::Private::ensurePrivateChatExists(int userId, QObject *parent)
{
	if(userId < 1 || userId > 255) {
		qWarning("ChatWidget::openPrivateChat(%d): Invalid user ID", userId);
		return false;
	}
	if(userId == myId) {
		qWarning("ChatWidget::openPrivateChat(%d): this is me...", userId);
		return false;
	}

	if(!chats.contains(userId)) {
		chats[userId] = Chat(parent);
		const int newTab = tabs->addTab(userlist->getUsername(userId));
		tabs->setTabData(newTab, userId);

		chats[userId].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(userId)),
			userlist->getUserById(userId).avatar
		);
		chats[userId].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(myId)),
			userlist->getUserById(myId).avatar
		);
	}

	return true;
}

void ChatWidget::openPrivateChat(int userId)
{
	if(!d->ensurePrivateChatExists(userId, this))
		return;

	for(int i=d->tabs->count()-1;i>=0;--i) {
		if(d->tabs->tabData(i).toInt() == userId) {
			d->tabs->setCurrentIndex(i);
			break;
		}
	}
}

static QString timestamp()
{
	return QStringLiteral("<span class=ts>%1</span>").arg(
		QDateTime::currentDateTime().toString("HH:mm")
	);
}

QString ChatWidget::Private::usernameSpan(int userId)
{
	const canvas::User user = userlist ? userlist->getUserById(userId) : canvas::User();

	QString userclass;
	if(user.isMod)
		userclass = QStringLiteral("mod");
	else if(user.isOperator)
		userclass = QStringLiteral("op");
	else if(user.isTrusted)
		userclass = QStringLiteral("trusted");
	else if(user.isAuth)
		userclass = QStringLiteral("registered");

	return QStringLiteral("<span class=\"username %1\">%2</span>").arg(
		userclass,
		user.name.isEmpty() ? QStringLiteral("<s>User #%1</s>").arg(userId) : user.name.toHtmlEscaped()
	);
}

void Chat::appendSeparator(QTextCursor &cursor)
{
	cursor.insertHtml(QStringLiteral(
		"<table height=1 width=\"100%\" class=sep><tr><td></td></tr></table>"
		));
}

void Chat::appendMessageCompact(int userId, const QString &usernameSpan, const QString &message, bool shout)
{
	Q_UNUSED(userId);

	lastAppendedId = -1;

	QTextCursor cursor(doc);

	cursor.movePosition(QTextCursor::End);

	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=\"message%1\">"
		"<tr>"
			"<td width=3 class=tab></td>"
			"<td>%2: %3</td>"
			"<td class=timestamp align=right>%4</td>"
		"</tr>"
		"</table>"
		).arg(
			shout ? QStringLiteral(" shout") : QString(),
			usernameSpan,
			message,
			timestamp()
		)
	);
}

void Chat::appendMessage(int userId, const QString &usernameSpan, const QString &message, bool shout)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	const qint64 ts = QDateTime::currentMSecsSinceEpoch();

	if(shout) {
		lastAppendedId = -2;

	} else if(lastAppendedId != userId) {
		appendSeparator(cursor);
		lastAppendedId = userId;

	} else if(ts - lastMessageTs < 60000) {
		QTextBlock b = doc->lastBlock().previous();
		cursor.setPosition(b.position() + b.length() - 1);

		cursor.insertHtml(QStringLiteral("<br>"));
		cursor.insertHtml(message);

		return;
	}

	// We'll have to make do with a very limited subset of HTML and CSS:
	// http://doc.qt.io/qt-5/richtext-html-subset.html
	// Embedding a whole browser engine just to render the chat widget would
	// be excessive.
	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=\"message%1\">"
		"<tr>"
			"<td width=3 rowspan=2 class=tab></td>"
			"<td width=40 rowspan=2><img src=\"avatar://%2\"></td>"
			"<td>%3</td>"
			"<td class=timestamp align=right>%4</td>"
		"</tr>"
		"<tr>"
			"<td colspan=2>%5</td>"
		"</tr>"
		"</table>"
		).arg(
			shout ? QStringLiteral(" shout") : QString(),
			QString::number(userId),
			usernameSpan,
			timestamp(),
			htmlutils::newlineToBr(message)
		)
	);
	lastMessageTs = ts;
}

void Chat::appendAction(const QString &usernameSpan, const QString &message)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	if(lastAppendedId != -1) {
		appendSeparator(cursor);
		lastAppendedId = -1;
	}
	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=message>"
		"<tr>"
			"<td width=3 class=tab></td>"
			"<td><span class=action>%1 %2</span></td>"
			"<td class=timestamp align=right>%3</td>"
		"</tr>"
		"</table>"
		).arg(
			usernameSpan,
			message,
			timestamp()
		)
	);
}

void Chat::appendNotification(const QString &message)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	if(lastAppendedId != 0) {
		appendSeparator(cursor);
		lastAppendedId = 0;
	}

	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=notification><tr>"
			"<td width=3 class=tab></td>"
			"<td>%1</td>"
			"<td align=right class=timestamp>%2</td>"
		"</tr></table>"
		).arg(
			htmlutils::newlineToBr(message),
			timestamp()
		)
	);
}

void ChatWidget::userJoined(int id, const QString &name)
{
	Q_UNUSED(name);

	if(d->userlist) {
		d->chats[0].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(id)),
			d->userlist->getUserById(id).avatar
		);
		if(d->chats.contains(id)) {
			d->chats[id].doc->addResource(
				QTextDocument::ImageResource,
				QUrl(QStringLiteral("avatar://%1").arg(id)),
				d->userlist->getUserById(id).avatar
			);
		}

	} else {
		qWarning("User #%d logged in, but userlist object not assigned to ChatWidget!", id);
	}

	// The server resends UserJoin messages during session reset.
	// We don't need to see the join messages again.
	if(d->announcedUsers.contains(id))
		return;

	d->announcedUsers << id;
	const QString msg = tr("%1 joined the session").arg(d->usernameSpan(id));
	const bool wasAtEnd = d->isAtEnd();

	d->publicChat().appendNotification(msg);
	if(wasAtEnd)
		d->scrollToEnd(0);

	if(d->chats.contains(id)) {
		d->chats[id].appendNotification(msg);
		if(wasAtEnd)
			d->scrollToEnd(id);
	}

	notification::playSound(notification::Event::LOGIN);
}

void ChatWidget::userParted(int id)
{
	QString msg = tr("%1 left the session").arg(d->usernameSpan(id));
	const bool wasAtEnd = d->isAtEnd();

	d->publicChat().appendNotification(msg);
	if(wasAtEnd)
		d->scrollToEnd(0);

	if(d->chats.contains(id)) {
		d->chats[id].appendNotification(msg);
		if(wasAtEnd)
			d->scrollToEnd(id);
	}

	d->announcedUsers.removeAll(id);

	notification::playSound(notification::Event::LOGOUT);
}

void ChatWidget::kicked(const QString &kickedBy)
{
	const bool wasAtEnd = d->isAtEnd();
	d->publicChat().appendNotification(tr("You have been kicked by %1").arg(kickedBy.toHtmlEscaped()));
	if(wasAtEnd)
		d->scrollToEnd(0);
}

void ChatWidget::receiveMessage(const protocol::MessagePtr &msg)
{
	const bool wasAtEnd = d->isAtEnd();
	int chatId = 0;

	if(msg->type() == protocol::MSG_CHAT) {
		const protocol::Chat &chat = msg.cast<protocol::Chat>();
		const QString safetext = chat.message().toHtmlEscaped();

		if(chat.isPin()) {
			d->pinned->setPinText(safetext);
		} else if(chat.isAction()) {
			d->publicChat().appendAction(d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext));

		} else {
			if(d->compactMode)
				d->publicChat().appendMessageCompact(msg->contextId(), d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext), chat.isShout());
			else
				d->publicChat().appendMessage(msg->contextId(), d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext), chat.isShout());
		}

	} else if(msg->type() == protocol::MSG_PRIVATE_CHAT) {
		const protocol::PrivateChat &chat = msg.cast<protocol::PrivateChat>();
		const QString safetext = chat.message().toHtmlEscaped();

		if(chat.target() != d->myId && chat.contextId() != d->myId) {
			qWarning("ChatWidget::recivePrivateMessage: message was targeted to user %d, but our ID is %d", chat.target(), d->myId);
			return;
		}

		// The server echoes back the messages we send
		chatId = chat.target() == d->myId ? chat.contextId() : chat.target();

		if(!d->ensurePrivateChatExists(chatId, this))
			return;

		Chat &c = d->chats[chatId];

		if(chat.isAction()) {
			c.appendAction(d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext));

		} else {
			if(d->compactMode)
				c.appendMessageCompact(msg->contextId(), d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext), false);
			else
				c.appendMessage(msg->contextId(), d->usernameSpan(msg->contextId()), htmlutils::linkify(safetext), false);
		}

	} else {
		qWarning("ChatWidget::receiveMessage: got wrong message type %s!", qPrintable(msg->messageName()));
		return;
	}

	if(chatId != d->currentChat) {
		for(int i=0;i<d->tabs->count();++i) {
			if(d->tabs->tabData(i).toInt() == chatId) {
				d->tabs->setTabTextColor(i, QColor(218, 68, 83));
				break;
			}
		}
	}

	if(!d->myline->hasFocus() || chatId != d->currentChat)
		notification::playSound(notification::Event::CHAT);

	if(wasAtEnd)
		d->scrollToEnd(chatId);
}

void ChatWidget::receiveMarker(int id, const QString &message)
{
	const bool wasAtEnd = d->isAtEnd();

	d->publicChat().appendNotification(QStringLiteral(
		"<img src=\"theme:flag-red.svg\"> %1: %2"
		).arg(
			d->usernameSpan(id),
			htmlutils::linkify(message.toHtmlEscaped())
		)
	);

	if(wasAtEnd)
		d->scrollToEnd(0);
}

void ChatWidget::systemMessage(const QString& message, bool alert)
{
	Q_UNUSED(alert);
	const bool wasAtEnd = d->isAtEnd();
	d->publicChat().appendNotification(message.toHtmlEscaped());
	if(wasAtEnd)
		d->scrollToEnd(0);
}

void ChatWidget::sendMessage(const QString &msg)
{
	if(msg.at(0) == '/') {
		// Special commands

		int split = msg.indexOf(' ');
		if(split<0)
			split = msg.length();

		const QString cmd = msg.mid(1, split-1).toLower();
		const QString params = msg.mid(split).trimmed();

		if(cmd == "clear") {
			clear();
			return;

		} else if(cmd.at(0)=='!' && d->currentChat == 0) {
			if(msg.length() > 2)
				emit message(protocol::Chat::announce(d->myId, msg.mid(2)));
			return;

		} else if(cmd == "me") {
			if(!params.isEmpty()) {
				if(d->currentChat == 0)
					emit message(protocol::Chat::action(d->myId, params, !d->preserveChat));
				else
					emit message(protocol::PrivateChat::action(d->myId, d->currentChat, params));
			}
			return;

		} else if(cmd == "pin" && d->currentChat == 0) {
			if(!params.isEmpty())
				emit message(protocol::Chat::pin(d->myId, params));
			return;

		} else if(cmd == "unpin" && d->currentChat == 0) {
			emit message(protocol::Chat::pin(d->myId, QStringLiteral("-")));
			return;

		} else if(cmd == "roll") {
			utils::DiceRoll result = utils::diceRoll(params.isEmpty() ? QStringLiteral("1d6") : params);
			if(result.number>0) {
				if(d->currentChat == 0)
					emit message(protocol::Chat::action(d->myId, "rolls " + result.toString(), !d->preserveChat));
				else
					emit message(protocol::PrivateChat::action(d->myId, d->currentChat, "rolls " + result.toString()));
			} else
				systemMessage(tr("Invalid dice roll description"));
			return;

		} else if(cmd == "help") {
			const QString text = QStringLiteral(
				"Available client commands:\n"
				"/help - show this message\n"
				"/clear - clear chat window\n"
				"/! <text> - make an announcement (recorded in session history)\n"
				"/me <text> - send action type message\n"
				"/pin <text> - pin a message to the top of the chat box (Ops only)\n"
				"/unpin - remove pinned message\n"
				"/roll [AdX] - roll dice"
			);
			systemMessage(text);
			return;
		}

	}

	// A normal chat message
	if(d->currentChat == 0)
		emit message(protocol::Chat::regular(d->myId, msg, !d->preserveChat));
	else
		emit message(protocol::PrivateChat::regular(d->myId, d->currentChat, msg));
}

void ChatWidget::chatTabSelected(int index)
{
	d->chats[d->currentChat].scrollPosition = d->view->verticalScrollBar()->value();

	const int id = d->tabs->tabData(index).toInt();
	Q_ASSERT(d->chats.contains(id));
	d->view->setDocument(d->chats[id].doc);
	d->view->verticalScrollBar()->setValue(d->chats[id].scrollPosition);
	d->tabs->setTabTextColor(index, QColor());
	d->currentChat = d->tabs->tabData(index).toInt();
	d->updatePreserveModeUi();
}

void ChatWidget::chatTabClosed(int index)
{
	const int id = d->tabs->tabData(index).toInt();
	Q_ASSERT(d->chats.contains(id));
	if(id == 0) {
		// Can't close the public chat
		return;
	}

	d->tabs->removeTab(index);

	delete d->chats[id].doc;
	d->chats.remove(id);
}

void ChatWidget::showChatContextMenu(const QPoint &pos)
{
	auto menu = std::unique_ptr<QMenu>(d->view->createStandardContextMenu());

	menu->addSeparator();

	menu->addAction(tr("Clear"), this, &ChatWidget::clear);

	auto compact = menu->addAction(tr("Compact mode"), this, &ChatWidget::setCompactMode);
	compact->setCheckable(true);
	compact->setChecked(d->compactMode);

	if(d->isAttached) {
		menu->addAction(tr("Detach"), this, &ChatWidget::detachRequested);
	} else {
		QWidget *win = parentWidget();
		while(win->parent() != nullptr)
			win = win->parentWidget();

		menu->addAction(tr("Attach"), win, &QWidget::close);
	}

	menu->exec(d->view->mapToGlobal(pos));
}

void ChatWidget::setCompactMode(bool compact)
{
	d->compactMode = compact;
	QSettings().setValue("history/compactchat", compact);
}

}

