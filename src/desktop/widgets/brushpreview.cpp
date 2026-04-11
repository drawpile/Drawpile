// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/brushpreview.h"
#include "desktop/main.h"
#include "libclient/config/config.h"
#include <QEvent>
#include <QFontMetrics>
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
	setCursor(Qt::PointingHandCursor);
	setToolTip(tr("Click to edit brush"));
	connect(
		&m_debounce, &DebounceTimer::noneChanged, this,
		&BrushPreview::triggerPreviewUpdate);

	config::Config *cfg = dpAppConfig();
	CFG_BIND_NOTIFY(
		cfg, CheckerColor1, this, BrushPreview::setBackgroundChanged);
	CFG_BIND_NOTIFY(
		cfg, CheckerColor2, this, BrushPreview::setBackgroundChanged);
	m_debounce.stopTimer();
}

void BrushPreview::setPreviewStyle(DP_BrushPreviewStyle style)
{
	if(m_style != style) {
		m_style = style;
		triggerPreviewUpdate();
	}
}

void BrushPreview::setPreviewShape(DP_BrushPreviewShape shape)
{
	if(m_shape != shape) {
		m_shape = shape;
		m_debounce.setNone();
	}
}

void BrushPreview::setShowTitle(bool showTitle)
{
	if(showTitle != m_showTitle) {
		m_showTitle = showTitle;
		if(m_presetEnabled && !m_presetTitle.isEmpty()) {
			triggerPreviewUpdate();
		}
	}
}

void BrushPreview::setShowThumbnail(bool showThumbnail)
{
	if(showThumbnail != m_showThumbnail) {
		m_showThumbnail = showThumbnail;
		if(m_presetEnabled) {
			triggerPreviewUpdate();
		}
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
		m_presetTitle = QString();
		m_presetThumbnail = QPixmap();
		m_presetCache = QPixmap();
		triggerPreviewUpdate();
	}
}

void BrushPreview::setPreset(
	const QString &title, const QPixmap &thumbnail, bool changed)
{
	if(!m_presetEnabled || m_presetTitle != title ||
	   thumbnail.cacheKey() != m_presetThumbnail.cacheKey() ||
	   changed != m_presetChanged) {
		m_presetEnabled = true;
		m_presetTitle = title;
		m_presetThumbnail = thumbnail;
		m_presetChanged = changed;
		m_presetCache = QPixmap();
		m_needTextBounds = true;
		if(m_presetEnabled && (m_showTitle || m_showThumbnail)) {
			update();
		} else {
			triggerPreviewUpdate();
		}
	}
}

void BrushPreview::setPresetTitle(const QString &title)
{
	if(title != m_presetTitle) {
		m_presetTitle = title;
		m_needTextBounds = true;
		if(m_presetEnabled && m_showTitle) {
			m_presetCache = QPixmap();
			update();
		}
	}
}

void BrushPreview::setPresetThumbnail(const QPixmap &thumbnail)
{
	if(thumbnail.cacheKey() != m_presetThumbnail.cacheKey()) {
		m_presetThumbnail = thumbnail;
		if(m_presetEnabled && m_showThumbnail) {
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

bool BrushPreview::event(QEvent *event)
{
	switch(event->type()) {
	case QEvent::ApplicationPaletteChange:
	case QEvent::PaletteChange:
		m_needPalette = true;
		triggerPreviewUpdate();
		break;
	default:
		break;
	}
	return QFrame::event(event);
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
	QPalette pal = palette();
	if(m_needPalette) {
		m_brushPreview.setPalette(
			pal.color(QPalette::Text), pal.color(QPalette::Base),
			pal.color(QPalette::Highlight));
	}

	qreal dpr = devicePixelRatioF();
	bool dprChanged = m_lastDpr != dpr;
	if(m_needUpdate || dprChanged) {
		updatePreview(pal, dpr);
	}
	if(m_presetEnabled && (m_presetCache.isNull() || dprChanged)) {
		updatePreset(dpr);
	}

	QPainter painter(this);
	if(m_presetEnabled && m_showThumbnail) {
		painter.drawPixmap(presetRect(), m_presetCache);
	}

	QRect pr = previewRect();
	painter.drawPixmap(pr, m_brushPreview.pixmap());

	if(m_presetEnabled && m_showTitle && !m_presetTitle.isEmpty()) {
		if(m_needTextBounds) {
			m_textBounds = painter.fontMetrics().boundingRect(m_presetTitle);
		}

		QRect textRect = m_textBounds.marginsAdded(QMargins(4, 1, 4, 1));
		textRect.moveBottomRight(pr.bottomRight());
		painter.setOpacity(0.9);
		painter.fillRect(textRect, pal.base());
		painter.setOpacity(1.0);
		painter.setPen(QPen(pal.text(), 1.0));
		painter.drawText(
			textRect, Qt::AlignCenter | Qt::TextDontClip, m_presetTitle);
	}

	if(m_presetEnabled && m_presetChanged) {
		QRect changeRect = changeIconRect();
		QSize changeSize = changeRect.size();
		if(m_changeIconCache.size() != changeSize) {
			m_changeIconCache =
				QIcon::fromTheme(QStringLiteral("drawpile_presetchanged"))
					.pixmap(changeSize);
		}
		painter.drawPixmap(changeRect, m_changeIconCache);
	}

	if(!isEnabled()) {
		painter.setOpacity(0.75);
		painter.fillRect(event->rect(), pal.window());
	}
}

void BrushPreview::setBackgroundChanged()
{
	m_debounce.noneChanged();
}

void BrushPreview::triggerPreviewUpdate()
{
	m_debounce.stopTimer();
	m_needUpdate = true;
	update();
}

void BrushPreview::updatePreview(const QPalette &pal, qreal dpr)
{
	QSize size = previewRect().size() * dpr;
	m_brushPreview.reset(size);
	m_brush.renderPreview(m_brushPreview, m_style, m_shape);
	m_brushPreview.paint(getPreviewBackground(pal, dpr));
	m_needUpdate = false;
	m_lastDpr = dpr;
}

const QPixmap &
BrushPreview::getPreviewBackground(const QPalette &pal, qreal dpr)
{
	if(m_style == DP_BRUSH_PREVIEW_STYLE_PLAIN) {
		return m_strokeBackground.getPixmapPlain(pal, dpr);
	} else {
		return m_strokeBackground.getPixmapConfig(dpAppConfig(), dpr);
	}
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
	if(m_presetEnabled && m_showThumbnail) {
		int margin = r.height();
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
	QRect r;
	if(m_showThumbnail) {
		r = presetRect();
	} else {
		r = previewRect();
	}

	int minDimension = qMin(r.width(), r.height());
	int editDimension = qMax(minDimension / 4, qMin(8, minDimension));
	int editOffset = editDimension / 8;
	QSize editSize(editDimension, editDimension);
	return QRect(QPoint(r.x() + editOffset, r.y() + editOffset), editSize);
}

}
