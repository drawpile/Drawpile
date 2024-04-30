// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_VIEWWRAPPER_H
#define DESKTOP_VIEW_VIEWWRAPPER_H
#include "desktop/view/canvaswrapper.h"
#include <QObject>

namespace view {

class CanvasController;
class CanvasInterface;
class CanvasScene;
class CanvasView;

class ViewWrapper final : public QObject, public CanvasWrapper {
	Q_OBJECT
public:
	explicit ViewWrapper(bool useOpenGl, QWidget *parent = nullptr);

	QAbstractScrollArea *viewWidget() const override;

	bool isTabletEnabled() const override;
	bool isTouchScrollEnabled() const override;
	bool isTouchDrawEnabled() const override;
	QString pressureCurveAsString() const override;

	QPoint viewCenterPoint() const override;
	bool isPointVisible(const QPointF &point) const override;
	QRectF screenRect() const override;

	canvas::CanvasModel *canvas() const override;
	void setCanvas(canvas::CanvasModel *canvas) override;

	drawingboard::AnnotationItem *getAnnotationItem(int annotationId) override;

	void clearKeys() override;

#ifdef __EMSCRIPTEN__
	void setEnableEraserOverride(bool enableEraserOverride) override;
#endif

	void setShowAnnotations(bool showAnnotations) override;
	void setShowAnnotationBorders(bool showAnnotationBorders) override;
	void setShowLaserTrails(bool showLaserTrails) override;
	void setShowOwnUserMarker(bool showOwnUserMarker) override;
	void setPointerTracking(bool pointerTracking) override;
	void setShowToggleItems(bool showToggleItems) override;

	void setCatchupProgress(int percent, bool force) override;
	void setSaveInProgress(bool saveInProgress) override;

	void showDisconnectedWarning(
		const QString &message, bool singleSession) override;
	void hideDisconnectedWarning() override;
	void showResetNotice(bool compatibilityMode, bool saveInProgress) override;
	void hideResetNotice() override;

	void disposeScene() override;

	void connectActions(const Actions &actions) override;

	void connectCanvasFrame(widgets::CanvasFrame *canvasFrame) override;

	void connectDocument(Document *doc) override;

	void connectLock(view::Lock *lock) override;

	void connectLoginDialog(Document *doc, dialogs::LoginDialog *dlg) override;

	void connectMainWindow(MainWindow *mainWindow) override;

	void connectNavigator(docks::Navigator *navigator) override;

	void connectToolSettings(docks::ToolSettings *toolSettings) override;

	void connectViewStatus(widgets::ViewStatus *viewStatus) override;

	void connectViewStatusBar(widgets::ViewStatusBar *viewStatusBar) override;

private:
	static CanvasInterface *instantiateView(
		bool useOpenGl, CanvasController *controller, QWidget *parent);

	CanvasScene *m_scene;
	CanvasController *m_controller;
	CanvasInterface *m_canvasWidget;
	CanvasView *m_view;
};

}

#endif
