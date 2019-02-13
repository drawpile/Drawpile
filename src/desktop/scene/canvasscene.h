/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CANVAS_SCENE_H
#define CANVAS_SCENE_H

#include <QGraphicsScene>

#include "../shared/net/message.h"

namespace paintcore {
	class LayerStack;
	class Brush;
}

namespace net {
	class Client;
}

class QGraphicsItem;

namespace canvas {
	class CanvasModel;
	class Selection;
}

//! Drawing board related classes
namespace drawingboard {

class CanvasItem;
class AnnotationState;
class AnnotationItem;
class SelectionItem;
class UserMarkerItem;
class LaserTrailItem;

/**
 * @brief The drawing board
 * The drawing board contains the picture and provides methods
 * to modify it.
 */
class CanvasScene : public QGraphicsScene
{
	Q_OBJECT

public:
	//! Margin around the image to make working near corners easier
	static const int MARGIN = 900;

	CanvasScene(QObject *parent);
	~CanvasScene();

	//! Clear and initialize the canvas
	void initCanvas(canvas::CanvasModel *model);

	//! Is there an image on the drawing board
	bool hasImage() const { return m_model != nullptr; }

	//! Are annotation borders shown?
	bool showAnnotationBorders() const { return _showAnnotationBorders; }

	//! Show/hide annotations
	void showAnnotations(bool show);

	canvas::CanvasModel *model() const { return m_model; }

public slots:
	//! Show annotation borders
	void showAnnotationBorders(bool hl);

	//! Show/hide remote cursor markers
	void showUserMarkers(bool show);

	//! Show user names in cursor markers
	void showUserNames(bool show);

	//! Show layer selection in cursor marker
	void showUserLayers(bool show);

	//! Show avatars in cursor marker
	void showUserAvatars(bool show);

	//! Show hide laser pointer trails
	void showLaserTrails(bool show);

	void activeAnnotationChanged(int id);

	//! Reveal the canvas item
	void showCanvas();

	//! Hide canvas item
	void hideCanvas();

signals:
	//! Canvas size has just changed
	void canvasResized(int xoffset, int yoffset, const QSize &oldSize);

private slots:
	void onSelectionChanged(canvas::Selection *sel);
	void handleCanvasResize(int xoffset, int yoffset, const QSize &oldsize);
	void advanceUsermarkerAnimation();

	void userCursorAdded(const QModelIndex&, int first, int last);
	void userCursorRemoved(const QModelIndex&, int first, int last);
	void userCursorChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed);

	void annotationsAdded(const QModelIndex&, int first, int last);
	void annotationsRemoved(const QModelIndex&, int first, int last);
	void annotationsChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed);
	void annotationsReset();

	void laserAdded(const QModelIndex&, int first, int last);
	void laserRemoved(const QModelIndex&, int first, int last);
	void laserChanged(const QModelIndex &first, const QModelIndex &last, const QVector<int> &changed);

private:
	AnnotationItem *getAnnotationItem(int id);

	//! The board contents
	CanvasItem *m_image;

	canvas::CanvasModel *m_model;

	//! Laser pointer trails
	QHash<int, LaserTrailItem*> m_lasertrails;

	//! Current selection
	SelectionItem *m_selection;

	//! User markers display remote user cursor positions
	QHash<int, UserMarkerItem*> m_usermarkers;

	QTimer *_animTickTimer;

	bool _showAnnotationBorders;
	bool _showAnnotations;
	bool m_showUserMarkers;
	bool m_showUserNames;
	bool m_showUserLayers;
	bool m_showUserAvatars;
	bool m_showLaserTrails;
};

}

#endif

