// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/listserverdelegate.h"

#include <QPainter>

namespace sessionlisting {

ListServerDelegate::ListServerDelegate(QObject *parent)
	: QItemDelegate(parent)
{

}

void ListServerDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	QPixmap favicon = index.data(Qt::DecorationRole).value<QIcon>().pixmap(64, 64);

	QRect decorationRect(opt.rect.x(), opt.rect.y(), favicon.width(), opt.rect.height());
	QRect textRect(qMax(64, decorationRect.width()) + 15, opt.rect.y(), opt.rect.width() - decorationRect.width() - 15, opt.rect.height());

	QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
							  ? QPalette::Normal : QPalette::Disabled;
	if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
		cg = QPalette::Inactive;

	if((opt.state & QStyle::State_Selected)) {
		painter->fillRect(opt.rect, opt.palette.brush(cg, QPalette::Highlight));
		painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
	} else {
		painter->setPen(opt.palette.color(cg, QPalette::Text));
	}

	drawBackground(painter, opt, index);
	drawDecoration(painter, opt, decorationRect, favicon);

	QFont font = opt.font;
	QFontMetrics fm(font);
	font.setWeight(QFont::Bold);
	painter->setFont(font);
	painter->drawText(textRect.topLeft() + QPoint(0, 2+fm.height()), index.data(Qt::DisplayRole).toString());

	font.setWeight(QFont::Normal);
	painter->setFont(font);
	textRect.setY(textRect.y() + fm.height() + 10);
	painter->drawText(textRect, index.data(Qt::UserRole+1).toString());

	drawFocus(painter, opt, textRect);

#if 0
	painter->setPen(QColor(Qt::red));
	painter->drawRect(opt.rect);
#endif

	painter->restore();
}

QSize ListServerDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize minsize(64, 64);
	QString desc = index.data(Qt::UserRole+1).toString();

	QFontMetrics fm(option.font);
	const QRect textrect(0, 0, 200, 64);
	const QRect descBounds = fm.boundingRect(textrect, Qt::TextWordWrap, desc);

	minsize = minsize.expandedTo(descBounds.size());

	return minsize;
}

}
