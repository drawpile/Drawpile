
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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

