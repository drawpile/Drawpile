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
		bool needquotes = tok.indexOf(space)!=-1;
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

