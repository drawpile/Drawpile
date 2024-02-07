// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_BRUSHPALLETEDELEGATE_H
#define DESKTOP_DOCKS_BRUSHPALLETEDELEGATE_H
#include <QHash>
#include <QItemDelegate>
#include <QPair>
#include <QPixmap>
#include <QReadWriteLock>

namespace docks {

class BrushPaletteDelegate final : public QItemDelegate {
	Q_OBJECT
public:
	explicit BrushPaletteDelegate(QObject *parent = nullptr);

	void paint(
		QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index)
		const override;

public slots:
	void clearCache();

private:
	mutable QHash<QPair<int, qreal>, QPixmap> m_cache;
	mutable QReadWriteLock m_lock;
};

}

#endif
