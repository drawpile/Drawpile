/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006, 2010 Calle Laakkonen

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

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

//! Color button for foreground and background colors
/**
 * The DualColorButton provides two colors and signals to notify when they
 * have changed.
 */
class PLUGIN_EXPORT DualColorButton : public QWidget {
	Q_OBJECT
	public:
		DualColorButton(QWidget *parent=0);
		DualColorButton(const QColor& fgColor, const QColor& bgColor, QWidget *parent=0);

		//! Get the foreground color
		QColor foreground() const { return foreground_; }

		//! Get the background color
		QColor background() const { return background_; }

		//! Prefer square shape
		int heightForWidth(int w) const { return w; }

	public slots:
		//! Set foreground color
		void setForeground(const QColor &c);

		//! Set background color
		void setBackground(const QColor &c);

		//! Swap foreground and background colors
		void swapColors();

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
		void leaveEvent(QEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void paintEvent(QPaintEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

	private:
		QColor foreground_;
		QColor background_;

		QRectF foregroundRect() const;
		QRectF backgroundRect() const;
		QRectF resetBlackRect() const;
		QRectF resetWhiteRect() const;

		QPoint dragStart_;
		enum {NODRAG,FOREGROUND,BACKGROUND} dragSource_;
		int hilite_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

