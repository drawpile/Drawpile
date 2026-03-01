#include "desktop/docks/brushpalettedelegate.h"
#include "desktop/docks/brushpalettedock.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>

namespace docks {

BrushPaletteDelegate::BrushPaletteDelegate(QObject *parent)
	: QItemDelegate(parent)
	, m_display(int(BrushPalette::Display::Thumbnails))
{
}

void BrushPaletteDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QItemDelegate::paint(painter, option, index);
	if(!index.isValid()) {
		return;
	}

	QStyleOptionViewItem opt = setOptions(index, option);
	bool selected = option.state & QStyle::State_Selected;
	bool enabled = option.state & QStyle::State_Enabled;

	painter->setPen(Qt::NoBrush);
	if(!selected && haveStroke()) {
		painter->setPen(QPen(opt.palette.window(), 1.0));
		painter->drawRect(opt.rect.marginsRemoved(QMargins(0, 0, 1, 1)));
	}

	QRect rect = opt.rect.marginsRemoved(QMargins(4, 4, 4, 4));
	if(rect.isEmpty()) {
		return;
	}

	painter->setPen(Qt::NoPen);
	qreal dpr = painter->device()->devicePixelRatioF();
	QSize previewSize(qRound(rect.width() * dpr), qRound(rect.height() * dpr));

	int id = index.data(brushes::BrushPresetModel::IdRole).toInt();
	m_lock.lockForRead();
	QHash<int, Preview>::const_iterator it = m_cache.find(id);
	Preview preview;
	if(it == m_cache.constEnd() || it->size != previewSize) {
		m_lock.unlock();
		m_lock.lockForWrite();
		preview = renderPreview(
			index, opt.palette, opt.fontMetrics, previewSize.width(),
			previewSize.height());
		preview.size = previewSize;
		m_cache.insert(id, preview);
	} else {
		preview = *it;
	}
	m_lock.unlock();

	int strokeLeft;
	if(haveThumbnail()) {
		strokeLeft = rect.x() + rect.height();
		if(!preview.thumbnail.isNull()) {
			if(selected) {
				preview.thumbnail =
					selectedPixmap(preview.thumbnail, option.palette, enabled);
			}

			int minDimension = qMin(rect.width(), rect.height());
			int thumbnailOffsetX;
			if(haveStroke()) {
				thumbnailOffsetX = 0;
			} else {
				thumbnailOffsetX = (rect.width() - minDimension) / 2;
			}

			painter->drawPixmap(
				QRect(
					rect.x() + thumbnailOffsetX, rect.y(), minDimension,
					minDimension),
				preview.thumbnail);
		}
	} else {
		strokeLeft = rect.x();
	}

	if(haveStroke()) {
		QRect strokeRect = rect;
		strokeRect.setLeft(strokeLeft);
		if(!strokeRect.isEmpty() && !preview.stroke.isNull()) {
			if(selected) {
				preview.stroke =
					selectedPixmap(preview.stroke, option.palette, enabled);
			}
			painter->drawPixmap(strokeRect, preview.stroke);
		}

		if(!preview.title.isEmpty()) {
			QRect textRect =
				preview.textBounds.marginsAdded(QMargins(4, 1, 4, 1));
			textRect.moveBottomRight(rect.bottomRight());
			painter->setOpacity(0.7);

			// This is the same logic used in QItemDelegate::selectedPixmap.
			QColor fillColor =
				opt.palette.color(QPalette::Normal, QPalette::Base);
			if(selected) {
				QColor highlightColor = opt.palette.color(
					enabled ? QPalette::Normal : QPalette::Disabled,
					QPalette::Highlight);
				fillColor.setRedF(
					fillColor.redF() * 0.7 + highlightColor.redF() * 0.3);
				fillColor.setGreenF(
					fillColor.greenF() * 0.7 + highlightColor.greenF() * 0.3);
				fillColor.setBlueF(
					fillColor.blueF() * 0.7 + highlightColor.blueF() * 0.3);
			}
			painter->fillRect(textRect.intersected(rect), fillColor);

			painter->setOpacity(1.0);
			painter->setPen(QPen(
				selected ? opt.palette.highlightedText() : opt.palette.text(),
				1.0));
			painter->setClipRect(rect);
			painter->drawText(
				textRect, Qt::AlignCenter | Qt::TextDontClip, preview.title);
			painter->setClipping(false);
		}
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

QSize BrushPaletteDelegate::sizeHint(
	const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);
	constexpr int PADDING = 8;
	int width = option.rect.width();
	QSize size = index.data(Qt::SizeHintRole).toSize();
	if(haveStroke()) {
		int multiplier = haveThumbnail() ? 3 : 2;
		int columns = width / ((size.height() + PADDING) * multiplier);
		if(columns > 1) {
			return QSize(width / columns - 1, size.height() + PADDING);
		} else {
			return QSize(width, size.height() + PADDING);
		}
	} else {
		int columns = width / (size.width() + PADDING + 1);
		if(columns > 1) {
			return QSize(width / columns - 1, size.height() + PADDING);
		} else {
			return QSize(size.width(), size.height() + PADDING);
		}
	}
}

void BrushPaletteDelegate::clearCache()
{
	m_lock.lockForWrite();
	m_cache.clear();
	m_lock.unlock();
}

void BrushPaletteDelegate::handleDataChanged(
	const QModelIndex &topLeft, const QModelIndex &bottomRight,
	const QVector<int> &roles)
{
	Q_UNUSED(roles);
	if(topLeft.isValid() && bottomRight.isValid()) {
		int firstRow = topLeft.row();
		int lastRow = bottomRight.row();
		const QAbstractItemModel *model = topLeft.model();
		if(firstRow <= lastRow && model) {
			m_lock.lockForWrite();
			for(int row = firstRow; row <= lastRow; ++row) {
				int id = model->index(row, 0)
							 .data(brushes::BrushPresetModel::IdRole)
							 .toInt();
				m_cache.remove(id);
			}
			m_lock.unlock();
		}
	}
}

void BrushPaletteDelegate::setDisplay(int display)
{
	if(display != m_display) {
		m_display = display;
		m_cache.clear();
	}
}

bool BrushPaletteDelegate::haveThumbnail() const
{
	switch(m_display) {
	case int(BrushPalette::Display::Strokes):
		return false;
	default:
		return true;
	}
}

bool BrushPaletteDelegate::haveStroke() const
{
	switch(m_display) {
	case int(BrushPalette::Display::Both):
	case int(BrushPalette::Display::Strokes):
		return true;
	default:
		return false;
	}
}

BrushPaletteDelegate::Preview BrushPaletteDelegate::renderPreview(
	const QModelIndex &index, const QPalette &pal,
	const QFontMetrics &fontMetrics, int w, int h) const
{
	Preview preview;
	int minDimension = qMin(w, h);
	int strokeWidth;

	if(haveThumbnail()) {
		strokeWidth = w - h;
		QPixmap thumbnail =
			index.data(brushes::BrushPresetModel::EffectiveThumbnailRole)
				.value<QPixmap>();
		if(!thumbnail.isNull()) {
			QSize thumbnailSize(minDimension, minDimension);
			if(thumbnail.size() == thumbnailSize) {
				preview.thumbnail = thumbnail;
			} else {
				preview.thumbnail = thumbnail.scaled(
					thumbnailSize, Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation);
			}
		}
	} else {
		strokeWidth = w;
	}

	if(haveStroke() && strokeWidth > 0) {
		brushes::Preset preset =
			index.data(brushes::BrushPresetModel::PresetRole)
				.value<brushes::Preset>();

		QRect strokeRect(0, 0, w - h, h);
		m_brushPreview.reset(QSize(w - h, h));
		m_brushPreview.setPalette(
			pal.color(QPalette::Text), pal.color(QPalette::Base));
		preset.effectiveBrush().renderPreview(
			m_brushPreview, DP_BRUSH_PREVIEW_STYLE_PLAIN,
			DP_BRUSH_PREVIEW_STROKE);
		m_brushPreview.paint();
		preview.stroke = m_brushPreview.pixmap();

		preview.title = preset.effectivePreviewTitle();
		if(!preview.title.isEmpty()) {
			preview.textBounds = fontMetrics.boundingRect(preview.title);
		}
	}

	return preview;
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
