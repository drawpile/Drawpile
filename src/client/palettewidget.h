/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef PALETTEWIDGET_H
#define PALETTEWIDGET_H

#include <QWidget>

class Palette;
class QScrollBar;
class QRubberBand;

namespace widgets {

class PaletteWidget : public QWidget {
	Q_OBJECT
	public:
		PaletteWidget(QWidget *parent);
		void setPalette(::Palette *palette);

	signals:
		void colorSelected(const QColor& color);

	protected:
		void resizeEvent(QResizeEvent *event);
		void paintEvent(QPaintEvent *);

		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void wheelEvent(QWheelEvent *event);

		void dragEnterEvent(QDragEnterEvent *event);
		void dragMoveEvent(QDragMoveEvent *event);
		void dragLeaveEvent(QDragLeaveEvent *event);
		void dropEvent(QDropEvent *event);

	private slots:
		void scroll(int pos);

	private:
		int columns() const;
		int indexAt(const QPoint& point) const;
		int nearestAt(const QPoint& point) const;
		QRect swatchRect(int index) const;
		QRect betweenRect(int index) const;

		::Palette *palette_;
		QScrollBar *scrollbar_;
		QSize swatchsize_;
		int spacing_;
		int scroll_;
		QPoint dragstart_;
		int dragsource_;
		QRubberBand *dragtarget_;
};

}

#endif

