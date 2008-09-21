/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include "core/point.h"

namespace network {
	class SessionState;
}

namespace protocol {
	class Annotation;
}

namespace interface {
	class BrushSource;
	class ColorSource;
}

namespace dpcore {
	class Brush;
}

//! Drawing board related classes
namespace drawingboard {

class BoardItem;
class AnnotationItem;
class User;
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

		//! Check if there are any annotations
		bool hasAnnotations() const;

		//! Delete all users
		void clearUsers();

		//! Set local user
		void setLocalUser(int id);

		//! Get a board editor
		BoardEditor *getEditor(network::SessionState *session=0);

		//! Add a preview stroke
		void addPreview(const dpcore::Point& point);

		//! End a preview stroke
		void endPreview();

		//! Scrap preview strokes
		void flushPreviews();

		//! Show/hide annotations
		void showAnnotations(bool show);

	public slots:
		//! Add a new user to the board
		void addUser(int id);

		//! User switches tools
		void userSetTool(int user, const dpcore::Brush& brush);

		//! User stroke information
		void userStroke(int user, const dpcore::Point& point);

		//! User ends a stroke
		void userEndStroke(int user);

		//! Add or change an annotation
		void annotate(const protocol::Annotation& annotation);

		//! Remove an annotation
		void unannotate(int id);

	signals:
		//! The local user just created a new annotation
		void newLocalAnnotation(drawingboard::AnnotationItem *item);

		//! An annotation is about to be deleted
		void annotationDeleted(drawingboard::AnnotationItem *item);

	private:
		
		//! Commit preview strokes to the board
		void commitPreviews();

		//! The board contents
		BoardItem *image_;

		//! List of board users
		QHash<int,User*> users_;

		//! ID of the local user
		int localuser_;

		//! Preview strokes currently on screen
		QQueue<Preview*> previews_;

		//! Cache of reusable preview strokes
		QQueue<Preview*> previewcache_;

		//! Has a preview been started?
		bool previewstarted_;

		//! Coordinate of the last preview stroke
		dpcore::Point lastpreview_;

		//! Preview stroke for use with tool previews (eg. line)
		Preview *toolpreview_;

		//! Brush source (UI)
		interface::BrushSource *brushsrc_;

		//! Color source (UI)
		interface::ColorSource *colorsrc_;
};

}

#endif

