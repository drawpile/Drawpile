#include <QStringList>

#include "../net/message.h"
#include "board.h"

namespace server {

Board::Board(int owner) : _exists(false), _title(""), _width(0), _height(0),_owner(owner) {
}

void Board::set(const QString& title, int w, int h) {
	_exists = true;
	_title = title;
	_width = w;
	_height = h;
}

bool Board::set(const QStringList& tokens) {
	Q_ASSERT(tokens[0].compare("BOARD")==0);
	if(tokens.size()!=5)
		return false;
	bool ok;
	int w, h;
	w = tokens[3].toInt(&ok);
	if(!ok) return false;
	h = tokens[4].toInt(&ok);
	if(!ok) return false;
	_exists = true;
	_title = tokens[2];
	_width = w;
	_height = h;
	return true;
}

QString Board::toMessage() const {
	QStringList tkns;
	if(_exists) 
		tkns << "BOARD" << QString::number(_owner) << _title <<
			QString::number(_width) << QString::number(_height);
	else
		tkns << "NOBOARD";
	return protocol::Message::quote(tkns);
}

}

