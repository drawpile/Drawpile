// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_WRAPPER_H
#define DESKTOP_SCENE_WRAPPER_H
#include "desktop/view/canvaswrapper.h"
#include <QObject>

namespace widgets {
class CanvasView;
}

namespace drawingboard {

class CanvasScene;

class SceneWrapper final : public QObject, public view::CanvasWrapper {
	Q_OBJECT
	using CanvasView = widgets::CanvasView;

public:
	SceneWrapper(QWidget *parent = nullptr);

	QWidget *viewWidget() const override;

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

#ifdef __EMSCRIPTEN__
	void setEnableEraserOverride(bool enableEraserOverride) override;
#endif

	void setShowAnnotations(bool showAnnotations) override;
	void setShowAnnotationBorders(bool showAnnotationBorders) override;
	void setShowLaserTrails(bool showLaserTrails) override;
	void setShowOwnUserMarker(bool showOwnUserMarker) override;
	void setPointerTracking(bool pointerTracking) override;

	void setCatchupProgress(int percent, bool force) override;
	void setSaveInProgress(bool saveInProgress) override;

	void showDisconnectedWarning(
		const QString &message, bool singleSession) override;
	void hideDisconnectedWarning() override;
	void showResetNotice(bool compatibilityMode, bool saveInProgress) override;
	void hideResetNotice() override;

	void disposeScene() override;

	void connectActions(const Actions &actions) override;

	void connectDocument(Document *doc) override;

	void connectLock(view::Lock *lock) override;

	void connectLoginDialog(Document *doc, dialogs::LoginDialog *dlg) override;

	void connectMainWindow(MainWindow *mainWindow) override;

	void connectNavigator(docks::Navigator *navigator) override;

	void connectToolSettings(docks::ToolSettings *toolSettings) override;

	void connectViewStatus(widgets::ViewStatus *viewStatus) override;

	void connectViewStatusBar(widgets::ViewStatusBar *viewStatusBar) override;

private:
	CanvasView *m_view;
	CanvasScene *m_scene;
};

}

#endif
