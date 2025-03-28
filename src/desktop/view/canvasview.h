// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASVIEW_H
#define DESKTOP_VIEW_CANVASVIEW_H
#include <QAbstractScrollArea>

class QColor;
class QImage;
class QUrl;

namespace widgets {
class NotificationBar;
}

namespace view {

class CanvasController;
class CanvasInterface;

class CanvasView final : public QAbstractScrollArea {
	Q_OBJECT
public:
	CanvasView(
		CanvasController *controller, CanvasInterface *canvasWidget,
		QWidget *parent = nullptr);

	void scrollStepLeft();
	void scrollStepRight();
	void scrollStepUp();
	void scrollStepDown();

	void showDisconnectedWarning(const QString &message, bool singleSession);
	void hideDisconnectedWarning();
	void showResetNotice(bool saveInProgress);
	void hideResetNotice();

signals:
	void colorDropped(const QColor &color);
	void imageDropped(const QImage &image);
	void urlDropped(const QUrl &url);
	void reconnectRequested();
	void savePreResetStateRequested();
	void savePreResetStateDismissed();

protected:
	void focusInEvent(QFocusEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	bool viewportEvent(QEvent *event) override;

	void scrollContentsBy(int dx, int dy) override;

private:
	enum class NotificationBarState { None, Reconnect, Reset };

	void setEnableScrollBars(bool enableScrollBars);
	void setTouchUseGestureEvents(bool touchUseGestureEvents);

	void handleDragDrop(QDropEvent *event, bool drop);

	void onControllerScrollAreaChanged(
		int minH, int maxH, int valueH, int pageStepH, int singleStepH,
		int minV, int maxV, int valueV, int pageStepV, int singleStepV);

	void activateNotificationBarAction();
	void dismissNotificationBar();
	void autoDismissNotificationBar();
	void setNotificationBarSaveInProgress(bool saveInProgress);

	CanvasController *m_controller;
	CanvasInterface *m_canvasWidget;
	widgets::NotificationBar *m_notificationBar;
	NotificationBarState m_notificationBarState = NotificationBarState::None;
	bool m_blockScrolling = false;
	bool m_touchUseGestureEvents = false;
};

}

#endif
