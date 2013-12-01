
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#ifndef POPUPMESSAGE_H
#define POPUPMESSAGE_H

#include <QWidget>
#include <QTimer>

class QLabel;

namespace widgets {

/**
 * @brief Popup messagebox
 * A simple box that can be popped up to display a message.
 */
class PopupMessage : public QWidget
{
	Q_OBJECT
	public:
		PopupMessage(QWidget *parent);

		//! Set the message to display
		void setMessage(const QString& message);

		// Pop up at the specified point
		void popupAt(const QPoint& point);

	protected:
		//void resizeEvent(QResizeEvent *);
		void paintEvent(QPaintEvent *);

	private:
		void redrawBubble();

		qreal arrowoffset_;
		QPainterPath bubble;
		QLabel *message_;
		QTimer timer_;
};

}

#endif

