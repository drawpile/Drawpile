/*
   Drawpile - a collaborative drawing program.

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
#ifndef INDEXGRAPHICSITEM_H
#define INDEXGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QBrush>
#include <QPen>
#include <QPixmap>

#include "recording/index.h"

struct IndexGraphicsContext;

class IndexGraphicsItem : public QGraphicsItem
{
public:
	enum { Type = UserType + 20 };

	explicit IndexGraphicsItem(QGraphicsItem *parent=0);

	static const int STEP_WIDTH = 16;
	static const int ITEM_HEIGHT = 16;
	static const int VERTICAL_PADDING = 2;

	static void addToScene(const recording::IndexEntry &entry, QGraphicsScene *scene);

	/**
	 * @brief Populate a graphics scene with index items
	 * @param scene
	 * @param index
	 */
	static void buildScene(QGraphicsScene *scene, const recording::Index &index);

	QRectF boundingRect() const;

	/**
	 * @brief Has this event been marked as silenced?
	 *
	 * Silenced entries can be filtered out of the recording
	 * @return
	 */
	bool isSilenced() const { return _silenced; }

	const recording::IndexEntry &entry() const { return _entry; }

	int type() const { return Type; }

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);
	void mousePressEvent(QGraphicsSceneMouseEvent *event);

private:
	static void addToScene(const recording::IndexEntry &entry, const IndexGraphicsContext &ctx, QGraphicsScene *scene);

	QRectF _rect;
	QBrush _brush;
	QPen _pen;
	QPixmap _icon;

	bool _canSilence;
	bool _silenced;
	recording::IndexEntry _entry;
};

#endif // INDEXGRAPHICSITEM_H
