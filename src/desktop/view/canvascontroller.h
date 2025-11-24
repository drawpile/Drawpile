// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASCONTROLLER_H
#define DESKTOP_VIEW_CANVASCONTROLLER_H
#include "desktop/utils/tabletfilter.h"
#include "desktop/view/lock.h"
#include "libclient/view/canvascontrollerbase.h"
#include "libclient/view/hudaction.h"

class QGestureEvent;
class QWidget;

namespace view {

class CanvasInterface;
class CanvasScene;
class WidgetTouchHandler;

class CanvasController final : public CanvasControllerBase {
	Q_OBJECT
public:
	explicit CanvasController(CanvasScene *scene, QObject *parent = nullptr);

	CanvasScene *scene() const { return m_scene; }

	void setCanvasWidget(CanvasInterface *canvasWidget)
	{
		m_canvasWidget = canvasWidget;
	}

	void handleGesture(QGestureEvent *event);

	void setLockState(
		QFlags<view::Lock::Reason> reasons, const QStringList &descriptions,
		const QVector<QAction *> &actions);

protected:
	TouchHandler *touchHandler() const override;

	void resetTabletFilter() override { m_tabletFilter.reset(); }

	bool shouldFilterTabletEvent(const QTabletEvent *event) override
	{
		return m_tabletFilter.shouldIgnore(event);
	}

	void setCursorOnCanvas(bool cursorOnCanvas) override;
	void setCursorPos(const QPointF &posf) override;
	void setViewportCursor(const QCursor &cursor) override;

	void showColorPick(int source, const QPointF &posf) override;
	void hideColorPick() override;

	HudAction checkHudHover(const QPointF &point) override;
	void activateHudAction(
		const HudAction &action, const QPoint &globalPos) override;
	void removeHudHover() override;

	void updateSceneTransform(const QTransform &transform) override;
	void handleViewSizeChange() override;

	bool showHudTransformNotice(const QString &text) override;
	bool showHudLockStatus(
		const QString &description, const QVector<QAction *> &actions) override;
	bool hideHudLockStatus() override;

	QPoint mapPointFromGlobal(const QPoint &point) const override;
	QSize viewSize() const override;
	qreal devicePixelRatioF() const override;
	QPointF viewToCanvasOffset() const override;
	QPointF viewTransformOffset() const override;
	QPointF viewCursorPosOrCenter() const override;

private:
	CanvasScene *m_scene;
	CanvasInterface *m_canvasWidget = nullptr;
	WidgetTouchHandler *m_touchHandler;
	QCursor m_currentCursor;
	TabletFilter m_tabletFilter;
};

}

#endif
