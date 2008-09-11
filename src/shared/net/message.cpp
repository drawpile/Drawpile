/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#include <QIODevice>
#include <QRegExp>

#include "message.h"

namespace protocol {

Message *Message::deserialize(QIODevice& data, int len) {
	return new Message(QString::fromUtf8(data.read(len)));
}

unsigned int Message::payloadLength() const {
	return _message.toUtf8().length();
}

void Message::serializeBody(QIODevice& data) const {
	data.write(_message.toUtf8());
}

QStringList Message::tokens() const {
	QRegExp re("(\"(?:[^\"]|\\\\\")+\"|\\S+)(?:\\s+|$)");
	QStringList tkn;
	int pos=0;
	while(pos<_message.length()) {
		pos = re.indexIn(_message, pos);
		if(pos==-1)
			break;
		QString text = re.capturedTexts()[1];
		// Unescape backslash and quotes
		text.replace("\\\\", "\\");
		text.replace("\\\"", "\"");
		if(text[0]=='"')
			tkn.append(text.mid(1, text.length()-2));
		else
			tkn.append(text);
		pos += re.matchedLength();
	}
	return tkn;
}

QString Message::quote(const QStringList& tokens) {
	const QRegExp space("\\s");
	QString msg;
	QListIterator<QString> i(tokens);
	while(i.hasNext()) {
		QString tok = i.next();
		bool needquotes = tok.indexOf(space)!=-1 || tok.length()==0;
		tok.replace('\\', "\\");
		tok.replace('"', "\\\"");
		if(needquotes) {
			msg.append('\"');
			msg.append(tok);
			msg.append('\"');
		} else {
			msg.append(tok);
		}
		if(i.hasNext())
			msg.append(' ');
	}
	return msg;
}

}

