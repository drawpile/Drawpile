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

#include "indexgraphicsitem.h"
#include "recording/index.h"
#include "utils/icon.h"

#include <QDebug>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QPalette>

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
		return icon::fromTheme(name).pixmap(IndexGraphicsItem::ITEM_HEIGHT, IndexGraphicsItem::ITEM_HEIGHT);
	}

	QPixmap getBuiltinIcon(const QString &name)
	{
		return icon::fromBuiltin(name).pixmap(IndexGraphicsItem::ITEM_HEIGHT, IndexGraphicsItem::ITEM_HEIGHT);
	}
}

struct IndexGraphicsContext {
	QHash<int,int> ctxrow;

	QPixmap strokeIcon;
	QPixmap createLayerIcon;
	QPixmap deleteLayerIcon;
	QPixmap putimageIcon;
	QPixmap textIcon;
	QPixmap markerIcon;
	QPixmap newMarkerIcon;
	QPixmap chatIcon;
	QPixmap pauseIcon;
	QPixmap laserIcon;
	QPixmap undoIcon;
	QPixmap redoIcon;
	QCursor silencableCursor;
};
Q_DECLARE_METATYPE(IndexGraphicsContext)

void IndexGraphicsItem::buildScene(QGraphicsScene *scene, const recording::Index &index)
{
	IndexGraphicsContext ctx;

	// Load item icons
	ctx.strokeIcon = getIcon("draw-brush");
	ctx.createLayerIcon = getIcon("list-add");
	ctx.deleteLayerIcon = getIcon("edit-delete");
	ctx.putimageIcon = getIcon("edit-paste");
	ctx.textIcon = getIcon("draw-text");
	ctx.markerIcon = getIcon("flag-red");
	ctx.newMarkerIcon = getIcon("flag-blue");
	ctx.chatIcon = getBuiltinIcon("chat");
	ctx.pauseIcon = getIcon("media-playback-pause");
	ctx.laserIcon = getIcon("tool-laserpointer");
	ctx.undoIcon = getIcon("edit-undo");
	ctx.redoIcon = getIcon("edit-redo");

	ctx.silencableCursor = QCursor(Qt::PointingHandCursor);

	// Assign rows to each context ID in the order they appear
	ctx.ctxrow[0] = VERTICAL_PADDING; // special case: reserve context 0 as the topmost row

	float minx = 0;
	unsigned int end=0;
	{
		int row=1;
		foreach(const recording::IndexEntry &e, index.entries()) {
			if(e.end > end)
				end = e.end;
			if(!ctx.ctxrow.contains(e.context_id)) {
				ctx.ctxrow[e.context_id] = VERTICAL_PADDING + row * (ITEM_HEIGHT + VERTICAL_PADDING*2);

				// Add user name label
				auto *label = new QGraphicsTextItem(index.contextName(e.context_id));
				QPointF pos(-label->boundingRect().width() - 5, ctx.ctxrow[e.context_id]);
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
	int separatorY=0;
	for(int i=0;i<=ctx.ctxrow.size();++i) {
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
		addToScene(e, ctx, scene);
	}

	// Remember index context so we can add new items later
	scene->setProperty("idxctx", QVariant::fromValue<IndexGraphicsContext>(ctx));
}

void IndexGraphicsItem::addToScene(const recording::IndexEntry &entry, QGraphicsScene *scene)
{
	const IndexGraphicsContext &ctx = scene->property("idxctx").value<IndexGraphicsContext>();
	addToScene(entry, ctx, scene);
}

void IndexGraphicsItem::addToScene(const recording::IndexEntry &e, const IndexGraphicsContext &ctx, QGraphicsScene *scene)
{
	auto *item = new IndexGraphicsItem;

	item->_rect = QRectF(
				e.start * STEP_WIDTH,
				ctx.ctxrow[e.context_id],
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
		item->_icon = (e.flags & recording::IndexEntry::FLAG_ADDED) ? ctx.newMarkerIcon : ctx.markerIcon;
		break;
	case IDX_CREATELAYER:
		item->_icon = ctx.createLayerIcon;
		break;
	case IDX_DELETELAYER:
		item->_icon = ctx.deleteLayerIcon;
		break;
	case IDX_PUTIMAGE:
		item->_icon = ctx.putimageIcon;
		break;
	case IDX_STROKE:
		color = QColor::fromRgb(e.color);
		item->_icon = ctx.strokeIcon;
		style = BLOCK;
		break;
	case IDX_ANNOTATE:
		item->_icon = ctx.textIcon;
		style = BLOCK;
		break;
	case IDX_CHAT:
		item->_icon = ctx.chatIcon;
		break;
	case IDX_PAUSE:
		item->_icon = ctx.pauseIcon;
		break;
	case IDX_LASER:
		item->_icon = ctx.laserIcon;
		color = QColor::fromRgb(e.color);
		style = OUTLINE;
		break;
	case IDX_UNDO:
		item->_icon = ctx.undoIcon;
		silence = false; // undo&redo filtering is performed before silencing
		break;
	case IDX_REDO:
		item->_icon = ctx.redoIcon;
		silence = false;
		break;
	case IDX_FILL:
		color = QColor::fromRgb(e.color);
		style = BLOCK;
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
		item->setCursor(ctx.silencableCursor);
	}

	item->_entry = e;

	scene->addItem(item);
}

void IndexGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *)
{
	if(_canSilence) {
		_silenced = !_silenced;
		update();
	}
}

