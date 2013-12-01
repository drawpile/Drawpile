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
#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include <QGraphicsView>

#include "core/point.h"
#include "tools.h"

namespace drawingboard {
	class CanvasScene;
}

namespace net {
	class Client;
}

namespace widgets {

class ToolSettingsDock;

/**
 * @brief Editor view
 * The editor view is a customized QGraphicsView that displays
 * the drawing board and handes user input.
 * It also provides other features, such as brush outline preview.
 */
class CanvasView : public QGraphicsView
{
	Q_OBJECT
	public:
		CanvasView(QWidget *parent=0);

		//! Set the board to use
		void setCanvas(drawingboard::CanvasScene *scene);

		//! Set the network client
		void setClient(net::Client *client);

		//! Set the tool settings panel
		void setToolSettings(widgets::ToolSettingsDock *settings);
		
		//! Get the current zoom factor
		int zoom() const { return _zoom; }

		//! Set the zoom factor
		void setZoom(int zoom);

		//! Get the current rotation angle in degrees
		qreal rotation() const { return _rotate; }

		//! Set the rotation angle in degrees
		void setRotation(qreal angle);
		
	signals:
		//! An image has been dropped on the widget
		void imageDropped(const QString& filename);

		//! Signals scene rectangle changes
		void viewMovedTo(const QRectF& rect);

		//! The view has been transformed
		void viewTransformed(int zoom, qreal angle);

	public slots:
		//! Select the active tool
		void selectTool(tools::Type tool);
		
		//! Select the currently active layer
		void selectLayer(int layer_id);
		
		//! Set the radius of the brush preview outline
		void setOutlineRadius(int radius);

		//! Enable or disable cursor outline
		void setOutline(bool enable);

		//! Scroll view to location
		void scrollTo(const QPoint& point);
		
		//! Proxy for announcing scene changes
		void sceneChanged();
		
		void setLocked(bool lock);

	protected:
		void enterEvent(QEvent *event);
		void leaveEvent(QEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void mouseDoubleClickEvent(QMouseEvent*);
		void wheelEvent(QWheelEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void keyReleaseEvent(QKeyEvent *event);
		bool viewportEvent(QEvent *event);
		void drawForeground(QPainter *painter, const QRectF& rect);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

	private:
		//! View transformation mode (for dragging)
		enum ViewTransform {NOTRANSFORM, TRANSLATE, ROTATE};

		//! Start dragging the view
		void startDrag(int x, int y, ViewTransform mode);

		//! Stop dragging the view
		void stopDrag();

		//! Drag the view
		void moveDrag(int x, int y);

		//! Redraw the scene around the outline cursor if necesasry
		void updateOutline(const dpcore::Point& point);

		void onPenDown(const dpcore::Point &p);
		void onPenMove(const dpcore::Point &p);
		void onPenUp();
		
		void resetCursor();

		/**
		 * @brief State of the pen
		 *
		 * - NOTDOWN pen is not down
		 * - MOUSEDOWN mouse is down
		 * - TABLETDOWN tablet stylus is down
		 */
		enum {NOTDOWN, MOUSEDOWN, TABLETDOWN} _pendown;

		//! Is the view being dragged
		ViewTransform _isdragging;
		ViewTransform _dragbtndown;
		int _dragx,_dragy;

		//! Previous pointer location
		dpcore::Point _prevpoint;

		int _outlinesize, _dia;
		bool _enableoutline, _showoutline;
		QCursor _cursor;

		//! View zoom in percents
		int _zoom;
		//! View rotation in degrees
		qreal _rotate;

		drawingboard::CanvasScene *_scene;
		
		tools::ToolCollection _toolbox;
		tools::Tool *_current_tool;

		bool _locked;
};

}

#endif

