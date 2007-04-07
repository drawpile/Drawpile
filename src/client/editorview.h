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
#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include <QGraphicsView>

namespace drawingboard {
	class Board;
	class Point;
}

namespace widgets {
//! Editor view
/**
 * The editor view is a customized QGraphicsView that displays
 * the drawing board and handes user input.
 * It also provides other features, such as brush outline preview.
 */
class EditorView : public QGraphicsView
{
	Q_OBJECT
	public:
		EditorView(QWidget *parent=0);

		void setBoard(drawingboard::Board *board);

	signals:
		//! This signal is emitted when a mouse button is pressed or the pen touches the tablet
		/**
		 * @param point coordinates
		 */
		void penDown(const drawingboard::Point& point);
		//! This signal is emitted when the pen or mouse pointer is moved while drawing
		void penMove(const drawingboard::Point& point);

		//! This signal is emitted when the pen is lifted or the mouse button released.
		void penUp();

		//! An image has been dropped on the widget
		void imageDropped(const QString& filename);

	public slots:
		//! Set the radius of the brush preview outline
		void setOutlineRadius(int radius);

		//! Enable or disable cursor outline
		void setOutline(bool enable);

		//! Set brush outline colors
		void setOutlineColors(const QColor& fg, const QColor& bg);

		//! Enable or disable crosshair cursor
		void setCrosshair(bool enable);

	protected:
		void enterEvent(QEvent *event);
		void leaveEvent(QEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void mouseDoubleClickEvent(QMouseEvent*);
		bool viewportEvent(QEvent *event);
		void drawForeground(QPainter *painter, const QRectF& rect);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

	private:
		void startDrag(int x, int y);
		void moveDrag(int x, int y);
		void stopDrag();

		enum {NOTDOWN, MOUSEDOWN, TABLETDOWN} pendown_;

		bool isdragging_;
		int dragx_,dragy_;

		QPoint prevpoint_;
		int outlinesize_, dia_;
		bool enableoutline_,showoutline_;
		bool crosshair_;
		QColor foreground_, background_;

		drawingboard::Board *board_;
};

}

#endif

