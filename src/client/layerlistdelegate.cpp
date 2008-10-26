/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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
#include "layerlistdelegate.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "icons.h"

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

LayerListDelegate::~LayerListDelegate()
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItemV2 opt = setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);

	// Background
	drawBackground(painter, opt, index);
	
	QRect textrect = opt.rect;

	if(index.row()==0) {
		// Draw add new layer button
		opt.font.setStyle(QFont::StyleItalic);
		opt.displayAlignment = Qt::AlignHCenter;
		drawDisplay(painter, opt, textrect, "New layer...");
	} else {
		const dpcore::Layer *layer = index.data().value<dpcore::Layer*>();
		QString name = QString("%1 %2 %3%").arg(layer->name()).arg("Normal").arg(100);

		const int delwidth = icon::kick().actualSize(QSize(16,16)).width();
		// Draw layer name
		textrect.setWidth(textrect.width() - delwidth);
		drawDisplay(painter, opt, textrect, name);

		// Draw delete button (except when in a network session)
		// TODO correct icon
		painter->drawPixmap(opt.rect.topRight()-QPoint(delwidth,0),
				icon::kick().pixmap(16));
	}
}

#if 0
QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
        QSize size = QItemDelegate::sizeHint(option, index);
        const QSize iconsize = icon::lock().actualSize(QSize(16,16));
        if(size.height() < iconsize.height())
                size.setHeight(iconsize.height());
        return size;
}
#endif

bool LayerListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
		const QStyleOptionViewItem &option, const QModelIndex &index)
{
	const int btnwidth = icon::lock().actualSize(QSize(16,16)).width();
	if(event->type() == QEvent::MouseButtonRelease) {
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);
		if(index.row()==0) {
			emit newLayer();
		} else {
			const dpcore::Layer *layer = index.data().value<dpcore::Layer*>();
			if(me->x() >= option.rect.width() - btnwidth)
				emit deleteLayer(layer);
		}
	}
	return QItemDelegate::editorEvent(event, model, option, index);
}

