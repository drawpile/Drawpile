// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/widgetutils.h"
#include "desktop/main.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QPair>
#include <QScrollBar>
#include <QWidget>

namespace utils {

ScopedUpdateDisabler::ScopedUpdateDisabler(QWidget *widget)
	: m_widget{widget}
	, m_wasEnabled{widget->updatesEnabled()}
{
	if(m_wasEnabled) {
		widget->setUpdatesEnabled(false);
	}
}

ScopedUpdateDisabler::~ScopedUpdateDisabler()
{
	if(m_wasEnabled) {
		m_widget->setUpdatesEnabled(true);
	}
}


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Kinetic scroll event filter from Krita
KisKineticScrollerEventFilter::KisKineticScrollerEventFilter(
	QScroller::ScrollerGestureType gestureType, QAbstractScrollArea *parent)
	: QObject(parent)
	, m_scrollArea(parent)
	, m_gestureType(gestureType)
{
}

bool KisKineticScrollerEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
		QScroller::ungrabGesture(m_scrollArea);
		break;
	case QEvent::Leave:
		QScroller::grabGesture(m_scrollArea, m_gestureType);
		break;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}
// SPDX-SnippetEnd


void showWindow(QWidget *widget, bool maximized)
{
	// On Android, we rarely want small windows unless it's like a simple
	// message box or something. Anything more complex is probably better off
	// being a full-screen window, which is also more akin to how Android's
	// native UI works. This wrapper takes care of that very common switch.
#ifdef Q_OS_ANDROID
	Q_UNUSED(maximized);
	widget->showFullScreen();
#else
	if(maximized) {
		widget->showMaximized();
	} else {
		widget->show();
	}
#endif
}

void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize)
{
	QSizePolicy sp = widget->sizePolicy();
	sp.setRetainSizeWhenHidden(retainSize);
	widget->setSizePolicy(sp);
}

static QPair<bool, QScroller::ScrollerGestureType>
getKineticScrollEnabledGesture(const desktop::settings::Settings &settings)
{
	switch(settings.kineticScrollGesture()) {
	case int(desktop::settings::KineticScrollGesture::LeftClick):
		return {true, QScroller::LeftMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::MiddleClick):
		return {true, QScroller::MiddleMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::RightClick):
		return {true, QScroller::RightMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::Touch):
		return {true, QScroller::TouchGesture};
	default:
		return {false, QScroller::ScrollerGestureType(0)};
	}
}

void initKineticScrolling(QAbstractScrollArea *scrollArea)
{
	const desktop::settings::Settings &settings = dpApp().settings();
	auto [enabled, gestureType] = getKineticScrollEnabledGesture(settings);

	// SPDX-SnippetBegin
	// SPDX-License-Identifier: GPL-3.0-or-later
	// SDPX—SnippetName: Kinetic scroll setup from Krita
	if(scrollArea && enabled) {
		int sensitivity =
			100 - qBound(0, settings.kineticScrollThreshold(), 100);
		bool hideScrollBars = settings.kineticScrollHideBars();
		float resistanceCoefficient = 10.0f;
		float dragVelocitySmoothFactor = 1.0f;
		float minimumVelocity = 0.0f;
		float axisLockThresh = 1.0f;
		float maximumClickThroughVelocity = 0.0f;
		float flickAccelerationFactor = 1.5f;
		float overshootDragResistanceFactor = 0.1f;
		float overshootDragDistanceFactor = 0.3f;
		float overshootScrollDistanceFactor = 0.1f;
		float overshootScrollTime = 0.4f;

		if(hideScrollBars) {
			scrollArea->setVerticalScrollBarPolicy(
				Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
			scrollArea->setHorizontalScrollBarPolicy(
				Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
		} else if(gestureType != QScroller::TouchGesture) {
			auto *filter =
				new KisKineticScrollerEventFilter(gestureType, scrollArea);
			scrollArea->horizontalScrollBar()->installEventFilter(filter);
			scrollArea->verticalScrollBar()->installEventFilter(filter);
		}

		QAbstractItemView *itemView =
			qobject_cast<QAbstractItemView *>(scrollArea);
		if(itemView) {
			itemView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		}

		QScroller *scroller = QScroller::scroller(scrollArea);
		// TODO: this leaks memory, see QTBUG-82355.
		QScroller::grabGesture(scrollArea, gestureType);

		QScrollerProperties properties;

		// DragStartDistance seems to be based on meter per second; though
		// it's not explicitly documented, other QScroller values are in
		// that metric. To start kinetic scrolling, with minimal
		// sensitivity, we expect a drag of 10 mm, with minimum sensitivity
		// any > 0 mm.
		const float mm = 0.001f;
		const float resistance = 1.0f - (sensitivity / 100.0f);
		const float mousePressEventDelay = 1.0f - 0.75f * resistance;

		properties.setScrollMetric(
			QScrollerProperties::DragStartDistance,
			resistance * resistanceCoefficient * mm);
		properties.setScrollMetric(
			QScrollerProperties::DragVelocitySmoothingFactor,
			dragVelocitySmoothFactor);
		properties.setScrollMetric(
			QScrollerProperties::MinimumVelocity, minimumVelocity);
		properties.setScrollMetric(
			QScrollerProperties::AxisLockThreshold, axisLockThresh);
		properties.setScrollMetric(
			QScrollerProperties::MaximumClickThroughVelocity,
			maximumClickThroughVelocity);
		properties.setScrollMetric(
			QScrollerProperties::MousePressEventDelay, mousePressEventDelay);
		properties.setScrollMetric(
			QScrollerProperties::AcceleratingFlickSpeedupFactor,
			flickAccelerationFactor);

		properties.setScrollMetric(
			QScrollerProperties::VerticalOvershootPolicy,
			QScrollerProperties::OvershootAlwaysOn);
		properties.setScrollMetric(
			QScrollerProperties::OvershootDragResistanceFactor,
			overshootDragResistanceFactor);
		properties.setScrollMetric(
			QScrollerProperties::OvershootDragDistanceFactor,
			overshootDragDistanceFactor);
		properties.setScrollMetric(
			QScrollerProperties::OvershootScrollDistanceFactor,
			overshootScrollDistanceFactor);
		properties.setScrollMetric(
			QScrollerProperties::OvershootScrollTime, overshootScrollTime);

		scroller->setScrollerProperties(properties);
	}
	// SPDX-SnippetEnd
}

bool isKineticScrollingBarsHidden()
{
	const desktop::settings::Settings &settings = dpApp().settings();
	return getKineticScrollEnabledGesture(settings).first &&
		   settings.kineticScrollHideBars();
}

}
