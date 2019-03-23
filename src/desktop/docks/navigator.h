/*
   Copyright (C) 2008-2019 Calle Laakkonen, 2007 M.K.A.

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef Navigator_H
#define Navigator_H

#include <QDockWidget>

class Ui_Navigator;

namespace paintcore {
	class LayerStackPixmapCacheObserver;
}

namespace drawingboard {
	class CanvasScene;
}

namespace canvas {
	class UserCursorModel;
}

namespace docks {

class NavigatorView : public QWidget
{
	Q_OBJECT
public:
	NavigatorView(QWidget *parent);

	void setLayerStackObserver(paintcore::LayerStackPixmapCacheObserver *observer);
	void setUserCursors(canvas::UserCursorModel *cursors) { m_cursors = cursors; }

	void setViewFocus(const QPolygonF& rect);

public slots:
	void setShowCursors(bool showCursors);

signals:
	void focusMoved(const QPoint& to);
	void wheelZoom(int steps);
	
protected:
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);

private slots:
	void onChange();
	void refreshCache();

private:
	paintcore::LayerStackPixmapCacheObserver *m_observer;
	canvas::UserCursorModel *m_cursors;
	QPixmap m_cache;
	QPixmap m_cursorBackground;
	QSize m_cachedSize;

	QTimer *m_refreshTimer;

	QPolygonF m_focusRect;
	int m_zoomWheelDelta;
	bool m_showCursors;
};

//! Navigator dock widget
class Navigator : public QDockWidget
{
	Q_OBJECT
public:
	Navigator(QWidget *parent);
	~Navigator();

	//! Set associated graphics scene
	void setScene(drawingboard::CanvasScene *scene);

	//! Set the user list (optional)
	void setUserCursors(canvas::UserCursorModel *cursors);

	// Set the actions for the buttons
	void setFlipActions(QAction *flip, QAction *mirror);

public slots:
	//! Move the view focus rectangle
	void setViewFocus(const QPolygonF& rect);

	//! Set the current angle and zoom
	void setViewTransformation(qreal zoom, qreal angle);

private slots:
	void updateZoom(int value);
	void updateAngle(int value);

signals:
	void focusMoved(const QPoint& to);
	void wheelZoom(int steps);
	void angleChanged(qreal newAngle);
	void zoomChanged(qreal newZoom);
	
private:
	Ui_Navigator *m_ui;
	bool m_updating;
};

}

#endif // Navigator_H
