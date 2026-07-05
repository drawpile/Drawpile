// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_BRUSHPREVIEW_H
#define DESKTOP_WIDGETS_BRUSHPREVIEW_H
#include "libclient/brushes/brush.h"
#include "libclient/drawdance/brushpreview.h"
#include "libclient/utils/checkerbackground.h"
#include "libclient/utils/debouncetimer.h"
#include <QFrame>

namespace widgets {

class BrushPreview final : public QFrame {
	Q_OBJECT
public:
	explicit BrushPreview(
		QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	DP_BrushPreviewStyle previewStyle() const { return m_style; }
	void setPreviewStyle(DP_BrushPreviewStyle style);

	DP_BrushPreviewShape previewShape() const { return m_shape; }
	void setPreviewShape(DP_BrushPreviewShape shape);

	void setShowTitle(bool showTitle);
	void setShowThumbnail(bool showThumbnail);

	const brushes::ActiveBrush &brush() const { return m_brush; }
	void setBrush(const brushes::ActiveBrush &brush);
	void setBrushSizeLimit(int brushSizeLimit);

	void clearPreset();
	void setPreset(
		int state, const QString &title, const QPixmap &thumbnail,
		bool changed);
	void setPresetState(int state);
	void setPresetTitle(const QString &title);
	void setPresetThumbnail(const QPixmap &thumbnail);
	void setPresetChanged(bool changed);

signals:
	void requestEditor();

protected:
	bool event(QEvent *event) override;
	void resizeEvent(QResizeEvent *) override;
	void changeEvent(QEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void setBackgroundChanged();
	void triggerPreviewUpdate();

	void updatePreview(const QPalette &pal, qreal dpr);
	const QPixmap &getPreviewBackground(const QPalette &pal, qreal dpr);
	void updatePreset(qreal dpr);

	QRect previewRect() const;
	QRect presetRect() const;
	void iconRects(QRect &outChangeRect, QRect &outStateRect) const;
	const QPixmap &changeIconPixmap(const QSize &size);
	const QPixmap &stateIconPixmap(const QSize &size);
	static QPixmap &getIconPixmap(
		QPixmap &inOutPixmap, const QSize &size, const QString &iconName);

	CheckerBackground m_strokeBackground;
	brushes::ActiveBrush m_brush;
	drawdance::BrushPreview m_brushPreview;
	qreal m_lastDpr = 1.0;
	QString m_presetTitle;
	QPixmap m_presetThumbnail;
	QPixmap m_presetCache;
	QPixmap m_changeIconCache;
	QPixmap m_deletedIconCache;
	QRect m_textBounds;
	DP_BrushPreviewStyle m_style = DP_BRUSH_PREVIEW_STYLE_PLAIN;
	DP_BrushPreviewShape m_shape = DP_BRUSH_PREVIEW_STROKE;
	int m_presetState;
	bool m_showTitle = true;
	bool m_showThumbnail = false;
	bool m_presetChanged = false;
	bool m_presetEnabled = false;
	bool m_needPalette = true;
	bool m_needTextBounds = true;
	bool m_needUpdate = false;
	DebounceTimer m_debounce;
};

}

#endif
