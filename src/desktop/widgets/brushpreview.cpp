// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/brushpreview.h"
#include <QEvent>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>

namespace widgets {

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent, f)
	, m_debounce(1000 / 60, this)
{
	setMinimumSize(32, 32);
	updateBackground();
	setCursor(Qt::PointingHandCursor);
	setToolTip(tr("Click to edit brush"));
	connect(
		&m_debounce, &DebounceTimer::noneChanged, this,
		&BrushPreview::triggerPreviewUpdate);
}

void BrushPreview::setPreviewShape(DP_BrushPreviewShape shape)
{
	if(m_shape != shape) {
		m_shape = shape;
		m_debounce.setNone();
	}
}

void BrushPreview::setBrush(const brushes::ActiveBrush &brush)
{
	m_brush = brush;
	m_debounce.setNone();
}

void BrushPreview::setBrushSizeLimit(int brushSizeLimit)
{
	m_brushPreview.setSizeLimit(brushSizeLimit);
	triggerPreviewUpdate();
}

void BrushPreview::clearPreset()
{
	if(m_presetEnabled) {
		m_presetEnabled = false;
		m_presetThumbnail = QPixmap();
		m_presetCache = QPixmap();
		triggerPreviewUpdate();
	}
}

void BrushPreview::setPreset(const QPixmap &thumbnail, bool changed)
{
	if(!m_presetEnabled ||
	   thumbnail.cacheKey() != m_presetThumbnail.cacheKey() ||
	   changed != m_presetChanged) {
		m_presetEnabled = true;
		m_presetThumbnail = thumbnail;
		m_presetChanged = changed;
		m_presetCache = QPixmap();
		if(m_presetEnabled) {
			update();
		} else {
			triggerPreviewUpdate();
		}
	}
}

void BrushPreview::setPresetThumbnail(const QPixmap &thumbnail)
{
	if(thumbnail.cacheKey() != m_presetThumbnail.cacheKey()) {
		m_presetThumbnail = thumbnail;
		if(m_presetEnabled) {
			m_presetCache = QPixmap();
			update();
		}
	}
}

void BrushPreview::setPresetChanged(bool changed)
{
	if(changed != m_presetChanged) {
		m_presetChanged = changed;
		if(m_presetEnabled) {
			update();
		}
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{
	m_changeIconCache = QPixmap();
	m_debounce.stopTimer();
	m_needUpdate = true;
}

void BrushPreview::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::PaletteChange) {
		updateBackground();
		m_changeIconCache = QPixmap();
	}
	triggerPreviewUpdate();
}

void BrushPreview::mouseReleaseEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton) {
		emit requestEditor();
	}
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	qreal dpr = devicePixelRatioF();
	bool dprChanged = m_lastDpr != dpr;
	if(m_needUpdate || dprChanged) {
		updatePreview(dpr);
	}
	if(m_presetEnabled && (m_presetCache.isNull() || dprChanged)) {
		updatePreset(dpr);
	}

	QPainter painter(this);
	if(m_presetEnabled) {
		painter.drawPixmap(presetRect(), m_presetCache);
		if(m_presetChanged) {
			QRect changeRect = changeIconRect();
			QSize changeSize = changeRect.size();
			if(m_changeIconCache.size() != changeSize) {
				m_changeIconCache =
					QIcon::fromTheme(QStringLiteral("drawpile_presetchanged"))
						.pixmap(changeSize);
			}
			painter.drawPixmap(changeRect, m_changeIconCache);
		}
	}
	painter.drawPixmap(previewRect(), m_brushPreview.pixmap());

	if(!isEnabled()) {
		QColor color = palette().color(QPalette::Window);
		color.setAlphaF(0.75);
		painter.fillRect(event->rect(), color);
	}
}

void BrushPreview::triggerPreviewUpdate()
{
	m_debounce.stopTimer();
	m_needUpdate = true;
	update();
}

void BrushPreview::updateBackground()
{
	int w = 16;
	bool dark = palette().color(QPalette::Window).lightness() < 128;
	m_background = QPixmap(w * 2, w * 2);
	m_background.fill(dark ? QColor(153, 153, 153) : QColor(204, 204, 204));
	QColor alt = dark ? QColor(102, 102, 102) : Qt::white;
	QPainter p(&m_background);
	p.fillRect(0, 0, w, w, alt);
	p.fillRect(w, w, w, w, alt);
}

void BrushPreview::updatePreview(qreal dpr)
{
	QSize size = previewRect().size() * dpr;
	m_brushPreview.reset(size);
	m_brush.renderPreview(m_brushPreview, m_shape);
	m_brushPreview.paint(m_background);
	m_needUpdate = false;
	m_lastDpr = dpr;
}

void BrushPreview::updatePreset(qreal dpr)
{
	QSize size = presetRect().size() * dpr;
	if(m_presetThumbnail.isNull()) {
		m_presetCache = QPixmap(size);
		m_presetCache.fill(Qt::transparent);
	} else if(m_presetThumbnail.size() == size) {
		m_presetCache = m_presetThumbnail;
	} else {
		m_presetCache = m_presetThumbnail.scaled(
			size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
}

QRect BrushPreview::previewRect() const
{
	QRect r = contentsRect();
	if(m_presetEnabled) {
		int margin = r.height() + 4;
		if(r.width() > margin) {
			return r.marginsRemoved(QMargins(margin, 0, 0, 0));
		} else {
			return QRect();
		}
	} else {
		return r;
	}
}

QRect BrushPreview::presetRect() const
{
	QRect r = contentsRect();
	int h = r.height();
	if(r.width() > h) {
		r.setWidth(h);
	}
	return r;
}

QRect BrushPreview::changeIconRect() const
{
	QRect r = presetRect();
	int minDimension = qMin(r.width(), r.height());
	int editDimension = qMax(minDimension / 4, qMin(8, minDimension));
	int editOffset = editDimension / 8;
	QSize editSize(editDimension, editDimension);
	return QRect(QPoint(r.x() + editOffset, r.y() + editOffset), editSize);
}

}
