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
#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H

#include <QGraphicsScene>
#include <QHash>
#include <QQueue>

#include "core/point.h"

namespace dpcore {
	class LayerStack;
}

namespace protocol {
	class Message;
}

namespace widgets {
	class LayerListWidget;
}

//! Drawing board related classes
namespace drawingboard {

class StateTracker;
class CanvasItem;
class AnnotationItem;
class Preview;

//! The drawing board
/**
 * The drawing board contains the picture and provides methods
 * to modify it. The board has a list of users and remembers each of their
 * states, so drawing commands can be interleaved.
 */
class CanvasScene : public QGraphicsScene
{
	Q_OBJECT

	public:
		CanvasScene(QObject *parent, widgets::LayerListWidget *layerlistwidget);
		~CanvasScene();

		//! Clear and initialize the canvas
		void initCanvas();

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

		void handleDrawingCommand(protocol::Message *cmd);

	signals:
		//! The local user just created a new annotation
		void newLocalAnnotation(drawingboard::AnnotationItem *item);

		//! An annotation is about to be deleted
		void annotationDeleted(drawingboard::AnnotationItem *item);

	private:
		
		//! Commit preview strokes to the board
		void commitPreviews();

		//! The board contents
		CanvasItem *_image;

		//! Drawing context state tracker
		StateTracker *_statetracker;

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

		bool hla_;
		
		//! The layer list widget to update on layer changes
		widgets::LayerListWidget *_layerlistwidget;
};

}

#endif

