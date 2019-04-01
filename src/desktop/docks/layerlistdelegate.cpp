/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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

#include "docks/layerlistdelegate.h"
#include "canvas/layerlist.h"
#include "utils/icon.h"
#include "../shared/net/layer.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>

namespace docks {

static const QSize ICON_SIZE { 16, 16 };

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent),
	  m_visibleIcon(icon::fromTheme("layer-visible-on")),
	  m_censoredIcon(QIcon(":/icons/censored.svg")),
	  m_hiddenIcon(icon::fromTheme("layer-visible-off")),
	  m_fixedIcon(icon::fromTheme("window-pin")),
	  m_showNumbers(false)
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();

	if(index.data(canvas::LayerListModel::IsLockedRole).toBool())
		opt.state &= ~QStyle::State_Enabled;

	drawBackground(painter, option, index);

	QRect textrect = opt.rect;

	// Draw layer opacity glyph
	QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
	drawOpacityGlyph(stylerect, painter, layer.opacity, layer.hidden, layer.censored);

	// Draw layer name
	textrect.setLeft(stylerect.right());

	QString title;
	if(m_showNumbers)
		title = QString("%1 - %2").arg(index.model()->rowCount() - index.row() - 1).arg(layer.title);
	else
		title = layer.title;

	if(index.data(canvas::LayerListModel::IsDefaultRole).toBool()) {
		opt.font.setUnderline(true);
	}

	drawDisplay(painter, opt, textrect, title);

	// Draw fixed icon
	if(layer.fixed) {
		m_fixedIcon.paint(painter,
			opt.rect.right() - ICON_SIZE.width(),
			opt.rect.top() + opt.rect.height()/2 - ICON_SIZE.height()/2,
			ICON_SIZE.width(),
			ICON_SIZE.height()
		);
	}

	painter->restore();
}

bool LayerListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(event->type() == QEvent::MouseButtonPress) {
		const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);

		if(me->button() == Qt::LeftButton) {
			if(me->x() < 24) {
				// Clicked on opacity glyph: toggle visibility
				emit toggleVisibility(layer.id, layer.hidden);
				return true;
			}
		}
	}

	return QItemDelegate::editorEvent(event, model, option, index);
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	QFontMetrics fm(option.font);
	int minheight = qMax(fm.height() * 3 / 2, ICON_SIZE.height()) + 2;
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
	const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();
	QString newtitle = static_cast<QLineEdit*>(editor)->text();
	if(layer.title != newtitle) {
		emit const_cast<LayerListDelegate*>(this)->layerCommand(protocol::MessagePtr(new protocol::LayerRetitle(0, layer.id, newtitle)));
	}
}

void LayerListDelegate::drawOpacityGlyph(const QRectF& rect, QPainter *painter, float value, bool hidden, bool censored) const
{
	const QRect r {
		int(rect.left() + rect.width() / 2 - ICON_SIZE.width()/2),
		int(rect.top() + rect.height() / 2 - ICON_SIZE.height()/2),
		ICON_SIZE.width(),
		ICON_SIZE.height(),
	};

	if(hidden) {
		m_hiddenIcon.paint(painter, r);

	} else {
		painter->save();
		painter->setOpacity(value);
		if(censored)
			m_censoredIcon.paint(painter, r);
		else
			m_visibleIcon.paint(painter, r);
		painter->restore();
	}
}

void LayerListDelegate::setShowNumbers(bool show) {
	m_showNumbers = show;
	emit sizeHintChanged(QModelIndex()); // trigger repaint
}

}
