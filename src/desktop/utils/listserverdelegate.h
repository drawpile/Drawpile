// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LISTSERVERDELEGATE_H
#define LISTSERVERDELEGATE_H

#include <QItemDelegate>

namespace sessionlisting {

class ListServerDelegate final : public QItemDelegate
{
public:
	ListServerDelegate(QObject *parent = nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

};

}

#endif // LISTSERVERDELEGATE_H
