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
#ifndef DUALCOLORBUTTON_H
#define DUALCOLORBUTTON_H

#include <QWidget>

namespace widgets {

//! Color button for foreground and background colors
/**
 * The DualColorButton provides two colors and signals to notify when they
 * have changed.
 */
class DualColorButton : public QWidget {
	Q_OBJECT
	public:
		DualColorButton(QWidget *parent=0);
		DualColorButton(const QColor& fgColor, const QColor& bgColor, QWidget *parent=0);

		//! Get the foreground color
		QColor foreground() const { return foreground_; }

		//! Get the background color
		QColor background() const { return background_; }

	public slots:
		//! Set foreground color
		void setForeground(const QColor &c);

		//! Set background color
		void setBackground(const QColor &c);

	signals:
		//! Emitted when foreground color is changed
		void foregroundChanged(const QColor &c);

		//! Emitted when background color is changed
		void backgroundChanged(const QColor &c);

		//! Emitted when the user clicks on the foreground color box
		void foregroundClicked();

		//! Emitted when the user clicks on the background color box
		void backgroundClicked();

	protected:
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void paintEvent(QPaintEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

	private:
		QColor foreground_;
		QColor background_;

		QRect foregroundRect() const;
		QRect backgroundRect() const;
		QRect resetBlackRect() const;
		QRect resetWhiteRect() const;

		QPoint dragStart_;
		enum {NODRAG,FOREGROUND,BACKGROUND} dragSource_;
};

}

#endif

