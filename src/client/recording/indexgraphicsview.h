/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef INDEXGRAPHICSVIEW_H
#define INDEXGRAPHICSVIEW_H

#include <QGraphicsView>

/**
 * @brief A specialized graphics view for visualizing a recording index
 *
 */
class IndexGraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	explicit IndexGraphicsView(QWidget *parent = 0);

protected:
	void mouseMoveEvent(QMouseEvent *e);
	void mouseDoubleClickEvent(QMouseEvent *e);
	void drawBackground(QPainter *painter, const QRectF &rect);

signals:
	//! User requested jump to given recording position
	void jumpRequest(int position);

private:
	int _highlight;
	QRectF _highlightrect;
	QBrush _highlightbrush;
};

#endif // INDEXGRAPHICSVIEW_H
