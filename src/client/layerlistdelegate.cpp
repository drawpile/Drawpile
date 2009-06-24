/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2009 Calle Laakkonen

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

#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>

#include "layerlistdelegate.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "layerwidget.h"
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
		opt.displayAlignment = Qt::AlignVCenter;

		const QPixmap addicon = icon::add().pixmap(16);
		painter->drawPixmap(opt.rect.topLeft()+QPoint(0,opt.rect.height()/2-addicon.height()/2),
				addicon);
		drawDisplay(painter, opt, textrect.adjusted(addicon.width(),0,0,0), "New layer...");
	} else {
		const dpcore::Layer *layer = index.data().value<dpcore::Layer*>();

		const QSize delsize = icon::remove().actualSize(QSize(16,16));
		const QSize hidesize = icon::layervisible().actualSize(QSize(16,16));

		// Draw layer hide button
		QRect btnrect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-hidesize.height()/2), hidesize);

		if(layer->hidden()==false)
			painter->drawPixmap(btnrect.topLeft(), icon::layervisible().pixmap(16));

		// Draw opacity meter
		btnrect.translate(btnrect.width(), 0);
		drawMeter(btnrect, painter,
				option.palette.color(QPalette::Button),
				option.palette.color(QPalette::Dark),
				layer->opacity() / 255.0
				);

		// Draw layer name
		textrect.setLeft(btnrect.right());
		textrect.setWidth(textrect.width() - delsize.width());
		drawDisplay(painter, opt, textrect, layer->name());

		// Draw delete button (except when in a network session, and when this is the last layer)
		const dpcore::LayerStack *layers = static_cast<const dpcore::LayerStack*>(index.model());
		if(layers->layers()>1) {
			painter->drawPixmap(opt.rect.topRight()-QPoint(delsize.width(), -opt.rect.height()/2+delsize.height()/2), icon::remove().pixmap(16));
		}

	}
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
        QSize size = QItemDelegate::sizeHint(option, index);
        const QSize iconsize = icon::lock().actualSize(QSize(16,16));
		QFontMetrics fm(option.font);
		int minheight = qMax(fm.height() * 2, iconsize.height()) + 2;
        if(size.height() < minheight)
                size.setHeight(minheight);
        return size;
}

bool LayerListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
		const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(event->type() == QEvent::MouseButtonRelease) {
		const int btnwidth = icon::lock().actualSize(QSize(16,16)).width();
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);
		if(index.row()==0) {
			emit newLayer();
		} else {
			const dpcore::LayerStack *layers = static_cast<const dpcore::LayerStack*>(index.model());
			const dpcore::Layer *layer = index.data().value<dpcore::Layer*>();
			// Delete button (but only when this is not the last layer and we are not in a network session)
			if(me->x() < btnwidth) {
				emit layerToggleHidden(layer->id());
			} else if(me->x() < btnwidth*2) {
				widgets::LayerEditor *lw = new widgets::LayerEditor(layer);
				lw->move(me->globalPos());
				lw->connect(lw, SIGNAL(opacityChanged(int,int)), this, SIGNAL(changeOpacity(int,int)));
				lw->show();

			} else if(me->x() >= option.rect.width() - btnwidth) {
				if(layers->layers()>1)
					emit deleteLayer(layer);
			}
		}
	}
	return QItemDelegate::editorEvent(event, model, option, index);
}

void LayerListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	QSize size = sizeHint(option, index);
	const int btnwidth = icon::lock().actualSize(QSize(16,16)).width();

	editor->setGeometry(QRect(
				btnwidth*2,
				index.row() * size.height()+1,
				size.width() - 2 * size.width(),
				size.height()-2
				));
}

void LayerListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
	emit renameLayer(
			index.data().value<dpcore::Layer*>()->id(),
			static_cast<QLineEdit*>(editor)->text()
			);
}

/**
 * Draw a meter glyph (used for displaying layer opacity)
 */
void LayerListDelegate::drawMeter(const QRectF& rect, QPainter *painter, const QColor& background, const QColor& foreground, float value) const
{
	painter->save();

	// Start at topright corner
	const qreal w = rect.height() * 0.6;
	const qreal gap = (rect.width() - w) / 2.0;
	QPainterPath path(QPointF(rect.right() - gap , rect.y()));
	path.lineTo(rect.left() + gap, rect.top());
	path.lineTo(rect.right() - rect.width()/2, rect.bottom());
	path.lineTo(rect.right() - gap, rect.y());

	painter->setClipPath(path);
	painter->fillRect(rect, QBrush(background));
	painter->fillRect(rect.adjusted(0, rect.height() - rect.height() * value, 0, 0), QBrush(foreground));
	painter->restore();

}


