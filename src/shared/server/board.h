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

#include <QObject>

#ifndef DP_SRV_BOARD_H
#define DP_SRV_BOARD_H

namespace server {

/**
 * Information about the drawing board.
 */
class Board : public QObject {
	Q_OBJECT
	public:
		Board(QObject *parent=0);

		~Board() { }

		//! Has the board been created yet?
		bool exists() const { return _exists; }

		//! Mark the board as inexistent
		void clear();

		//! Set board options
		void set(int owner, const QString& title, int w, int h, bool lock);

		//! Set board options from a message
		bool set(int owner, const QStringList& tokens);

		//! Lock/unlock the board
		void setLock(bool lock) { _lock = lock; }

		//! Is the board locked?
		bool locked() const { return _lock; }

		//! Set the size limit in bytes on drawing command buffer.
		/**
		 * When this limit is exceeded, the buffer is cleared and the next
		 * login will trigger a board sync.
		 */
		void setBufferLimit(int limit) { bufferLimit_ = limit; }

		//! Get the ID of the board owner
		int owner() const { return _owner; }

		//! Width of the board in pixels
		int width() const { return _width; }

		//! Height of the board in pixels
		int height() const { return _height; }

		//! Set the maximum number of users for this board
		void setMaxUsers(int maxusers) { _maxusers = maxusers; }

		//! Get the maximum number of users for this board
		int maxUsers() const { return _maxusers; }

		//! Lock new users by default?
		bool defaultLock() const { return _deflock; }

		//! The title of the board
		const QString& title() const { return _title; }

		//! Clear buffered raster data and drawing commands
		void clearBuffer();

		//! Set the length of the expected buffer
		void setExpectedBufferLength(int len);

		//! Add a new chunk of raster data
		void addRaster(const QByteArray& chunk);

		//! Add a preserialized drawing command
		void addDrawingCommand(const QByteArray& packet);

		//! Get the expected length of the raster buffer
		int rasterBufferLength() const { return rasterlen_; }

		//! Get the raster buffer (may be unfinished)
		const QByteArray& rasterBuffer() const { return raster_; }

		//! Get the buffered drawing commands
		const QByteArray& drawingBuffer() const { return drawing_; }

		//! Is the buffer valid?
		/**
		 * The buffer is valid only when there is raster data, or
		 * at least a promise of raster data.
		 */
		bool validBuffer() const { return valid_; }

		/**
		 * Get a board info message representing this board.
		 */
		QString toMessage() const;
	signals:
		//! More raster data has become available
		void rasterAvailable();

	protected:
		void connectNotify(const char *signal);
		void disconnectNotify(const char *signal);

	private:
		bool _exists;
		QString _title;
		int _width;
		int _height;
		int _owner;
		bool _lock;
		int _maxusers;
		bool _deflock;

		QByteArray raster_;
		QByteArray drawing_;
		int rasterlen_;
		bool valid_;
		int clientsWaitingForBuffer_;
		int bufferLimit_;
};

}

#endif

