// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/viewwrapper.h"
#include "desktop/dialogs/logindialog.h"
#include "desktop/docks/navigator.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/transformsettings.h"
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasscene.h"
#include "desktop/view/canvasview.h"
#include "desktop/view/glcanvas.h"
#include "desktop/view/softwarecanvas.h"
#include "desktop/widgets/canvasframe.h"
#include "desktop/widgets/viewstatus.h"
#include "desktop/widgets/viewstatusbar.h"
#include "libclient/document.h"
#include "libclient/tools/toolcontroller.h"
#include <functional>

using std::placeholders::_5;

namespace view {

ViewWrapper::ViewWrapper(bool useOpenGl, QWidget *parent)
	: QObject(parent)
	, m_scene(new CanvasScene(!useOpenGl, parent))
	, m_controller(new CanvasController(m_scene, parent))
	, m_canvasWidget(instantiateView(useOpenGl, m_controller, parent))
	, m_view(new CanvasView(m_controller, m_canvasWidget, parent))
{
	dpApp().settings().bindUserMarkerPersistence(
		m_scene, &CanvasScene::setUserMarkerPersistence);
}

QAbstractScrollArea *ViewWrapper::viewWidget() const
{
	return m_view;
}

bool ViewWrapper::isTabletEnabled() const
{
	return m_controller->isTabletEnabled();
}

bool ViewWrapper::isTouchScrollEnabled() const
{
	return m_controller->isTouchPanEnabled();
}

bool ViewWrapper::isTouchDrawEnabled() const
{
	return m_controller->isTouchDrawEnabled();
}

QString ViewWrapper::pressureCurveAsString() const
{
	return m_controller->pressureCurve().toString();
}

QPoint ViewWrapper::viewCenterPoint() const
{
	return m_controller->viewCenterPoint();
}

QPointF ViewWrapper::viewTransformOffset() const
{
	return m_canvasWidget->viewTransformOffset();
}

bool ViewWrapper::isPointVisible(const QPointF &point) const
{
	return m_controller->isPointVisible(point);
}

QRectF ViewWrapper::screenRect() const
{
	return m_controller->screenRect();
}

canvas::CanvasModel *ViewWrapper::canvas() const
{
	return m_controller->canvasModel();
}

void ViewWrapper::setCanvas(canvas::CanvasModel *canvas)
{
	m_controller->setCanvasModel(canvas);
	m_scene->setCanvasModel(canvas);
}

drawingboard::AnnotationItem *ViewWrapper::getAnnotationItem(int annotationId)
{
	return m_scene->getAnnotationItem(annotationId);
}

void ViewWrapper::clearKeys()
{
	m_controller->clearKeys();
}

#ifdef __EMSCRIPTEN__
void ViewWrapper::setEnableEraserOverride(bool enableEraserOverride)
{
	m_controller->setEnableEraserOverride(enableEraserOverride);
}
#endif

void ViewWrapper::setShowAnnotations(bool showAnnotations)
{
	m_scene->setShowAnnotations(showAnnotations);
}

void ViewWrapper::setShowAnnotationBorders(bool showAnnotationBorders)
{
	m_scene->setShowAnnotationBorders(showAnnotationBorders);
}

void ViewWrapper::setShowLaserTrails(bool showLaserTrails)
{
	m_scene->setShowLaserTrails(showLaserTrails);
}

void ViewWrapper::setShowOwnUserMarker(bool showOwnUserMarker)
{
	m_scene->setShowOwnUserMarker(showOwnUserMarker);
}

void ViewWrapper::setPointerTracking(bool pointerTracking)
{
	m_controller->setPointerTracking(pointerTracking);
}

void ViewWrapper::setShowSelectionMask(bool showSelectionMask)
{
	m_scene->setShowSelectionMask(showSelectionMask);
}

void ViewWrapper::setShowToggleItems(bool showToggleItems)
{
	m_scene->setShowToggleItems(showToggleItems);
}

void ViewWrapper::setCatchupProgress(int percent, bool force)
{
	if(force || m_scene->hasCatchup()) {
		m_scene->setCatchupProgress(percent);
	}
}

void ViewWrapper::setStreamResetProgress(int percent)
{
	m_scene->setStreamResetProgress(percent);
}

void ViewWrapper::setSaveInProgress(bool saveInProgress)
{
	m_controller->setSaveInProgress(saveInProgress);
}

void ViewWrapper::showDisconnectedWarning(
	const QString &message, bool singleSession)
{
	m_view->showDisconnectedWarning(message, singleSession);
}

void ViewWrapper::hideDisconnectedWarning()
{
	m_view->hideDisconnectedWarning();
}

void ViewWrapper::showResetNotice(bool compatibilityMode, bool saveInProgress)
{
	m_view->showResetNotice(compatibilityMode, saveInProgress);
}

void ViewWrapper::hideResetNotice()
{
	m_view->hideResetNotice();
}

void ViewWrapper::disposeScene()
{
	// Nothing, not needed for this implementation.
}

void ViewWrapper::connectActions(const Actions &actions)
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
	connect(
		actions.zoomin, &QAction::triggered, m_controller,
		&CanvasController::zoomInCursor);
	connect(
		actions.zoomincenter, &QAction::triggered, m_controller,
		&CanvasController::zoomInCenter);
	connect(
		actions.zoomout, &QAction::triggered, m_controller,
		&CanvasController::zoomOutCursor);
	connect(
		actions.zoomoutcenter, &QAction::triggered, m_controller,
		&CanvasController::zoomOutCenter);
	connect(
		actions.zoomorig, &QAction::triggered, m_controller,
		&CanvasController::resetZoomCursor);
	connect(
		actions.zoomorigcenter, &QAction::triggered, m_controller,
		&CanvasController::resetZoomCenter);
	connect(
		actions.zoomfit, &QAction::triggered, m_controller,
		&CanvasController::zoomToFit);
	connect(
		actions.zoomfitwidth, &QAction::triggered, m_controller,
		&CanvasController::zoomToFitWidth);
	connect(
		actions.zoomfitheight, &QAction::triggered, m_controller,
		&CanvasController::zoomToFitHeight);
	connect(
		actions.rotateorig, &QAction::triggered, m_controller,
		&CanvasController::resetRotation);
	connect(
		actions.rotatecw, &QAction::triggered, m_controller,
		&CanvasController::rotateStepClockwise);
	connect(
		actions.rotateccw, &QAction::triggered, m_controller,
		&CanvasController::rotateStepCounterClockwise);
	connect(
		actions.viewflip, &QAction::triggered, m_controller,
		&CanvasController::setFlip);
	connect(
		actions.viewmirror, &QAction::triggered, m_controller,
		&CanvasController::setMirror);
	connect(
		actions.showgrid, &QAction::toggled, m_controller,
		&CanvasController::setPixelGrid);
	connect(
		actions.showusermarkers, &QAction::toggled, m_scene,
		&CanvasScene::setShowUserMarkers);
	connect(
		actions.showusernames, &QAction::toggled, m_scene,
		&CanvasScene::setShowUserNames);
	connect(
		actions.showuserlayers, &QAction::toggled, m_scene,
		&CanvasScene::setShowUserLayers);
	connect(
		actions.showuseravatars, &QAction::toggled, m_scene,
		&CanvasScene::setShowUserAvatars);
	connect(
		actions.evadeusercursors, &QAction::toggled, m_scene,
		&CanvasScene::setEvadeUserCursors);
}

void ViewWrapper::connectCanvasFrame(widgets::CanvasFrame *canvasFrame)
{
	connect(
		m_controller, &CanvasController::transformChanged, canvasFrame,
		&widgets::CanvasFrame::setTransform);
	connect(
		m_controller, &CanvasController::viewChanged, canvasFrame,
		&widgets::CanvasFrame::setView);
}

void ViewWrapper::connectDocument(Document *doc)
{
	connect(
		m_controller, &CanvasController::pointerMove, doc,
		&Document::sendPointerMove);

	tools::ToolController *toolCtrl = doc->toolCtrl();
	connect(
		toolCtrl, &tools::ToolController::activeAnnotationChanged, m_scene,
		&CanvasScene::setActiveAnnotation);
	connect(
		toolCtrl, &tools::ToolController::transformToolStateChanged, m_scene,
		&CanvasScene::setTransformToolState);
	connect(
		toolCtrl, &tools::ToolController::toolStateChanged, m_controller,
		&CanvasController::setToolState, Qt::QueuedConnection);
	connect(
		toolCtrl, &tools::ToolController::panRequested, m_controller,
		&CanvasController::scrollBy);
	connect(
		toolCtrl, &tools::ToolController::zoomRequested, m_controller,
		&CanvasController::zoomTo);
	connect(
		toolCtrl, &tools::ToolController::colorUsed, m_scene,
		&CanvasScene::setComparisonColor);
	connect(
		toolCtrl, &tools::ToolController::colorPickRequested, m_scene,
		&CanvasScene::setColorPick);
	connect(
		toolCtrl, &tools::ToolController::maskPreviewRequested, m_scene,
		&CanvasScene::setMaskPreview);
	connect(
		toolCtrl, &tools::ToolController::pathPreviewRequested, m_scene,
		&CanvasScene::setPathPreview);
	connect(
		toolCtrl, &tools::ToolController::toolNoticeRequested, m_scene,
		&CanvasScene::setToolNotice, Qt::QueuedConnection);

	connect(
		m_scene, &CanvasScene::annotationDeleted, toolCtrl,
		&tools::ToolController::deselectDeletedAnnotation);
	connect(
		m_controller, &CanvasController::canvasOffset, toolCtrl,
		&tools::ToolController::offsetActiveTool);
	connect(
		toolCtrl, &tools::ToolController::toolCapabilitiesChanged, m_scene,
		std::bind(&view::CanvasScene::setSelectionIgnored, m_scene, _5));
	m_scene->setSelectionIgnored(toolCtrl->activeToolIgnoresSelections());

	connect(
		toolCtrl, &tools::ToolController::toolCursorChanged, m_controller,
		&CanvasController::setToolCursor);
	m_controller->setToolCursor(toolCtrl->activeToolCursor());

	connect(
		toolCtrl, &tools::ToolController::toolCapabilitiesChanged, m_controller,
		&CanvasController::setToolCapabilities);
	m_controller->setToolCapabilities(
		toolCtrl->activeToolAllowColorPick(),
		toolCtrl->activeToolAllowToolAdjust(),
		toolCtrl->activeToolHandlesRightClick(),
		toolCtrl->activeToolIsFractional());

	connect(
		m_controller, &CanvasController::penDown, toolCtrl,
		&tools::ToolController::startDrawing);
	connect(
		m_controller, &CanvasController::penMove, toolCtrl,
		&tools::ToolController::continueDrawing);
	connect(
		m_controller, &CanvasController::penHover, toolCtrl,
		&tools::ToolController::hoverDrawing);
	connect(
		m_controller, &CanvasController::penUp, toolCtrl,
		&tools::ToolController::endDrawing);

	m_scene->setComparisonColor(toolCtrl->foregroundColor());
}

void ViewWrapper::connectLock(view::Lock *lock)
{
	connect(
		lock, &view::Lock::reasonsChanged, m_controller,
		&CanvasController::setLockReasons);
	connect(
		lock, &view::Lock::descriptionChanged, m_controller,
		&CanvasController::setLockDescription);
}

void ViewWrapper::connectLoginDialog(Document *doc, dialogs::LoginDialog *dlg)
{
	connect(doc, &Document::serverLoggedIn, dlg, [this]() {
		m_controller->setCanvasVisible(false);
		m_scene->setCanvasVisible(false);
	});
	connect(dlg, &dialogs::LoginDialog::destroyed, m_controller, [this]() {
		m_controller->setCanvasVisible(true);
		m_scene->setCanvasVisible(true);
	});
}

void ViewWrapper::connectMainWindow(MainWindow *mainWindow)
{
	connect(
		m_view, &CanvasView::imageDropped, mainWindow, &MainWindow::dropImage);
	connect(m_view, &CanvasView::urlDropped, mainWindow, &MainWindow::dropUrl);
	connect(
		m_controller, &CanvasController::toggleActionActivated, mainWindow,
		&MainWindow::handleToggleAction);
	connect(
		m_controller, &CanvasController::touchTapActionActivated, mainWindow,
		&MainWindow::handleTouchTapAction);
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
		mainWindow, &MainWindow::viewShifted, m_controller,
		&CanvasController::scrollByF);
}

void ViewWrapper::connectNavigator(docks::Navigator *navigator)
{
	connect(
		navigator, &docks::Navigator::focusMoved, m_controller,
		&CanvasController::scrollTo);
	connect(
		navigator, &docks::Navigator::wheelZoom, m_controller,
		&CanvasController::zoomSteps);
	connect(
		navigator, &docks::Navigator::zoomChanged, m_controller,
		&CanvasController::setZoom);
	connect(
		m_controller, &CanvasController::viewChanged, navigator,
		&docks::Navigator::setViewFocus);
	connect(
		m_controller, &CanvasController::transformChanged, navigator,
		&docks::Navigator::setViewTransformation);
}

void ViewWrapper::connectToolSettings(docks::ToolSettings *toolSettings)
{
	connect(
		toolSettings, &docks::ToolSettings::sizeChanged, m_controller,
		&CanvasController::setOutlineSize);
	connect(
		toolSettings, &docks::ToolSettings::subpixelModeChanged, m_controller,
		&CanvasController::setOutlineMode);
	connect(
		m_view, &CanvasView::colorDropped, toolSettings,
		&docks::ToolSettings::setForegroundColor);
	connect(
		m_controller, &CanvasController::quickAdjust, toolSettings,
		&docks::ToolSettings::quickAdjustCurrent1);

	tools::BrushSettings *brushSettings = toolSettings->brushSettings();
	connect(
		brushSettings, &tools::BrushSettings::blendModeChanged, m_controller,
		&CanvasController::setBrushBlendMode);

	tools::LaserPointerSettings *laserPointerSettings =
		toolSettings->laserPointerSettings();
	connect(
		laserPointerSettings,
		&tools::LaserPointerSettings::pointerTrackingToggled, m_controller,
		&CanvasController::setPointerTracking);

	tools::ZoomSettings *zoomSettings = toolSettings->zoomSettings();
	connect(
		zoomSettings, &tools::ZoomSettings::resetZoom, m_controller,
		&CanvasController::resetZoomCenter);
	connect(
		zoomSettings, &tools::ZoomSettings::fitToWindow, m_controller,
		&CanvasController::zoomToFit);

	toolSettings->annotationSettings()->setCanvasView(this);
	toolSettings->transformSettings()->setCanvasView(this);
}

void ViewWrapper::connectViewStatus(widgets::ViewStatus *viewStatus)
{
	connect(
		m_controller, &CanvasController::transformChanged, viewStatus,
		&widgets::ViewStatus::setTransformation);
	connect(
		viewStatus, &widgets::ViewStatus::zoomStepped, m_controller,
		&CanvasController::zoomSteps);
	connect(
		viewStatus, &widgets::ViewStatus::zoomChanged, m_controller,
		&CanvasController::setZoom);
	connect(
		viewStatus, &widgets::ViewStatus::angleChanged, m_controller,
		&CanvasController::setRotation);
}

void ViewWrapper::connectViewStatusBar(widgets::ViewStatusBar *viewStatusBar)
{
	connect(
		m_controller, &CanvasController::coordinatesChanged, viewStatusBar,
		&widgets::ViewStatusBar::setCoordinates);
}

CanvasInterface *ViewWrapper::instantiateView(
	bool useOpenGl, CanvasController *controller, QWidget *parent)
{
	if(useOpenGl) {
		return new GlCanvas(controller, parent);
	} else {
		return new SoftwareCanvas(controller, parent);
	}
}

}
