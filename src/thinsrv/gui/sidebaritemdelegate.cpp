// SPDX-License-Identifier: GPL-3.0-or-later

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
