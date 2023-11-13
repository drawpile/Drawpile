#include "desktop/docks/brushpalettedelegate.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QPainter>

namespace docks {

BrushPaletteDelegate::BrushPaletteDelegate(QObject *parent)
	: QItemDelegate(parent)
{
}

void BrushPaletteDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QItemDelegate::paint(painter, option, index);
	if(index.isValid()) {
		int id = index.data(brushes::BrushPresetModel::IdRole).toInt();
		m_lock.lockForRead();
		QHash<int, QPixmap>::const_iterator it = m_cache.find(id);
		QPixmap pixmap;
		if(it == m_cache.constEnd()) {
			m_lock.unlock();
			m_lock.lockForWrite();
			pixmap = index.data(brushes::BrushPresetModel::ThumbnailRole)
						 .value<QPixmap>();
			m_cache.insert(id, pixmap);
		} else {
			pixmap = it.value();
		}
		m_lock.unlock();

		QStyleOptionViewItem opt = setOptions(index, option);
		drawDecoration(
			painter, option, opt.rect.marginsRemoved(QMargins(4, 4, 4, 4)),
			pixmap);
	}
}

QSize BrushPaletteDelegate::sizeHint(
	const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);
	return index.data(Qt::SizeHintRole).toSize() + QSize(8, 8);
}

void BrushPaletteDelegate::clearCache()
{
	m_lock.lockForWrite();
	m_cache.clear();
	m_lock.unlock();
}

}
