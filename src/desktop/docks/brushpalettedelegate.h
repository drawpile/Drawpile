// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_BRUSHPALLETEDELEGATE_H
#define DESKTOP_DOCKS_BRUSHPALLETEDELEGATE_H
#include "libclient/drawdance/brushpreview.h"
#include "libclient/utils/checkerbackground.h"
#include <QHash>
#include <QItemDelegate>
#include <QPair>
#include <QPixmap>
#include <QReadWriteLock>

class QFontMetrics;
class QPalette;

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

	void clearCache();
	void handleDataChanged(
		const QModelIndex &topLeft, const QModelIndex &bottomRight,
		const QVector<int> &roles);

	int display() const { return m_display; }
	void setDisplay(int display);

	int columnCount() const { return m_columnCount; }
	void setColumnCount(int columnCount);

private:
	static constexpr int MARGIN = 4;
	static constexpr int PADDING = MARGIN * 2;

	struct Preview {
		QPixmap thumbnail;
		QPixmap stroke;
		QSize size;
		QRect textBounds;
		QString title;
	};

	bool haveThumbnail() const;
	bool haveStroke() const;

	Preview renderPreview(
		const QModelIndex &index, const QPalette &pal,
		const QFontMetrics &fontMetrics, int w, int h, qreal dpr) const;

	const QPixmap &getEditIcon(const QSize &size) const;

	static int calculateStrokeColumnCount(int width, int height, int multiplier)
	{
		return width / ((height + PADDING) * multiplier);
	}

	int m_display;
	int m_columnCount = 0;
	mutable QHash<int, Preview> m_cache;
	mutable QReadWriteLock m_lock;
	mutable QPixmap m_editIcon;
	mutable drawdance::BrushPreview m_brushPreview;
	mutable CheckerBackground m_strokeBackground;
};

}

#endif
