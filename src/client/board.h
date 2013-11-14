/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2009 Calle Laakkonen

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

namespace interface {
	class BrushSource;
	class ColorSource;
}

namespace dpcore {
	class Brush;
	class LayerStack;
}

namespace widgets {
	class LayerList;
}

//! Drawing board related classes
namespace drawingboard {

class BoardItem;
class AnnotationItem;
class User;
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

	public:
		Board(QObject *parent, interface::BrushSource *brush, interface::ColorSource *color);
		~Board();

		//! Initialize to a solid color
		bool initBoard(const QSize& size, const QColor& background);

		//! Initialize the board using an existing image as base
		bool initBoard(QImage image);

		//! Initialize the board from a file
		bool initBoard(const QString& file);

		//! Set the layer list widget to update when local user's layer changes
		void setLayerList(widgets::LayerList *llist);

		//! Get board width
		int width() const;

		//! Get board height
		int height() const;

		//! Get the layers
		dpcore::LayerStack *layers();

		//! Get board contents as an image
		QImage image() const;

		//! Save the board
		bool save(const QString& filename) const;

		//! Check if we are using features that require saving in OpenRaster format to preserve
		bool needSaveOra() const;

		//! Is there an image on the drawing board
		bool hasImage() const;

		//! Check if there are any annotations
		bool hasAnnotations() const;

		//! Get all annotations as message strings
		// TODO
		QStringList getAnnotations(bool zeroid) const;

		//! Remove all annotations
		void clearAnnotations();

		//! Delete all users
		void clearUsers();

		//! Set local user
		void setLocalUser(int id);

		//! Add a preview stroke
		void addPreview(const dpcore::Point& point);

		//! End a preview stroke
		void endPreview();

		//! Scrap preview strokes
		void flushPreviews();

		//! Show/hide annotations
		void showAnnotations(bool show);

	public slots:
		//! Highlight all annotations
		void highlightAnnotations(bool hl);

		//! Add a new user to the board
		void addUser(int id);

		//! User switches tools
		void userSetTool(int user, const dpcore::Brush& brush);

		//! User stroke information
		void userStroke(int user, const dpcore::Point& point);

		//! User ends a stroke
		void userEndStroke(int user);

		//! Add or change an annotation
#if 0
		void annotate(const protocol::Annotation& annotation);

		//! Remove an annotation
		void unannotate(int id);
#endif

		//! Create a new layer
		void addLayer(const QString& name);

		//! Delete a layer
		void deleteLayer(int id, bool mergedown);

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

		bool hla_;

		widgets::LayerList *layerwidget_;
};

}

#endif

