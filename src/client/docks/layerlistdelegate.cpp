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
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

#include "net/client.h"
#include "net/layerlist.h"

#include "docks/layerlistdelegate.h"
#include "widgets/layerwidget.h"

#include "icons.h"

namespace widgets {

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();
	
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
		drawDisplay(painter, opt, textrect.adjusted(addicon.width(),0,0,0), index.data().toString());
	} else {
		net::LayerListItem layer = index.data().value<net::LayerListItem>();

		const QSize delsize = icon::remove().actualSize(QSize(16,16));

		// Draw layer opacity glyph
		QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
		drawStyleGlyph(stylerect, painter, option.palette, layer.opacity, layer.hidden);

		// Draw layer lock icon
		QRect lockrect(opt.rect.topLeft() + QPoint(stylerect.right(), 0), QSize(24, 24));
		painter->drawPixmap(
			lockrect.topLeft() + QPoint(4, 6),
			icon::lock().pixmap(
				16,
				QIcon::Normal,
				layer.isLockedFor(_client->myId()) ?  QIcon::On : QIcon::Off
			)
		);

		// Draw layer name
		textrect.setLeft(lockrect.right());
		textrect.setWidth(textrect.width() - delsize.width());
		drawDisplay(painter, opt, textrect, layer.title);

		// Draw delete button
		painter->drawPixmap(opt.rect.topRight()-QPoint(delsize.width(), -opt.rect.height()/2+delsize.height()/2), icon::remove().pixmap(16));
	}
	
	painter->restore();
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
			clickNewLayer();
		} else {
			if(me->x() < btnsize) {
				// Layer style button
				widgets::LayerStyleEditor *lw = new widgets::LayerStyleEditor(index);
				lw->move(me->globalPos() - QPoint(15, 15));
				lw->connect(lw, SIGNAL(opacityChanged(QModelIndex,int)), this, SLOT(changeOpacity(QModelIndex,int)));
				lw->connect(lw, SIGNAL(setHidden(QModelIndex, bool)), this, SLOT(setVisibility(QModelIndex,bool)));
				lw->show();
			} else if(me->x() < 2*btnsize) {
				// Layer lock button (TODO user exclusive access)
				clickLockLayer(index);
			} else if(me->x() >= option.rect.width() - btnsize) {
				// Delete button
				clickDeleteLayer(index);
			}
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
	editor->setGeometry(option.rect.adjusted(btnwidth*2, 0, -btnwidth, 0));
}

void LayerListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
	net::LayerListItem layer = index.data().value<net::LayerListItem>();
	QString newtitle = static_cast<QLineEdit*>(editor)->text();
	if(layer.title != newtitle) {
		layer.title = newtitle;
		sendLayerAttribs(layer);
	}
}


/**
 * Opacity slider was adjusted
 */
void LayerListDelegate::changeOpacity(const QModelIndex &index, int opacity)
{
	net::LayerListItem layer = index.data().value<net::LayerListItem>();
	layer.opacity = opacity / 255.0;
	sendLayerAttribs(layer);
}

void LayerListDelegate::setVisibility(const QModelIndex &index, bool hidden)
{
	Q_ASSERT(_client);
	net::LayerListItem layer = index.data().value<net::LayerListItem>();
	_client->sendLayerVisibility(layer.id, hidden);
}

void LayerListDelegate::clickNewLayer()
{
	bool ok;
	QString name = QInputDialog::getText(0,
		tr("Add a new layer"),
		tr("Layer name:"),
		QLineEdit::Normal,
		"",
		&ok
	);
	if(ok) {
		if(name.isEmpty())
			name = tr("Unnamed layer");
		_client->sendNewLayer(0, Qt::transparent, name);
	}
}

void LayerListDelegate::clickLockLayer(const QModelIndex &index)
{
	Q_ASSERT(_client);
	net::LayerListItem layer = index.data().value<net::LayerListItem>();

	layer.locked = !layer.locked;
	sendLayerAcl(layer);
}

void LayerListDelegate::clickDeleteLayer(const QModelIndex &index)
{
	Q_ASSERT(_client);
	net::LayerListItem layer = index.data().value<net::LayerListItem>();

	QMessageBox box(QMessageBox::Question,
		tr("Delete layer"),
		tr("Really delete \"%1\"?").arg(layer.title),
		QMessageBox::NoButton
	);

	box.addButton(tr("Delete"), QMessageBox::DestructiveRole);

	// Offer the choice to merge down only if there is a layer
	// below this one.
	QPushButton *merge = 0;
	if(index.sibling(index.row()+1, 0).isValid()) {
		merge = box.addButton(tr("Merge down"), QMessageBox::DestructiveRole);
		box.setInformativeText(tr("Press merge down to merge the layer with the first visible layer below instead of deleting."));
	}

	QPushButton *cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);

	box.setDefaultButton(cancel);
	box.exec();

	QAbstractButton *choice = box.clickedButton();
	if(choice != cancel)
		_client->sendDeleteLayer(layer.id, choice==merge);
}

void LayerListDelegate::sendLayerAttribs(const net::LayerListItem &layer) const
{
	Q_ASSERT(_client);

	_client->sendLayerAttribs(layer.id, layer.opacity, layer.title);
}

void LayerListDelegate::sendLayerAcl(const net::LayerListItem &layer) const
{
	Q_ASSERT(_client);

	_client->sendLayerAcl(layer.id, layer.locked, layer.exclusive);
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
