// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_AVATARITEMDELEGATE_H
#define LIBCLIENT_UTILS_AVATARITEMDELEGATE_H

#include <QItemDelegate>
#include <QRect>

class QPainter;
class QStyleOptionViewItem;

class AvatarItemDelegate final : public QItemDelegate {
public:
	using QItemDelegate::QItemDelegate;
protected:
	void drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect) const override
	{
		// The default Qt implementation draws an empty focus rect underneath
		// the avatar when it is selected even when there is no DisplayRole
		// data because it checks if the rect is valid instead of if it is empty
		if (rect.isEmpty())
			return;

		return QItemDelegate::drawFocus(painter, option, rect);
	}
};

#endif
