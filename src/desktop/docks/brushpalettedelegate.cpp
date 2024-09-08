#include "desktop/docks/brushpalettedelegate.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QIcon>
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
		qreal dpr = painter->device()->devicePixelRatioF();
		QPair<int, qreal> key(id, dpr);
		m_lock.lockForRead();
		QHash<QPair<int, qreal>, QPixmap>::const_iterator it =
			m_cache.find(key);
		QPixmap pixmap;
		if(it == m_cache.constEnd()) {
			m_lock.unlock();
			m_lock.lockForWrite();
			pixmap =
				index.data(brushes::BrushPresetModel::EffectiveThumbnailRole)
					.value<QPixmap>();
			if(!pixmap.isNull()) {
				pixmap = pixmap.scaled(
					index.data(Qt::SizeHintRole).toSize() * dpr,
					Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			}
			m_cache.insert(key, pixmap);
		} else {
			pixmap = it.value();
		}
		m_lock.unlock();

		QStyleOptionViewItem opt = setOptions(index, option);
		QRect rect = opt.rect.marginsRemoved(QMargins(4, 4, 4, 4));
		if(!pixmap.isNull() && rect.isValid()) {
			if(option.state & QStyle::State_Selected) {
				pixmap = selectedPixmap(
					pixmap, option.palette,
					option.state & QStyle::State_Enabled);
			}
			painter->drawPixmap(rect, pixmap);
		}

		if(index.data(brushes::BrushPresetModel::HasChangesRole).toBool()) {
			int minDimension = qMin(rect.width(), rect.height());
			int editDimension = qMax(minDimension / 4, qMin(8, minDimension));
			int editOffset = editDimension / 8;
			QSize editSize(editDimension, editDimension);
			QRect editRect = QRect(
				QPoint(rect.x() + editOffset, rect.y() + editOffset), editSize);
			painter->drawPixmap(editRect, getEditIcon(editSize));
		}
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

const QPixmap &BrushPaletteDelegate::getEditIcon(const QSize &size) const
{
	if(m_editIcon.size() != size) {
		m_editIcon = QIcon::fromTheme(QStringLiteral("drawpile_presetchanged"))
						 .pixmap(size);
	}
	return m_editIcon;
}

}
