// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/listserverdelegate.h"
#include "libclient/utils/listservermodel.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

namespace sessionlisting {

ListServerDelegate::ListServerDelegate(QObject *parent)
	: QItemDelegate(parent)
{

}

static auto getMetric(QStyle::PixelMetric pm, const QStyleOptionViewItem &option)
{
	const auto *style = option.widget ? option.widget->style() : QApplication::style();
	return style->pixelMetric(pm, &option, option.widget);
}

void ListServerDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	auto opt = setOptions(index, option);

	const auto iconSize = getMetric(QStyle::PM_LargeIconSize, opt);
	const auto favicon = index.data(Qt::DecorationRole).value<QIcon>();

	const QRect decorationRect(
		opt.rect.x(), opt.rect.center().y() - iconSize / 2,
		iconSize, iconSize
	);

	const auto left = decorationRect.x() + decorationRect.width() +
		getMetric(QStyle::PM_FocusFrameHMargin, opt);

	const auto top = getMetric(QStyle::PM_FocusFrameVMargin, opt);
	QRect textRect(
		left, opt.rect.y() + top,
		opt.rect.width() - left, opt.rect.height()
	);

	drawBackground(painter, opt, index);
	favicon.paint(painter, decorationRect);

	painter->save();

	QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled
		? QPalette::Normal : QPalette::Disabled;
	if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
		cg = QPalette::Inactive;

	if((opt.state & QStyle::State_Selected)) {
		painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
	} else {
		painter->setPen(opt.palette.color(cg, QPalette::Text));
	}

	QFont font = opt.font;
	const QFontMetrics fm(font);

	font.setWeight(QFont::Bold);
	painter->setFont(font);
	painter->drawText(
		textRect.topLeft() + QPoint(0, fm.ascent()),
		index.data(Qt::DisplayRole).toString()
	);

	font.setWeight(QFont::Normal);
	painter->setFont(font);
	textRect.adjust(0, fm.lineSpacing(), 0, 0);
	painter->drawText(
		textRect,
		Qt::AlignLeading | Qt::AlignTop | Qt::TextWordWrap,
		index.data(ListServerModel::DescriptionRole).toString()
	);

	drawFocus(painter, opt, opt.rect);

	painter->restore();
}

QSize ListServerDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	const auto opt = setOptions(index, option);
	const auto iconSize = getMetric(QStyle::PM_LargeIconSize, opt);
	const auto focusVMargin = getMetric(QStyle::PM_FocusFrameVMargin, opt) * 2;
	const auto iconMargin = getMetric(QStyle::PM_FocusFrameHMargin, opt);
	const auto focusHMargin = iconMargin * 2;

	auto textRect = option.rect.isValid()
		? option.rect.adjusted(iconSize + iconMargin, 0, 0, 0)
		: QRect(iconSize + iconMargin, 0, (INT_MAX/256), 0);
	textRect.setHeight(INT_MAX/256);

	auto font = opt.font;
	font.setBold(true);
	const auto displaySize = QFontMetrics(font).boundingRect(
		textRect,
		0,
		index.data(Qt::DisplayRole).toString()
	);

	const auto descriptionSize = opt.fontMetrics.boundingRect(
		textRect,
		Qt::TextWordWrap,
		index.data(ListServerModel::DescriptionRole).toString()
	);

	return QSize(
		focusHMargin + iconSize + iconMargin + qMax(displaySize.width(), descriptionSize.width()),
		qMax(iconSize, focusVMargin + displaySize.height() + opt.fontMetrics.leading() + descriptionSize.height())
	);
}

}
