// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef Navigator_H
#define Navigator_H

#include <QDockWidget>

class QSlider;
class QToolButton;

namespace canvas {
	class CanvasModel;
}

namespace docks {

class NavigatorView final : public QWidget
{
	Q_OBJECT
public:
	NavigatorView(QWidget *parent);

	void setCanvasModel(canvas::CanvasModel *model);
	void setViewFocus(const QPolygonF& rect);

public slots:
	void setShowCursors(bool showCursors);
	void setRealtimeUpdate(bool realtime);

signals:
	void focusMoved(const QPoint& to);
	void wheelZoom(int steps);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private slots:
	void onChange();
	void onResize();
	void refreshCache();
	void onCursorMove(uint8_t user, uint16_t layer, int x, int y);

private:
	struct UserCursor {
		QPixmap avatar;
		QPoint pos;
		qint64 lastMoved;
		int id;
	};

	canvas::CanvasModel *m_model;
	QVector<UserCursor> m_cursors;
	QPixmap m_cache;
	QPixmap m_cursorBackground;
	QSize m_cachedSize;

	QTimer *m_refreshTimer;

	QPolygonF m_focusRect;
	int m_zoomWheelDelta;
	bool m_showCursors;
};

//! Navigator dock widget
class Navigator final : public QDockWidget
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

	void setMinimumZoom(int zoom);

private slots:
	void updateZoom(int value);

signals:
	void focusMoved(const QPoint& to);
	void wheelZoom(int steps);
	void zoomChanged(qreal newZoom);

private:
	QSlider *m_zoomSlider;
	QToolButton *m_resetZoomButton;
	NavigatorView *m_view;
	bool m_updating;
};

}

#endif // Navigator_H
