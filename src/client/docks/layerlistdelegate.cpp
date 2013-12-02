/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>

#include "net/client.h"
#include "net/layerlist.h"

#include "docks/layerlistdelegate.h"

#include "utils/icons.h"

namespace widgets {

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	drawBackground(painter, option, index);

	QRect textrect = opt.rect;

	const net::LayerListItem &layer = index.data().value<net::LayerListItem>();

	const QSize locksize = icon::lock().actualSize(QSize(16,16));

	// Draw layer opacity glyph
	QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
	drawOpacityGlyph(stylerect, painter, layer.opacity, layer.hidden);

	// Draw layer name
	textrect.setLeft(stylerect.right());
	textrect.setWidth(textrect.width() - locksize.width());
	drawDisplay(painter, opt, textrect, layer.title);

	// Draw lock button
	if(layer.isLockedFor(_client->myId()))
		painter->drawPixmap(
			opt.rect.topRight()-QPoint(locksize.width(), -opt.rect.height()/2+locksize.height()/2),
			icon::lock().pixmap(16, QIcon::Normal, QIcon::On)
		);

	painter->restore();
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	const QSize iconsize = icon::lock().actualSize(QSize(16,16));
	QFontMetrics fm(option.font);
	int minheight = qMax(fm.height() * 3 / 2, iconsize.height()) + 2;
	if(size.height() < minheight)
		size.setHeight(minheight);
	return size;
}

void LayerListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	const int btnwidth = 24;

	static_cast<QLineEdit*>(editor)->setFrame(true);
	editor->setGeometry(option.rect.adjusted(btnwidth, 0, -btnwidth, 0));
}

void LayerListDelegate::setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex& index) const
{
	const net::LayerListItem &layer = index.data().value<net::LayerListItem>();
	QString newtitle = static_cast<QLineEdit*>(editor)->text();
	if(layer.title != newtitle) {
		_client->sendLayerTitle(layer.id, newtitle);
	}
}

void LayerListDelegate::drawOpacityGlyph(const QRectF& rect, QPainter *painter, float value, bool hidden) const
{
	int x = rect.left() + rect.width() / 2 - 8;
	int y = rect.top() + rect.height() / 2 - 8;
	if(hidden) {
		painter->drawPixmap(x, y, icon::layerHide().pixmap(QSize(16,16), QIcon::Normal, QIcon::On));
	} else {
		painter->save();
		painter->setOpacity(value);
		painter->drawPixmap(x, y, icon::layerHide().pixmap(QSize(16,16), QIcon::Normal, QIcon::Off));
		painter->restore();
	}
}

}
