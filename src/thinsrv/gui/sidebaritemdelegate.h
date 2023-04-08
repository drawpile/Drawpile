// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIDEBARITEMDELEGATE_H
#define SIDEBARITEMDELEGATE_H

#include <QItemDelegate>

namespace server {
namespace gui {

class SidebarItemDelegate final : public QItemDelegate
{
	Q_OBJECT
public:
	explicit SidebarItemDelegate(QObject *parent=nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override;
};

}
}

#endif
