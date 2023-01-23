/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "thinsrv/gui/sidebaritemdelegate.h"

#include <QPainter>

namespace server {
namespace gui {

SidebarItemDelegate::SidebarItemDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

void SidebarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QRect rect = option.rect;
	rect.setLeft(0);
	if (!index.parent().isValid()) {
		// Paint section header
		painter->fillRect(rect, option.palette.brush(QPalette::Base));

		QRect textrect = rect;
		QString text = option.fontMetrics.elidedText(
			index.data(Qt::DisplayRole).toString(),
			Qt::ElideMiddle,
			textrect.width()
		);

		QStyleOptionViewItem opts = option;
		opts.state &= ~QStyle::State_Enabled;

		drawDisplay(painter, opts, textrect, text);
	} else {
		// Paint section page
		QPalette::ColorRole bg;
		if((option.state & QStyle::State_Selected)) {
			bg = QPalette::Highlight;
		} else {
			bg = QPalette::Base;
		}
		painter->fillRect(rect, option.palette.brush(bg));

		QRect textrect = rect.adjusted(12, 0, -12, 0);
		QString text = option.fontMetrics.elidedText(
			index.data(Qt::DisplayRole).toString(),
			Qt::ElideMiddle,
			textrect.width()
		);

		drawDisplay(painter, option, textrect, text);
	}
}

QSize SidebarItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize sz = QItemDelegate::sizeHint(option, index) + QSize(2, 8);
	if(!index.parent().isValid())
		sz.rheight() += 4;

	return sz;
}

}
}
