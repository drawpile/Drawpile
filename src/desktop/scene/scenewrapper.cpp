// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/scenewrapper.h"
#include "desktop/dialogs/logindialog.h"
#include "desktop/docks/navigator.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "desktop/scene/canvasscene.h"
#include "desktop/scene/canvasview.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/widgets/viewstatus.h"
#include "desktop/widgets/viewstatusbar.h"
#include "libclient/document.h"
#include "libclient/tools/toolcontroller.h"

namespace drawingboard {

SceneWrapper::SceneWrapper(QWidget *parent)
	: QObject(parent)
	, m_view(new CanvasView(parent))
	, m_scene(new CanvasScene(parent))
{
	m_scene->setBackgroundBrush(
		parent->palette().brush(QPalette::Active, QPalette::Window));
	m_scene->showToggleItems(dpApp().smallScreenMode());
	m_view->setCanvas(m_scene);
}

QWidget *SceneWrapper::viewWidget() const
{
	return m_view;
}

bool SceneWrapper::isTabletEnabled() const
{
	return m_view->isTabletEnabled();
}

bool SceneWrapper::isTouchScrollEnabled() const
{
	return m_view->isTouchScrollEnabled();
}

bool SceneWrapper::isTouchDrawEnabled() const
{
	return m_view->isTouchDrawEnabled();
}

QString SceneWrapper::pressureCurveAsString() const
{
	return m_view->pressureCurve().toString();
}

QPoint SceneWrapper::viewCenterPoint() const
{
	return m_view->viewCenterPoint();
}

bool SceneWrapper::isPointVisible(const QPointF &point) const
{
	return m_view->isPointVisible(point);
}

QRectF SceneWrapper::screenRect() const
{
	return m_view->mapToCanvas(m_view->rect()).boundingRect();
}

canvas::CanvasModel *SceneWrapper::canvas() const
{
	return m_scene ? m_scene->model() : nullptr;
}

void SceneWrapper::setCanvas(canvas::CanvasModel *canvas)
{
	if(m_scene) {
		m_scene->initCanvas(canvas);
	}
}

drawingboard::AnnotationItem *SceneWrapper::getAnnotationItem(int annotationId)
{
	return m_scene ? m_scene->getAnnotationItem(annotationId) : nullptr;
}

void SceneWrapper::clearKeys()
{
	m_view->clearKeys();
}

#ifdef __EMSCRIPTEN__
void SceneWrapper::setEnableEraserOverride(bool enableEraserOverride)
{
	m_view->setEnableEraserOverride(enableEraserOverride);
}
#endif

void SceneWrapper::setShowAnnotations(bool showAnnotations)
{
	if(m_scene) {
		m_scene->showAnnotations(showAnnotations);
	}
}

void SceneWrapper::setShowAnnotationBorders(bool showAnnotationBorders)
{
	if(m_scene) {
		m_scene->showAnnotationBorders(showAnnotationBorders);
	}
}

void SceneWrapper::setShowLaserTrails(bool showLaserTrails)
{
	if(m_scene) {
		m_scene->showLaserTrails(showLaserTrails);
	}
}

void SceneWrapper::setShowOwnUserMarker(bool showOwnUserMarker)
{
	if(m_scene) {
		m_scene->setShowOwnUserMarker(showOwnUserMarker);
	}
}

void SceneWrapper::setPointerTracking(bool pointerTracking)
{
	m_view->setPointerTracking(pointerTracking);
}

void SceneWrapper::setCatchupProgress(int percent, bool force)
{
	if(m_scene && (force || m_scene->hasCatchup())) {
		m_scene->setCatchupProgress(percent);
	}
}

void SceneWrapper::setSaveInProgress(bool saveInProgress)
{
	m_view->setSaveInProgress(saveInProgress);
}

void SceneWrapper::showDisconnectedWarning(
	const QString &message, bool singleSession)
{
	m_view->showDisconnectedWarning(message, singleSession);
}

void SceneWrapper::hideDisconnectedWarning()
{
	m_view->hideDisconnectedWarning();
}

void SceneWrapper::showResetNotice(bool compatibilityMode, bool saveInProgress)
{
	m_view->showResetNotice(compatibilityMode, saveInProgress);
}

void SceneWrapper::hideResetNotice()
{
	m_view->hideResetNotice();
}

void SceneWrapper::disposeScene()
{
	m_view->setScene(nullptr);
	delete m_scene;
	m_scene = nullptr;
}

void SceneWrapper::connectActions(const Actions &actions)
{
	connect(
		actions.moveleft, &QAction::triggered, m_view,
		&CanvasView::scrollStepLeft);
	connect(
		actions.moveright, &QAction::triggered, m_view,
		&CanvasView::scrollStepRight);
	connect(
		actions.moveup, &QAction::triggered, m_view, &CanvasView::scrollStepUp);
	connect(
		actions.movedown, &QAction::triggered, m_view,
		&CanvasView::scrollStepDown);
	connect(actions.zoomin, &QAction::triggered, m_view, &CanvasView::zoomin);
	connect(actions.zoomout, &QAction::triggered, m_view, &CanvasView::zoomout);
	connect(
		actions.zoomorig, &QAction::triggered, m_view, &CanvasView::resetZoom);
	connect(
		actions.zoomfit, &QAction::triggered, m_view, &CanvasView::zoomToFit);
	connect(
		actions.zoomfitwidth, &QAction::triggered, m_view,
		&CanvasView::zoomToFitWidth);
	connect(
		actions.zoomfitheight, &QAction::triggered, m_view,
		&CanvasView::zoomToFitHeight);
	connect(
		actions.rotateorig, &QAction::triggered, m_view,
		&CanvasView::resetRotation);
	connect(
		actions.rotatecw, &QAction::triggered, m_view,
		&CanvasView::rotateStepClockwise);
	connect(
		actions.rotateccw, &QAction::triggered, m_view,
		&CanvasView::rotateStepCounterClockwise);
	connect(
		actions.viewflip, &QAction::triggered, m_view,
		&CanvasView::setViewFlip);
	connect(
		actions.viewmirror, &QAction::triggered, m_view,
		&CanvasView::setViewMirror);
	connect(
		actions.showgrid, &QAction::toggled, m_view, &CanvasView::setPixelGrid);
	connect(
		actions.showusermarkers, &QAction::toggled, m_scene,
		&drawingboard::CanvasScene::showUserMarkers);
	connect(
		actions.showusernames, &QAction::toggled, m_scene,
		&drawingboard::CanvasScene::showUserNames);
	connect(
		actions.showuserlayers, &QAction::toggled, m_scene,
		&drawingboard::CanvasScene::showUserLayers);
	connect(
		actions.showuseravatars, &QAction::toggled, m_scene,
		&drawingboard::CanvasScene::showUserAvatars);
}

void SceneWrapper::connectDocument(Document *doc)
{
	connect(m_view, &CanvasView::pointerMoved, doc, &Document::sendPointerMove);

	tools::ToolController *toolCtrl = doc->toolCtrl();
	connect(
		toolCtrl, &tools::ToolController::activeAnnotationChanged, m_scene,
		&drawingboard::CanvasScene::setActiveAnnotation);
	connect(
		toolCtrl, &tools::ToolController::busyStateChanged, m_view,
		&CanvasView::setBusy);
	connect(
		toolCtrl, &tools::ToolController::zoomRequested, m_view,
		&CanvasView::zoomTo);

	connect(
		m_scene, &drawingboard::CanvasScene::annotationDeleted, toolCtrl,
		&tools::ToolController::deselectDeletedAnnotation);
	connect(
		m_scene, &drawingboard::CanvasScene::canvasResized, toolCtrl,
		&tools::ToolController::offsetActiveTool);

	connect(
		toolCtrl, &tools::ToolController::toolCursorChanged, m_view,
		&CanvasView::setToolCursor);
	m_view->setToolCursor(toolCtrl->activeToolCursor());

	connect(
		toolCtrl, &tools::ToolController::toolCapabilitiesChanged, m_view,
		&CanvasView::setToolCapabilities);
	m_view->setToolCapabilities(
		toolCtrl->activeToolAllowColorPick(),
		toolCtrl->activeToolAllowToolAdjust(),
		toolCtrl->activeToolHandlesRightClick());

	connect(
		m_view, &CanvasView::penDown, toolCtrl,
		&tools::ToolController::startDrawing);
	connect(
		m_view, &CanvasView::penMove, toolCtrl,
		&tools::ToolController::continueDrawing);
	connect(
		m_view, &CanvasView::penHover, toolCtrl,
		&tools::ToolController::hoverDrawing);
	connect(
		m_view, &CanvasView::penUp, toolCtrl,
		&tools::ToolController::endDrawing);
}

void SceneWrapper::connectLock(view::Lock *lock)
{
	connect(
		lock, &view::Lock::reasonsChanged, m_view, &CanvasView::setLockReasons);
	connect(
		lock, &view::Lock::descriptionChanged, m_view,
		&CanvasView::setLockDescription);
}

void SceneWrapper::connectLoginDialog(Document *doc, dialogs::LoginDialog *dlg)
{
	if(m_scene) {
		connect(doc, &Document::serverLoggedIn, dlg, [this]() {
			if(m_scene) {
				m_scene->hideCanvas();
			}
		});
		connect(
			dlg, &dialogs::LoginDialog::destroyed, m_scene,
			&drawingboard::CanvasScene::showCanvas);
	}
}

void SceneWrapper::connectMainWindow(MainWindow *mainWindow)
{
	connect(
		m_view, &CanvasView::imageDropped, mainWindow, &MainWindow::dropImage);
	connect(m_view, &CanvasView::urlDropped, mainWindow, &MainWindow::dropUrl);
	connect(
		m_view, &CanvasView::toggleActionActivated, mainWindow,
		&MainWindow::handleToggleAction);
	connect(
		m_view, &CanvasView::reconnectRequested, mainWindow,
		&MainWindow::reconnect);
	connect(
		m_view, &CanvasView::savePreResetStateRequested, mainWindow,
		&MainWindow::savePreResetImageAs);
	connect(
		m_view, &CanvasView::savePreResetStateDismissed, mainWindow,
		&MainWindow::discardPreResetImage);
	connect(
		mainWindow, &MainWindow::viewShifted, m_view, &CanvasView::scrollBy);
}

void SceneWrapper::connectNavigator(docks::Navigator *navigator)
{
	connect(
		navigator, &docks::Navigator::focusMoved, m_view,
		&CanvasView::scrollTo);
	connect(
		navigator, &docks::Navigator::wheelZoom, m_view,
		&CanvasView::zoomSteps);
	connect(
		navigator, &docks::Navigator::zoomChanged, m_view,
		&CanvasView::setZoom);
	connect(
		m_view, &CanvasView::viewRectChange, navigator,
		&docks::Navigator::setViewFocus);
	connect(
		m_view, &CanvasView::viewTransformed, navigator,
		&docks::Navigator::setViewTransformation);
}

void SceneWrapper::connectToolSettings(docks::ToolSettings *toolSettings)
{
	connect(
		toolSettings, &docks::ToolSettings::sizeChanged, m_view,
		&CanvasView::setOutlineSize);
	connect(
		toolSettings, &docks::ToolSettings::subpixelModeChanged, m_view,
		&CanvasView::setOutlineMode);
	connect(
		m_view, &CanvasView::colorDropped, toolSettings,
		&docks::ToolSettings::setForegroundColor);
	connect(
		m_view, &CanvasView::quickAdjust, toolSettings,
		&docks::ToolSettings::quickAdjustCurrent1);

	tools::BrushSettings *brushSettings = toolSettings->brushSettings();
	connect(
		brushSettings, &tools::BrushSettings::blendModeChanged, m_view,
		&CanvasView::setBrushBlendMode);

	tools::LaserPointerSettings *laserPointerSettings =
		toolSettings->laserPointerSettings();
	connect(
		laserPointerSettings,
		&tools::LaserPointerSettings::pointerTrackingToggled, m_view,
		&CanvasView::setPointerTracking);

	tools::ZoomSettings *zoomSettings = toolSettings->zoomSettings();
	connect(
		zoomSettings, &tools::ZoomSettings::resetZoom, m_view,
		&CanvasView::resetZoom);
	connect(
		zoomSettings, &tools::ZoomSettings::fitToWindow, m_view,
		&CanvasView::zoomToFit);

	toolSettings->annotationSettings()->setCanvasView(this);
	toolSettings->selectionSettings()->setCanvasView(this);
}

void SceneWrapper::connectViewStatus(widgets::ViewStatus *viewStatus)
{
	connect(
		m_view, &CanvasView::viewTransformed, viewStatus,
		&widgets::ViewStatus::setTransformation);
	connect(
		viewStatus, &widgets::ViewStatus::zoomChanged, m_view,
		&CanvasView::setZoom);
	connect(
		viewStatus, &widgets::ViewStatus::angleChanged, m_view,
		&CanvasView::setRotation);
}

void SceneWrapper::connectViewStatusBar(widgets::ViewStatusBar *viewStatusBar)
{
	connect(
		m_view, &CanvasView::coordinatesChanged, viewStatusBar,
		&widgets::ViewStatusBar::setCoordinates);
}

}
