/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QDebug>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsSceneHoverEvent>
#include <QIcon>
#include <QStyleOptionGraphicsItem>
#include <QPalette>

#include "indexgraphicsitem.h"
#include "recording/index.h"

IndexGraphicsItem::IndexGraphicsItem(QGraphicsItem *parent)
	: QGraphicsItem(parent), _canSilence(false), _silenced(false)
{
}

QRectF IndexGraphicsItem::boundingRect() const
{
	return _rect;
}

void IndexGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *)
{
	Q_UNUSED(options);
	if(isSilenced()) {
		QPen pen = _pen;
		pen.setStyle(Qt::DashDotDotLine);
		painter->setPen(pen);

		QBrush brush = _brush;
		QColor color = brush.color();
		color.setAlphaF(0.5);
		brush.setColor(color);
		painter->setBrush(brush);
	} else {
		painter->setPen(_pen);
		painter->setBrush(_brush);
	}

	painter->drawRect(_rect);
	if(!_icon.isNull()) {
		if(isSilenced())
			painter->setOpacity(0.5);
		painter->drawPixmap(_rect.topLeft(), _icon);
	}
}

namespace {

	QPixmap getIcon(const QString &name)
	{
		return QIcon::fromTheme(name, QIcon(":icons/" + name)).pixmap(IndexGraphicsItem::ITEM_HEIGHT, IndexGraphicsItem::ITEM_HEIGHT);
	}
}
void IndexGraphicsItem::buildScene(QGraphicsScene *scene, const recording::Index &index)
{
	// Load item icons
	const QPixmap strokeIcon = getIcon("draw-brush");
	const QPixmap createLayerIcon = getIcon("list-add");
	const QPixmap deleteLayerIcon = getIcon("list-remove");
	const QPixmap putimageIcon = getIcon("edit-paste");
	const QPixmap textIcon = getIcon("draw-text");
	const QPixmap markerIcon = getIcon("flag-red");
	const QPixmap chatIcon = getIcon("chat");
	const QPixmap pauseIcon = getIcon("media-playback-pause");
	const QPixmap laserIcon = getIcon("tool-laserpointer");

	const QCursor silencableCursor = QCursor(Qt::PointingHandCursor);

	// Assign rows to each context ID in the order they appear
	QHash<int, int> ctxrow;
	ctxrow[0] = VERTICAL_PADDING; // special case: reserve context 0 as the topmost row

	float minx = 0;
	unsigned int end=0;
	{
		int row=1;
		foreach(const recording::IndexEntry &e, index.entries()) {
			if(e.end > end)
				end = e.end;
			if(!ctxrow.contains(e.context_id)) {
				ctxrow[e.context_id] = VERTICAL_PADDING + row * (ITEM_HEIGHT + VERTICAL_PADDING*2);

				// Add user name label
				auto *label = new QGraphicsTextItem(index.contextName(e.context_id));
				QPointF pos(-label->boundingRect().width() - 5, ctxrow[e.context_id]);
				if(pos.x() < minx)
					minx = pos.x();
				label->setPos(pos);
				scene->addItem(label);

				++row;
			}
		}
	}

	++end;

	// Create row separator lines
	QPen separatorpen(Qt::gray);
	separatorpen.setStyle(Qt::DotLine);
	int separatorY;
	for(int i=0;i<=ctxrow.size();++i) {
		separatorY = i * (ITEM_HEIGHT+VERTICAL_PADDING*2);
		auto *line = new QGraphicsLineItem(minx, separatorY, end*STEP_WIDTH, separatorY);
		line->setPen(separatorpen);
		scene->addItem(line);
	}

	// Create snapshot indicator lines
	foreach(const recording::SnapshotEntry &e, index.snapshots()) {
		int x = e.pos * STEP_WIDTH;
		auto *line = new QGraphicsLineItem(x, -10, x, separatorY);
		line->setPen(separatorpen);
		scene->addItem(line);
	}

	// Create a graphical item for each index entry
	foreach(const recording::IndexEntry &e, index.entries()) {
		auto *item = new IndexGraphicsItem;

		item->_rect = QRectF(
					e.start * STEP_WIDTH,
					ctxrow[e.context_id],
					(e.end - e.start + 1) * STEP_WIDTH,
					ITEM_HEIGHT
		);

		enum {ICON_ONLY, BLOCK, OUTLINE} style = e.start == e.end ? ICON_ONLY : BLOCK;
		QColor color(Qt::white);

		bool silence = true;

		switch(e.type) {
		using namespace recording;
		case IDX_NULL:
			color = Qt::black;
			silence = false;
			break;
		case IDX_MARKER:
			item->_icon = markerIcon;
			break;
		case IDX_CREATELAYER:
			item->_icon = createLayerIcon;
			break;
		case IDX_DELETELAYER:
			item->_icon = deleteLayerIcon;
			break;
		case IDX_PUTIMAGE:
			item->_icon = putimageIcon;
			break;
		case IDX_STROKE:
			color = QColor::fromRgb(e.color);
			item->_icon = strokeIcon;
			style = BLOCK;
			break;
		case IDX_ANNOTATE:
			item->_icon = textIcon;
			style = BLOCK;
			break;
		case IDX_CHAT:
			item->_icon = chatIcon;
			break;
		case IDX_PAUSE:
			item->_icon = pauseIcon;
			break;
		case IDX_LASER:
			item->_icon = laserIcon;
			color = QColor::fromRgb(e.color);
			style = OUTLINE;
			break;
		default: break;
		}

		switch(style) {
		case ICON_ONLY:
			item->_pen = Qt::NoPen;
			break;
		case BLOCK:
			item->_brush = QBrush(color);
			item->_pen = QPen(item->_brush.color().darker(120));
			break;
		case OUTLINE:
			item->_pen = QPen(color);
			break;
		}

		if(!e.title.isEmpty())
			item->setToolTip(e.title);

		if(silence) {
			item->_canSilence = true;
			item->setCursor(silencableCursor);
		}

		item->_entry = e;

		scene->addItem(item);
	}
}

void IndexGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *)
{
	if(_canSilence) {
		_silenced = !_silenced;
		update();
	}
}

