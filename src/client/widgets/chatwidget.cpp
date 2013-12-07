/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QDebug>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QUrl>

#include "chatlineedit.h"
#include "chatwidget.h"

namespace {
QString esc(QString str)
{
	return str.replace('<', "&lt;").replace('>', "&gt;");
}
}

namespace widgets {

ChatBox::ChatBox(QWidget *parent)
	:  QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setMargin(0);

	_view = new QTextBrowser(this);
	_view->setOpenExternalLinks(true);

	layout->addWidget(_view, 1);

	_myline = new ChatLineEdit(this);
	_myline->setPlaceholderText(tr("Chat..."));
	layout->addWidget(_myline);

	setLayout(layout);

	connect(_myline, SIGNAL(returnPressed(QString)), this, SIGNAL(message(QString)));

	// Chat window styling
	setStyleSheet(
		"QTextEdit, QLineEdit {"
			"border: none;"
			"background-color: #111;"
			"color: #adadad;"
			"font-family: Monospace"
		"}"
		"QLineEdit {"
			"border-top: 2px solid #3333da"
		"}"
	);
}

ChatBox::~ChatBox()
{
	//delete ui_;
}

void ChatBox::clear()
{
	_view->clear();
}

void ChatBox::userJoined(const QString &name)
{
	systemMessage(tr("<b>%1</b> joined the session").arg(esc(name)));
}

void ChatBox::userParted(const QString &name)
{
	systemMessage(tr("<b>%1</b> left the session").arg(esc(name)));
}

/**
 * The received message is displayed in the chat box.
 * @param nick nickname of the user who said something
 * @param message what was said
 * @param isme if true, the message was sent by this user
 */
void ChatBox::receiveMessage(const QString& nick, const QString& message, bool isme)
{
	QString extrastyle;
	if(isme)
		extrastyle = "color: white";

	// TODO turn URLs into links
	_view->append("<p style=\"margin: 5px 0\"><b style=\""+ extrastyle + "\">&lt;" + esc(nick) + "&gt;</b> " + esc(message) + "</p>");
}

/**
 * @param message the message
 */
void ChatBox::systemMessage(const QString& message)
{
	_view->append("<p style=\"margin: 5px 0;color: yellow\"> *** " + message + " ***</p>");
}

}

