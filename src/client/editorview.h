/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
}

namespace widgets {
//! Editor view
/**
 * The editor view is a customized QGraphicsView that translates
 * user input to drawing commands.
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
		 * @param x initial x coordinate
		 * @param y initial y coordinate
		 * @param pressure initial pressure (always 1.0 if not supported)
		 * @param isEraser is the input device the wide end of a tablet stylus?
		 */
		void penDown(int x,int y, qreal pressure, bool isEraser);
		//! This signal is emitted when the pen or mouse pointer is moved while drawing
		void penMove(int x,int y, qreal pressure);

		//! This signal is emitted when the pen is lifted or the mouse button released.
		void penUp();

	public slots:
		//! Set the size of the brush preview outline
		void setOutlineSize(int size);

		//! Enable or disable cursor outline
		void setOutline(bool enable);
		//
		//! Enable or disable crosshair cursor
		void setCrosshair(bool enable);

	protected:
		void enterEvent(QEvent *event);
		void leaveEvent(QEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void tabletEvent(QTabletEvent *event);
	private:
		void startDrag(int x, int y);
		void moveDrag(int x, int y);
		void stopDrag();

		bool pendown_;

		bool isdragging_;
		QCursor oldcursor_;
		int dragx_,dragy_;

		QPoint prevpoint_;
		int outlinesize_;
		bool showoutline_;
		bool crosshair_;

		drawingboard::Board *board_;
};

}

#endif

