/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2017 Calle Laakkonen

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
#include <QPainterPath>

class QTextDocument;
class QTimer;

namespace widgets {

/**
 * @brief Popup messagebox
 * A simple box that can be popped up to display a message.
 */
class PopupMessage final : public QWidget
{
	Q_OBJECT
public:
	PopupMessage(QWidget *parent);

	/**
	 * @brief Pop up the message box and show a message
	 *
	 * If the popup is already visible, the message will be appended
	 * to the existing one.
	 *
	 * @param point origin point for the popup (the little arrow will point here)
	 * @param message the message to show.
	 */
	void showMessage(const QPoint& point, const QString &message);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	void setMessage(const QString &message);
	void redrawBubble();

	qreal m_arrowoffset;
	QPainterPath m_bubble;
	QTimer *m_timer;
	QTextDocument *m_doc;
	QWidget *m_parentWidget;
};

}

#endif

