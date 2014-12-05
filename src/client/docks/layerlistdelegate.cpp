/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>

#include "net/client.h"
#include "net/layerlist.h"
#include "utils/icon.h"

#include "docks/layerlistdelegate.h"

namespace docks {

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent),
	  _visibleicon(icon::fromTheme("layer-visible-on").pixmap(16, 16)),
	  _hiddenicon(icon::fromTheme("layer-visible-off").pixmap(16, 16))
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const net::LayerListItem &layer = index.data().value<net::LayerListItem>();

	if(layer.isLockedFor(_client->myId()))
		opt.state &= ~QStyle::State_Enabled;

	drawBackground(painter, option, index);

	QRect textrect = opt.rect;

	// Draw layer opacity glyph
	QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
	drawOpacityGlyph(stylerect, painter, layer.opacity, layer.hidden);

	// Draw layer name
	textrect.setLeft(stylerect.right());
	drawDisplay(painter, opt, textrect, layer.title);

	painter->restore();
}

bool LayerListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(event->type() == QEvent::MouseButtonRelease) {
		const net::LayerListItem &layer = index.data().value<net::LayerListItem>();
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);

		if(me->button() == Qt::LeftButton) {
			if(me->x() < 24) {
				// Clicked on opacity glyph: toggle visibility
				emit toggleVisibility(layer.id, layer.hidden);
			}
		}
	}

	return QItemDelegate::editorEvent(event, model, option, index);
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	const QSize iconsize = _visibleicon.size();
	QFontMetrics fm(option.font);
	int minheight = qMax(fm.height() * 3 / 2, iconsize.height()) + 2;
	if(size.height() < minheight)
		size.setHeight(minheight);
	return size;
}

void LayerListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex &) const
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
		painter->drawPixmap(x, y, _hiddenicon);
	} else {
		painter->save();
		painter->setOpacity(value);
		painter->drawPixmap(x, y, _visibleicon);
		painter->restore();
	}
}

}
