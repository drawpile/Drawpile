/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#ifndef BOARD_H
#define BOARD_H

#include <QGraphicsScene>
#include <QHash>
#include <QQueue>

#include "point.h"

namespace network {
	class SessionState;
}

namespace interface {
	class BrushSource;
	class ColorSource;
}

//! Drawing board related classes
namespace drawingboard {

class Layer;
class User;
class Brush;
class BoardEditor;
class Preview;

//! The drawing board
/**
 * The drawing board contains the picture and provides methods
 * to modify it. The board has a list of users and remembers each of their
 * states, so drawing commands can be interleaved.
 */
class Board : public QGraphicsScene
{
	Q_OBJECT
	friend class BoardEditor;

	public:
		Board(QObject *parent, interface::BrushSource *brush, interface::ColorSource *color);
		~Board();

		//! Initialize to a solid color
		void initBoard(const QSize& size, const QColor& background);

		//! Initialize the board using an existing image as base
		void initBoard(QImage image);

		//! Get board width
		int width() const;

		//! Get board height
		int height() const;

		//! Get board contents as an image
		QImage image() const;

		//! Is there an image on the drawing board
		bool hasImage() const;

		//! Delete all users
		void clearUsers();

		//! Set local user
		void setLocalUser(int id);

		//! Get a board editor
		BoardEditor *getEditor(network::SessionState *session=0);

		//! Add a preview stroke
		void addPreview(const Point& point);

		//! End a preview stroke
		void endPreview();

		//! Scrap preview strokes
		void flushPreviews();

	public slots:
		//! Add a new user to the board
		void addUser(int id);

		//! Remove a user from the board
		void removeUser(int id);

		//! User switches tools
		void userSetTool(int user, const drawingboard::Brush& brush);

		//! User stroke information
		void userStroke(int user, const drawingboard::Point& point);

		//! User ends a stroke
		void userEndStroke(int user);

	private:
		//! Commit preview strokes to the board
		void commitPreviews();

		Layer *image_;
		QHash<int,User*> users_;
		int localuser_;

		QQueue<Preview*> previews_;
		QQueue<Preview*> previewcache_;
		bool previewstarted_;
		Point lastpreview_;
		Preview *toolpreview_;

		interface::BrushSource *brushsrc_;
		interface::ColorSource *colorsrc_;
};

}

#endif

