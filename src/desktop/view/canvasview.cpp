// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/canvasview.h"
#include "desktop/main.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasinterface.h"
#include "desktop/view/canvasscene.h"
#include "desktop/view/glcanvas.h"
#include "desktop/widgets/notifbar.h"
#include "libclient/drawdance/eventlog.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGestureEvent>
#include <QMimeData>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTabletEvent>
#include <QTouchEvent>

namespace view {

CanvasView::CanvasView(
	CanvasController *controller, CanvasInterface *canvasWidget,
	QWidget *parent)
	: QAbstractScrollArea(parent)
	, m_controller(controller)
	, m_canvasWidget(canvasWidget)
{
	setFrameShape(QFrame::NoFrame);

	QWidget *vp = m_canvasWidget->asWidget();
	setViewport(vp);
	vp->setAcceptDrops(true);
	vp->setAttribute(Qt::WA_AcceptTouchEvents);
	vp->setMouseTracking(true);
	vp->setTabletTracking(true);
	vp->ungrabGesture(Qt::TapGesture);
	vp->ungrabGesture(Qt::TapAndHoldGesture);
	vp->ungrabGesture(Qt::PanGesture);
	vp->ungrabGesture(Qt::PinchGesture);
	vp->ungrabGesture(Qt::SwipeGesture);

	m_notificationBar = new widgets::NotificationBar(this);
	connect(
		m_notificationBar, &widgets::NotificationBar::actionButtonClicked, this,
		&CanvasView::activateNotificationBarAction);
	connect(
		m_notificationBar, &widgets::NotificationBar::closeButtonClicked, this,
		&CanvasView::dismissNotificationBar);
	connect(
		m_notificationBar, &widgets::NotificationBar::heightChanged,
		controller->scene(), &CanvasScene::setNotificationBarHeight);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindCanvasScrollBars(this, &CanvasView::setEnableScrollBars);
	settings.bindTouchGestures(this, &CanvasView::setTouchUseGestureEvents);

	connect(
		m_controller, &CanvasController::scrollAreaChanged, this,
		&CanvasView::onControllerScrollAreaChanged);
	connect(
		m_controller, &CanvasController::cursorChanged, this,
		&CanvasView::setCursor);
	connect(
		m_controller, &CanvasController::penDown, this,
		&CanvasView::autoDismissNotificationBar);
	connect(
		m_controller, &CanvasController::saveInProgressChanged, this,
		&CanvasView::setNotificationBarSaveInProgress);
}

void CanvasView::scrollStepLeft()
{
	QScrollBar *hbar = horizontalScrollBar();
	hbar->setValue(hbar->value() - hbar->singleStep());
}

void CanvasView::scrollStepRight()
{
	QScrollBar *hbar = horizontalScrollBar();
	hbar->setValue(hbar->value() + hbar->singleStep());
}

void CanvasView::scrollStepUp()
{
	QScrollBar *vbar = verticalScrollBar();
	vbar->setValue(vbar->value() - vbar->singleStep());
}

void CanvasView::scrollStepDown()
{
	QScrollBar *vbar = verticalScrollBar();
	vbar->setValue(vbar->value() + vbar->singleStep());
}

void CanvasView::showDisconnectedWarning(
	const QString &message, bool singleSession)
{
	if(m_notificationBarState != NotificationBarState::Reconnect) {
		dismissNotificationBar();
		m_notificationBarState = NotificationBarState::Reconnect;
		m_notificationBar->show(
			message, QIcon::fromTheme("view-refresh"),
			QCoreApplication::translate("widgets::CanvasView", "Reconnect"),
			widgets::NotificationBar::RoleColor::Warning, !singleSession);
		m_notificationBar->setActionButtonEnabled(true);
	}
}

void CanvasView::hideDisconnectedWarning()
{
	if(m_notificationBarState == NotificationBarState::Reconnect) {
		m_notificationBarState = NotificationBarState::None;
		m_notificationBar->hide();
	}
}

void CanvasView::showResetNotice(bool saveInProgress)
{
	if(m_notificationBarState != NotificationBarState::Reset) {
		dismissNotificationBar();
		m_notificationBarState = NotificationBarState::Reset;
		QString message = QCoreApplication::translate(
			"widgets::CanvasView", "Do you want to save the canvas "
								   "as it was before the reset?");
		m_notificationBar->show(
			message, QIcon::fromTheme("document-save-as"), tr("Save As…"),
			widgets::NotificationBar::RoleColor::Notice);
		m_notificationBar->setActionButtonEnabled(!saveInProgress);
	}
}

void CanvasView::hideResetNotice()
{
	if(m_notificationBarState == NotificationBarState::Reset) {
		m_notificationBarState = NotificationBarState::None;
		m_notificationBar->hide();
	}
}

void CanvasView::focusInEvent(QFocusEvent *event)
{
	QAbstractScrollArea::focusInEvent(event);
	m_controller->handleFocusIn();
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
	QAbstractScrollArea::resizeEvent(event);
	m_canvasWidget->handleResize(event);
}

void CanvasView::paintEvent(QPaintEvent *event)
{
	QAbstractScrollArea::paintEvent(event);
	m_canvasWidget->handlePaint(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	QAbstractScrollArea::mouseMoveEvent(event);
	m_controller->handleMouseMove(event);
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
	QAbstractScrollArea::mousePressEvent(event);
	m_controller->handleMousePress(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	QAbstractScrollArea::mouseReleaseEvent(event);
	m_controller->handleMouseRelease(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
	mousePressEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	m_controller->handleWheel(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
	QAbstractScrollArea::keyPressEvent(event);
	m_controller->handleKeyPress(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
	QAbstractScrollArea::keyReleaseEvent(event);
	m_controller->handleKeyRelease(event);
}

void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
	handleDragDrop(event, false);
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
	handleDragDrop(event, false);
}

void CanvasView::dropEvent(QDropEvent *event)
{
	handleDragDrop(event, true);
}

bool CanvasView::viewportEvent(QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter: {
		// Give focus to this widget on mouseover. This is so that
		// using a key for dragging works right away. Avoid stealing
		// focus from text edit widgets though.
		QWidget *oldfocus = QApplication::focusWidget();
		if(!oldfocus || !(oldfocus->inherits("QLineEdit") ||
						  oldfocus->inherits("QTextEdit") ||
						  oldfocus->inherits("QPlainTextEdit"))) {
			setFocus(Qt::MouseFocusReason);
		}
		m_controller->handleEnter();
		break;
	}
	case QEvent::Leave:
		m_controller->handleLeave();
		break;
	case QEvent::TabletMove:
		m_controller->handleTabletMove(static_cast<QTabletEvent *>(event));
		break;
	case QEvent::TabletPress:
		m_controller->handleTabletPress(static_cast<QTabletEvent *>(event));
		break;
	case QEvent::TabletRelease:
		m_controller->handleTabletRelease(static_cast<QTabletEvent *>(event));
		break;
	// Touch and gesture events must not call the parent function, otherwise
	// it will somehow capture them operation and one-finger touch won't work.
	case QEvent::TouchBegin:
		if(m_touchUseGestureEvents) {
			break;
		} else {
			m_controller->handleTouchBegin(static_cast<QTouchEvent *>(event));
			return true;
		}
	case QEvent::TouchUpdate:
		if(m_touchUseGestureEvents) {
			break;
		} else {
			m_controller->handleTouchUpdate(static_cast<QTouchEvent *>(event));
			return true;
		}
	case QEvent::TouchEnd:
		if(m_touchUseGestureEvents) {
			break;
		} else {
			m_controller->handleTouchEnd(
				static_cast<QTouchEvent *>(event), false);
			return true;
		}
	case QEvent::TouchCancel:
		if(m_touchUseGestureEvents) {
			break;
		} else {
			m_controller->handleTouchEnd(
				static_cast<QTouchEvent *>(event), true);
			return true;
		}
	case QEvent::Gesture:
		if(m_touchUseGestureEvents) {
			m_controller->handleGesture(static_cast<QGestureEvent *>(event));
			return true;
		} else {
			break;
		}
	default:
		break;
	}
	return QAbstractScrollArea::viewportEvent(event);
}

void CanvasView::scrollContentsBy(int dx, int dy)
{
	if(!m_blockScrolling) {
		m_controller->scrollBy(-dx, -dy);
	}
	QAbstractScrollArea::scrollContentsBy(dx, dy);
}

void CanvasView::setEnableScrollBars(bool enableScrollBars)
{
	Qt::ScrollBarPolicy policy =
		enableScrollBars ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff;
	setHorizontalScrollBarPolicy(policy);
	setVerticalScrollBarPolicy(policy);
}

void CanvasView::setTouchUseGestureEvents(bool touchUseGestureEvents)
{
	if(touchUseGestureEvents && !m_touchUseGestureEvents) {
		DP_EVENT_LOG("grab gesture events");
		viewport()->grabGesture(Qt::TapGesture);
		viewport()->grabGesture(Qt::PanGesture);
		viewport()->grabGesture(Qt::PinchGesture);
		m_touchUseGestureEvents = true;
	} else if(!touchUseGestureEvents && m_touchUseGestureEvents) {
		DP_EVENT_LOG("ungrab gesture events");
		viewport()->ungrabGesture(Qt::TapGesture);
		viewport()->ungrabGesture(Qt::PanGesture);
		viewport()->ungrabGesture(Qt::PinchGesture);
		m_touchUseGestureEvents = false;
	}
}

void CanvasView::handleDragDrop(QDropEvent *event, bool drop)
{
	const QMimeData *mimeData = event->mimeData();
	if(mimeData->hasImage()) {
		event->acceptProposedAction();
		if(drop) {
			emit imageDropped(qvariant_cast<QImage>(mimeData->imageData()));
		}
	} else if(mimeData->hasUrls() && !mimeData->urls().isEmpty()) {
		event->acceptProposedAction();
		if(drop) {
			emit urlDropped(mimeData->urls().first());
		}
	} else if(mimeData->hasColor()) {
		event->acceptProposedAction();
		if(drop) {
			emit colorDropped(event->mimeData()->colorData().value<QColor>());
		}
	}
}

void CanvasView::onControllerScrollAreaChanged(
	int minH, int maxH, int valueH, int pageStepH, int singleStepH, int minV,
	int maxV, int valueV, int pageStepV, int singleStepV)
{
	QScopedValueRollback<bool> rollback(m_blockScrolling, true);
	QScrollBar *hbar = horizontalScrollBar();
	QScrollBar *vbar = verticalScrollBar();
	hbar->setRange(minH, maxH);
	vbar->setRange(minV, maxV);
	hbar->setValue(valueH);
	vbar->setValue(valueV);
	hbar->setPageStep(pageStepH);
	vbar->setPageStep(pageStepV);
	hbar->setSingleStep(singleStepH);
	vbar->setSingleStep(singleStepV);
}

void CanvasView::activateNotificationBarAction()
{
	switch(m_notificationBarState) {
	case NotificationBarState::Reconnect:
		hideDisconnectedWarning();
		emit reconnectRequested();
		break;
	case NotificationBarState::Reset:
		emit savePreResetStateRequested();
		break;
	default:
		qWarning(
			"Unknown notification bar state %d", int(m_notificationBarState));
		break;
	}
}

void CanvasView::dismissNotificationBar()
{
	m_notificationBar->hide();
	NotificationBarState state = m_notificationBarState;
	m_notificationBarState = NotificationBarState::None;
	switch(state) {
	case NotificationBarState::None:
	case NotificationBarState::Reconnect:
		break;
	case NotificationBarState::Reset:
		emit savePreResetStateDismissed();
		break;
	default:
		qWarning("Unknown notification bar state %d", int(state));
		break;
	}
}

void CanvasView::autoDismissNotificationBar()
{
	if(m_notificationBarState == NotificationBarState::Reset &&
	   m_notificationBar->isActionButtonEnabled()) {
		// There's a notice asking the user if they want to save the
		// pre-reset image, but they started drawing again, so
		// apparently they don't want to save it. Start a timer to
		// automatically dismiss the notice.
		m_notificationBar->startAutoDismissTimer();
	}
}

void CanvasView::setNotificationBarSaveInProgress(bool saveInProgress)
{
	m_notificationBar->setActionButtonEnabled(
		m_notificationBarState != NotificationBarState::Reset ||
		!saveInProgress);
}

}
