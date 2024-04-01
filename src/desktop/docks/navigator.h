// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef Navigator_H
#define Navigator_H

#include "desktop/docks/dockbase.h"

class KisDoubleSliderSpinBox;
class QMenu;

namespace canvas {
class CanvasModel;
class PaintEngine;
}

namespace widgets{
class GroupedToolButton;
}

namespace docks {

class NavigatorView final : public QWidget
{
	Q_OBJECT
public:
	NavigatorView(QWidget *parent);

	void setCanvasModel(canvas::CanvasModel *model);
	void setViewFocus(const QPolygonF& rect);
	void setShowCursors(bool showCursors);
	void setRealtimeUpdate(bool realtime);
	void setBackgroundColor(const QColor &backgroundColor);
	void setCheckerColor1(const QColor &checkerColor1);
	void setCheckerColor2(const QColor &checkerColor2);

signals:
	void focusMoved(const QPointF& to);
	void wheelZoom(int steps);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
	void onChange(const QRect &rect = QRect{});
	void onTileCacheDirtyCheckNeeded();
	void onResize();
	void refreshCache();
	void onCursorMove(unsigned int flags, int user, int layer, int x, int y);

private:
	static constexpr int CHECKER_SIZE = 32;

	struct UserCursor {
		QPixmap avatar;
		QPoint pos;
		qint64 lastMoved;
		int id;
	};

	QPointF getFocusPoint(const QPointF &eventPoint);
	void refreshFromPixmap(canvas::PaintEngine *pe);
	void refreshFromTileCache(canvas::PaintEngine *pe);
	bool refreshCacheSize(const QSize &canvasSize);
	void getRefreshArea(
		const QSize &canvasSize, QRectF &outSourceArea, QRectF &outTargetArea);

	canvas::CanvasModel *m_model;
	QVector<UserCursor> m_cursors;
	QColor m_backgroundColor = QColor(100, 100, 100);
	QPixmap m_checker = QPixmap(CHECKER_SIZE, CHECKER_SIZE);
	QPixmap m_cache;
	QPixmap m_cursorBackground;
	QSize m_cachedSize;
	QSize m_canvasSize;
	QTimer *m_refreshTimer;
	QRect m_refreshArea;
	QPolygonF m_focusRect;
	int m_zoomWheelDelta;
	bool m_useTileCache = false;
	bool m_refreshAll = false;
	bool m_tileCacheDirtyCheckNeeded = false;
	bool m_showCursors;
};

//! Navigator dock widget
class Navigator final : public DockBase
{
	Q_OBJECT
public:
	Navigator(QWidget *parent);
	~Navigator() override;

	void setCanvasModel(canvas::CanvasModel *model);

public slots:
	//! Move the view focus rectangle
	void setViewFocus(const QPolygonF& rect);

	//! Set the current angle and zoom
	void setViewTransformation(qreal zoom, qreal angle);

private slots:
	void updateZoom(double value);

signals:
	void focusMoved(const QPointF& to);
	void wheelZoom(int steps);
	void zoomChanged(qreal newZoom);

private:
	KisDoubleSliderSpinBox *m_zoomSlider;
	widgets::GroupedToolButton *m_resetZoomButton;
	NavigatorView *m_view;
	bool m_updating;
};

}

#endif // Navigator_H
