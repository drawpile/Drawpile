/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2017 Calle Laakkonen

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

#include <QDebug>
#include <QResizeEvent>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QUrl>
#include <QLabel>
#include <QSettings>

namespace widgets {

ChatBox::ChatBox(QWidget *parent)
	:  QWidget(parent),
	  m_wasCollapsed(false),
	  m_operator(false),
	  m_preserveChat(false)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setMargin(0);

	m_pinned = new QLabel(this);
	m_pinned->setVisible(false);
	m_pinned->setOpenExternalLinks(true);
	m_pinned->setStyleSheet(QStringLiteral(
		"background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #8d8d8d, stop:1 #31363b);"
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
		"p { margin: 5px 0; }"
		".marker { color: #da4453 }"
		".sysmsg { color: #fdbc4b }"
		".announcement { color: #fcfcfc }"
		".nick { font-weight: bold }"
		".nick.me { color: #fcfcfc }"
		".log { color: #aaaaaa; font-style: italic }"
		".action { color: #fcfcfc }"
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
		color = "#1d99f3";
	}

	// Set placeholder text and window style based on the mode
	m_myline->setPlaceholderText(placeholder);
	setStyleSheet(QStringLiteral(
		"QTextEdit, QLineEdit {"
			"border: none;"
			"background-color: #232629;"
			"color: #bdc3c7;"
			"font-family: Monospace"
		"}"
		"QLineEdit {"
			"border-top: 2px solid %1"
		"}"
		).arg(color)
	);
}

void ChatBox::focusInput()
{
	m_myline->setFocus();
}

void ChatBox::clear()
{
	m_view->clear();
}

void ChatBox::userJoined(int id, const QString &name)
{
	Q_UNUSED(id);
	systemMessage(tr("<b>%1</b> joined the session").arg(name.toHtmlEscaped()));
	notification::playSound(notification::Event::LOGIN);
}

void ChatBox::userParted(const QString &name)
{
	systemMessage(tr("<b>%1</b> left the session").arg(name.toHtmlEscaped()));
	notification::playSound(notification::Event::LOGOUT);
}

void ChatBox::kicked(const QString &kickedBy)
{
	systemMessage(tr("You have been kicked by %1").arg(kickedBy.toHtmlEscaped()));
}

/**
 * The received message is displayed in the chat box.
 * @param nick nickname of the user who said something
 * @param message what was said
 * @param announcement is this a public announcement?
 * @param isme if true, the message was sent by this user
 */
void ChatBox::receiveMessage(const QString& nick, const QString& message, bool announcement, bool action, bool isme, bool islog)
{
	if(islog) {
		const int loglevel = QSettings().value("settings/serverlog", 1).toInt();
		if(loglevel==0)
			return;
		else if(loglevel==1 && !m_operator)
			return;
	}
	if(action) {
		m_view->append(
			"<p class=\"chat action\"> * " + nick.toHtmlEscaped() +
			" " +
			htmlutils::linkify(message.toHtmlEscaped()) +
			"</p>"
		);
	} else {
		m_view->append(
			"<p class=\"chat" + QString(islog ? " log" : "") + "\"><span class=\"nick" + QString(isme ? " me" : "") + "\">&lt;" +
			nick.toHtmlEscaped() +
			"&gt;</span> <span class=\"msg" + QString(announcement ? " announcement" : "") + "\">" +
			htmlutils::linkify(message.toHtmlEscaped()) +
			"</span></p>"
		);
	}
	if(!m_myline->hasFocus())
		notification::playSound(notification::Event::CHAT);
}

void ChatBox::setPinnedMessage(const QString &message)
{
	if(message == "-") {
		// note: the protocol doesn't allow empty chat messages,
		// which is why we have to use a special value like this
		// to clear the pinning.
		m_pinned->setVisible(false);
		m_pinned->setText(QString());
	} else {
		m_pinned->setText(htmlutils::linkify(message.toHtmlEscaped(), QStringLiteral("style=\"color:#3daae9\"")));
		m_pinned->setVisible(true);
	}
}

void ChatBox::receiveMarker(const QString &nick, const QString &message)
{
	m_view->append(
		"<p class=\"marker\"><span class=\"nick\">&lt;" +
		nick.toHtmlEscaped() +
		"&gt;</span> <span class=\"msg\">" +
		htmlutils::linkify(message.toHtmlEscaped()) +
		"</span></p>"
	);
}

/**
 * @param message the message
 */
void ChatBox::systemMessage(const QString& message, bool alert)
{
	Q_UNUSED(alert);
	m_view->append("<p class=\"sysmsg\"> *** " + message + " ***</p>");
}

void ChatBox::sendMessage(const QString &msg)
{
	// TODO implement this in JavaScript?
	// It would make it easy to add custom commands.
	if(msg.at(0) == '/') {
		// Special commands

		int split = msg.indexOf(' ');
		if(split<0)
			split = msg.length();

		QString cmd = msg.mid(1, split-1).toLower();
		QString params = msg.mid(split).trimmed();

		if(cmd == "clear") {
			// client side command: clear chat window
			clear();

		} else if(cmd.at(0)=='!') {
			// public announcement
			emit message(msg.mid(2), m_preserveChat, true, false);

		} else if(cmd == "me") {
			if(!params.isEmpty())
				emit message(msg.mid(msg.indexOf(' ')+1), m_preserveChat, false, true);

		} else if(cmd == "pin") {
			if(!params.isEmpty())
				emit pinMessage(msg.mid(msg.indexOf(' ')+1));

		} else if(cmd == "unpin") {
			emit pinMessage(QStringLiteral("-"));

		} else if(cmd == "roll") {
			if(params.isEmpty())
				params = "1d6";

			utils::DiceRoll result = utils::diceRoll(params);
			if(result.number>0)
				emit message("rolls " + result.toString(), m_preserveChat, false, true);
			else
				systemMessage(tr("Invalid dice roll description: %1").arg(params));

#ifndef NDEBUG
		} else if(cmd == "rolltest") {
			if(params.isEmpty())
				params = "1d6";

			QList<float> d = utils::diceRollDistribution(params);
			QString msg(params + "\n");
			for(int i=0;i<d.size();++i)
				msg += QStringLiteral("%1: %2\n").arg(i+1).arg(d.at(i) * 100);
			systemMessage(msg);
#endif
		}

	} else {
		// A normal chat message
		emit message(msg, m_preserveChat, false, false);
	}
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

