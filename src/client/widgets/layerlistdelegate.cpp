/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2010 Calle Laakkonen

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

#include "layerlistdelegate.h"
#include "layerlistwidget.h"
#include "layerlistitem.h"
#include "layerwidget.h"

#include "icons.h"

namespace widgets {

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	// This is copied from QItemDelegate::paint
	QStyleOptionViewItemV4 opt = setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);
	const QStyleOptionViewItemV3 *v3 = qstyleoption_cast<const QStyleOptionViewItemV3 *>(&option);
    opt.locale = v3 ? v3->locale : QLocale();
    opt.widget = v3 ? v3->widget : 0;

	// Background
	// Note. We have to do this manually, instead of just calling drawBackground(),
	// because in certain styles (like plastic) the highlight will be drawn in
	// drawDisplay instead of here. That would be ugly.
	if (opt.state & QStyle::State_Selected) {
		painter->fillRect(opt.rect, opt.palette.brush(QPalette::Normal, QPalette::Highlight));
	}

	QRect textrect = opt.rect;

	if(index.row()==0) {
		// Draw add new layer button
		opt.font.setStyle(QFont::StyleItalic);
		opt.displayAlignment = Qt::AlignVCenter;

		const QPixmap addicon = icon::add().pixmap(24);
		painter->drawPixmap(opt.rect.topLeft()+QPoint(0,opt.rect.height()/2-addicon.height()/2),
				addicon);
		drawDisplay(painter, opt, textrect.adjusted(addicon.width(),0,0,0), tr("New layer..."));
	} else {
		LayerListItem layer = index.data().value<LayerListItem>();

		const QSize delsize = icon::remove().actualSize(QSize(16,16));

		QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
		drawStyleGlyph(stylerect, painter, option.palette, layer.opacity, layer.hidden);

		// Draw layer name
		textrect.setLeft(stylerect.right());
		textrect.setWidth(textrect.width() - delsize.width());
		drawDisplay(painter, opt, textrect, layer.title);

		// Draw delete button (except when in a network session, and when this is the last layer)
#if 0
		const dpcore::LayerStack *layers = static_cast<const dpcore::LayerStack*>(index.model());
		if(layers->layers()>1) {
			painter->drawPixmap(opt.rect.topRight()-QPoint(delsize.width(), -opt.rect.height()/2+delsize.height()/2), icon::remove().pixmap(16));
		}
#endif
	}
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
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
	const int btnsize = 24;
	if(event->type() == QEvent::MouseButtonRelease) {
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);
		if(index.row()==0) {
			emit newLayer();
		} else {
#if 0
			const dpcore::Layer *layer = index.data().value<dpcore::Layer*>();

			if(me->x() < btnsize) {
				// Layer style button
				widgets::LayerStyleEditor *lw = new widgets::LayerStyleEditor(layer);
				lw->move(me->globalPos() - QPoint(15, 15));
				lw->connect(lw, SIGNAL(opacityChanged(int,int)), this, SIGNAL(changeOpacity(int,int)));
				lw->connect(lw, SIGNAL(toggleHidden(int)), this, SIGNAL(layerToggleHidden(int)));
				lw->show();

			} else if(me->x() >= option.rect.width() - btnsize) {
				// Delete button
				const dpcore::LayerStack *layers = static_cast<const dpcore::LayerStack*>(index.model());
				if(layers->layers()>1)
					emit deleteLayer(layer);
			}
#endif
		}
	} else if(event->type() == QEvent::MouseButtonPress) {
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);
		// Select layer on mousedown because it feels more responsive
		// and if start dragging some other layer than what is currently
		// selected, we drag the right one. (i.e. the one under the pointer)
		if(index.row()>0 && me->x() > btnsize && me->x() < option.rect.width() - btnsize) {
			emit select(index);
		}
	}

	return QItemDelegate::editorEvent(event, model, option, index);
}

void LayerListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	const int btnwidth = 24;

	static_cast<QLineEdit*>(editor)->setFrame(true);
	editor->setGeometry(option.rect.adjusted(btnwidth, 0, -btnwidth, 0));
}

void LayerListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
#if 0
	emit renameLayer(
			index.data().value<dpcore::Layer*>()->id(),
			static_cast<QLineEdit*>(editor)->text()
			);
#endif
}

void LayerListDelegate::drawStyleGlyph(const QRectF& rect, QPainter *painter,const QPalette& palette, float value, bool hidden) const
{
	painter->save();

	QRectF gr = rect.adjusted(2,2,-2,-2);
	painter->setRenderHint(QPainter::Antialiasing);

	// Draw the background
	painter->setPen(palette.color(QPalette::Dark));
	QLinearGradient grad(gr.topLeft(), gr.bottomLeft());
	grad.setColorAt(0, palette.color(QPalette::Light));
	grad.setColorAt(1, palette.color(QPalette::Mid));
	painter->setBrush(grad);

	painter->drawEllipse(gr);

	// Fill glyph to indicate opacity
	painter->setClipRect(QRectF(gr.topLeft(), QSize(gr.width() * value, gr.height())));
	grad.setColorAt(0, palette.color(QPalette::Dark));
	grad.setColorAt(1, palette.color(QPalette::Shadow));
	painter->setBrush(grad);

	painter->drawEllipse(gr.adjusted(1,1,-1,-1));
	painter->setClipping(false);

	if(hidden) {
		// Indicate if layer is hidden
		grad.setColorAt(0, QColor(255,0,0));
		grad.setColorAt(1, QColor(155,0,0));
		painter->setPen(QPen(grad, 3, Qt::SolidLine, Qt::RoundCap));
		painter->drawLine(gr.topLeft(), gr.bottomRight());
		painter->drawLine(gr.topRight(), gr.bottomLeft());
	}

	painter->restore();
}

}
