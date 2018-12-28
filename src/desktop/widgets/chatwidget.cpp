/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2018 Calle Laakkonen

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
#include "chatwidget.h"
#include "utils/html.h"
#include "utils/funstuff.h"
#include "notifications.h"

#include "../shared/net/meta.h"
#include "canvas/userlist.h"

#include <QDebug>
#include <QResizeEvent>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QTextBlock>
#include <QScrollBar>

namespace widgets {

ChatBox::ChatBox(QWidget *parent)
	: QWidget(parent),
	  m_userlist(nullptr),
	  m_wasCollapsed(false),
	  m_preserveChat(false),
	  m_myId(1),
	  m_lastAppendedId(0),
	  m_lastMessageTs(0)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setMargin(0);

	m_pinned = new QLabel(this);
	m_pinned->setVisible(false);
	m_pinned->setOpenExternalLinks(true);
	m_pinned->setStyleSheet(QStringLiteral(
		"background: #232629;"
		"border-bottom: 1px solid #2980b9;"
		"color: #eff0f1;"
		"padding: 3px;"
		));
	layout->addWidget(m_pinned, 0);

	m_view = new QTextBrowser(this);
	m_view->setOpenExternalLinks(true);

	layout->addWidget(m_view, 1);

	m_myline = new ChatLineEdit(this);
	layout->addWidget(m_myline);

	setLayout(layout);

	connect(m_myline, &ChatLineEdit::returnPressed, this, &ChatBox::sendMessage);

	m_view->document()->setDefaultStyleSheet(
		".sep { background: #4d4d4d }"
		".notification { background: #232629 }"
		".message, .notification {"
			"color: #eff0f1;"
			"margin: 6px 0 6px 0"
		"}"
		".shout { background: #34292c }"
		".shout .tab { background: #da4453 }"
		".action { font-style: italic }"
		".username { font-weight: bold }"
		".trusted { color: #27ae60 }"
		".registered { color: #16a085 }"
		".op { color: #f47750 }"
		".mod { color: #ed1515 }"
		".timestamp { color #4d4d4d }"
		"a:link { color: #1d99f3 }"
	);

	setPreserveMode(false);
}

void ChatBox::setPreserveMode(bool preservechat)
{
	QString placeholder, color;

	m_preserveChat = preservechat;

	if(preservechat) {
		placeholder = tr("Chat (recorded)...");
		color = "#da4453";
	} else {
		placeholder = tr("Chat...");
		color = "#4d4d4d";
	}

	// Set placeholder text and window style based on the mode
	m_myline->setPlaceholderText(placeholder);
	setStyleSheet(QStringLiteral(
		"QTextEdit, QLineEdit {"
			"background-color: #313438;"
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

void ChatBox::loggedIn(int myId)
{
	m_myId = myId;
	m_announcedUsers.clear();
}

void ChatBox::focusInput()
{
	m_myline->setFocus();
}

void ChatBox::clear()
{
	m_view->clear();
	m_lastAppendedId = 0;

	// Re-add avatars
	if(m_userlist) {
		for(int i=0;i<m_userlist->rowCount();++i) {
			const QModelIndex idx = m_userlist->index(i);
			m_view->document()->addResource(
				QTextDocument::ImageResource,
				QUrl(QStringLiteral("avatar://%1").arg(idx.data(canvas::UserListModel::IdRole).toInt())),
				idx.data(canvas::UserListModel::AvatarRole)
			);
		}
	}
}
void ChatBox::scrollToEnd()
{
	m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
}

static QString timestamp()
{
	return QStringLiteral("<span class=ts>%1</span>").arg(
		QDateTime::currentDateTime().toString("HH:mm")
	);
}

QString ChatBox::usernameSpan(int userId)
{
	const canvas::User user = m_userlist ? m_userlist->getUserById(userId) : canvas::User();

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

void ChatBox::appendSeparator()
{
	m_view->append(QStringLiteral(
		"<table height=1 width=\"100%\" class=sep><tr><td></td></tr></table>"
	));
}

void ChatBox::appendMessage(int userId, const QString &message, bool shout)
{
	const qint64 ts = QDateTime::currentMSecsSinceEpoch();

	if(shout) {
		m_lastAppendedId = -2;

	} else if(m_lastAppendedId != userId) {
		appendSeparator();
		m_lastAppendedId = userId;

	} else if(ts - m_lastMessageTs < 60000) {
		appendToLastMessage(message);
		return;
	}

	// We'll have to make do with a very limited subset of HTML and CSS:
	// http://doc.qt.io/qt-5/richtext-html-subset.html
	// Embedding a whole browser engine just to render the chat widget would
	// be excessive.
	m_view->append(QStringLiteral(
		"<table width=\"100%\" class=\"message%1\">"
		"<tr>"
			"<td width=3 rowspan=2 class=tab></td>"
			"<td width=50 rowspan=2><img src=\"avatar://%2\"></td>"
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
			usernameSpan(userId),
			timestamp(),
			htmlutils::newlineToBr(message)
		)
	);
	m_lastMessageTs = ts;
	scrollToEnd();
}

void ChatBox::appendToLastMessage(const QString &message)
{
	QTextCursor cursor(m_view->document());

	QTextBlock b = m_view->document()->lastBlock().previous();

	cursor.setPosition(b.position() + b.length() - 1);

	cursor.insertHtml(QStringLiteral("<br>"));
	cursor.insertHtml(message);
	scrollToEnd();
}

void ChatBox::appendAction(int userId, const QString &message)
{
	if(m_lastAppendedId != -1) {
		appendSeparator();
		m_lastAppendedId = -1;
	}

	m_view->append(QStringLiteral(
		"<table width=\"100%\" class=message>"
		"<tr>"
			"<td><span class=action>%1 %2</span></td>"
			"<td class=timestamp align=right>%3</td>"
		"</tr>"
		"</table>"
		).arg(
			usernameSpan(userId),
			message,
			timestamp()
		)
	);
	scrollToEnd();
}

void ChatBox::appendNotification(const QString &message)
{
	if(m_lastAppendedId != 0) {
		appendSeparator();
		m_lastAppendedId = 0;
	}

	m_view->append(QStringLiteral(
		"<table width=\"100%\" class=notification><tr>"
			"<td>%1</td>"
			"<td align=right class=timestamp>%2</td>"
		"</tr></table>"
		).arg(
			htmlutils::newlineToBr(message),
			timestamp()
		)
	);
	scrollToEnd();
}

void ChatBox::userJoined(int id, const QString &name)
{
	Q_UNUSED(name);

	if(m_userlist) {
		m_view->document()->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(id)),
			m_userlist->getUserById(id).avatar
		);
	} else {
		qWarning("User #%d logged in, but userlist object not assigned to ChatWidget!", id);
	}

	// The server resends UserJoin messages during session reset.
	// We don't need to see the join messages again.
	if(m_announcedUsers.contains(id))
		return;

	m_announcedUsers << id;
	appendNotification(tr("%1 joined the session").arg(usernameSpan(id)));
	notification::playSound(notification::Event::LOGIN);
}

void ChatBox::userParted(int id)
{
	appendNotification(tr("%1 left the session").arg(usernameSpan(id)));
	notification::playSound(notification::Event::LOGOUT);
	m_announcedUsers.removeAll(id);
}

void ChatBox::kicked(const QString &kickedBy)
{
	appendNotification(tr("You have been kicked by %1").arg(kickedBy.toHtmlEscaped()));
}

void ChatBox::receiveMessage(const protocol::MessagePtr &msg)
{
	if(msg->type() != protocol::MSG_CHAT) {
		qWarning("ChatBox::receiveMessage: message type (%d) is not MSG_CHAT!", msg->type());
		return;
	}

	const protocol::Chat &chat = msg.cast<protocol::Chat>();
	const QString safetext = chat.message().toHtmlEscaped();

	if(chat.isPin()) {
		if(safetext == "-") {
			// note: the protocol doesn't allow empty chat messages,
			// which is why we have to use a special value like this
			// to clear the pinning.
			m_pinned->setVisible(false);
			m_pinned->setText(QString());
		} else {
			m_pinned->setText(htmlutils::linkify(safetext, QStringLiteral("style=\"color:#3daae9\"")));
			m_pinned->setVisible(true);
		}

	} else if(chat.isAction()) {
		appendAction(msg->contextId(), htmlutils::linkify(safetext));

	} else {
		appendMessage(msg->contextId(), htmlutils::linkify(safetext), chat.isShout());
	}

	if(!m_myline->hasFocus())
		notification::playSound(notification::Event::CHAT);

}

void ChatBox::receiveMarker(int id, const QString &message)
{
	appendNotification(QStringLiteral(
		"<img src=\"theme:flag-red.svg\"> %1: %2"
		).arg(
			usernameSpan(id),
			htmlutils::linkify(message.toHtmlEscaped())
		)
	);
}

void ChatBox::systemMessage(const QString& message, bool alert)
{
	Q_UNUSED(alert);
	appendNotification(message.toHtmlEscaped());
}

void ChatBox::sendMessage(const QString &msg)
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

		} else if(cmd.at(0)=='!') {
			emit message(protocol::Chat::announce(m_myId, msg.mid(2)));
			return;

		} else if(cmd == "me") {
			if(!params.isEmpty())
				emit message(protocol::Chat::action(m_myId, params, !m_preserveChat));
			return;

		} else if(cmd == "pin") {
			if(!params.isEmpty())
				emit message(protocol::Chat::pin(m_myId, params));
			return;

		} else if(cmd == "unpin") {
			emit message(protocol::Chat::pin(m_myId, QStringLiteral("-")));
			return;

		} else if(cmd == "roll") {
			utils::DiceRoll result = utils::diceRoll(params.isEmpty() ? QStringLiteral("1d6") : params);
			if(result.number>0)
				emit message(protocol::Chat::action(m_myId, "rolls " + result.toString(), !m_preserveChat));
			else
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
	emit message(protocol::Chat::regular(m_myId, msg, !m_preserveChat));
}

void ChatBox::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if(event->size().height() == 0) {
		if(!m_wasCollapsed)
			emit expanded(false);
		m_wasCollapsed = true;
	} else if(m_wasCollapsed) {
		m_wasCollapsed = false;
		emit expanded(true);
	}
}

}

