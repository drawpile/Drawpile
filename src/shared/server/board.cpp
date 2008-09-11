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

#include <QStringList>

#include "../net/message.h"
#include "board.h"

namespace server {

Board::Board(QObject *parent) : QObject(parent), _exists(false),
	_title(""), _width(0), _height(0),_owner(0),rasterlen_(0), valid_(false),
	clientsWaitingForBuffer_(0), bufferLimit_(1024 * 500)
{
}

void Board::set(int owner, const QString& title, int w, int h)
{
	_owner = owner;
	_exists = true;
	_title = title;
	_width = w;
	_height = h;
}

bool Board::set(int owner, const QStringList& tokens)
{
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
	_owner = owner;
	valid_ = false;
	return true;
}

/**
 * Mark the board as inexistent
 */
void Board::clear() {
	_exists = false;
}

/**
 * Reset the buffer for new raster data. Resetting
 * the buffer will invalidate it.
 */
void Board::clearBuffer()
{
	raster_.clear();
	drawing_.clear();
	valid_ = false;
	rasterlen_ = 0;
}

/**
 * Telling there will be raster data validates the buffer. (The buffer
 * must have been cleared previously)
 * This is because when we know how much data there will be, a client
 * somewhere has already set aside a copy of the synced board for us.
 */
void Board::setExpectedBufferLength(int len)
{
	rasterlen_ = len;
	valid_ = true;
}

/**
 * Add a new piece to the raster buffer.
 * Emits a signal to notify clients who may be waiting for the next piece.
 */
void Board::addRaster(const QByteArray& chunk)
{
	raster_.append(chunk);
	Q_ASSERT(raster_.size() <= rasterlen_);
	emit rasterAvailable();
}

/**
 * Drawing commands are accepted in the buffer only when the buffer is valid.
 * Once the buffer fills to the limit, it gets cleared
 * and invalidated, necessitating a raster data download. Note that
 * the buffer is not cleared as long as even one client hasn't got it
 * all yet. Clients express their interest in the buffer by connecting
 * to the rasterAvailable() signal.
 */
void Board::addDrawingCommand(const QByteArray& packet)
{
	if(valid_) {
		if(clientsWaitingForBuffer_==0 && raster_.length() > bufferLimit_)
			clearBuffer();
		else
			drawing_.append(packet);
	}
}

QString Board::toMessage() const
{
	QStringList tkns;
	if(_exists) 
		tkns << "BOARD" << QString::number(_owner) << _title <<
			QString::number(_width) << QString::number(_height);
	else
		tkns << "NOBOARD";
	return protocol::Message::quote(tkns);
}

void Board::connectNotify(const char *signal) {
	++clientsWaitingForBuffer_;
}

void Board::disconnectNotify(const char *signal) {
	--clientsWaitingForBuffer_;
}

}

