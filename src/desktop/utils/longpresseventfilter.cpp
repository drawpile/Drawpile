// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/longpresseventfilter.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/qtguicompat.h"
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QScroller>
#include <QStyleHints>
#include <QTimer>
#ifdef Q_OS_ANDROID
#	include <libshared/util/androidutils.h>
#endif


LongPressEventFilter::LongPressEventFilter(QObject *parent)
	: QObject(parent)
{
	m_timer = new QTimer(this);
	m_timer->setTimerType(Qt::CoarseTimer);
	m_timer->setSingleShot(true);
	connect(
		m_timer, &QTimer::timeout, this,
		&LongPressEventFilter::triggerLongPress);
	dpApp().settings().bindKineticScrollGesture(
		this, &LongPressEventFilter::setKineticScrollGesture);
#ifdef Q_OS_ANDROID
	m_longPressTimeout = utils::androidLongPressTimeout();
#endif
}

bool LongPressEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	if(!m_handlingEvent) {
		QScopedValueRollback rollback(m_handlingEvent, true);
		switch(event->type()) {
		case QEvent::MouseButtonPress:
			if(handleMousePress(
				   qobject_cast<QWidget *>(watched),
				   static_cast<QMouseEvent *>(event))) {
				event->setAccepted(true);
				return true;
			}
			break;
		case QEvent::MouseMove:
			if(handleMouseMove(static_cast<QMouseEvent *>(event))) {
				return true;
			}
			break;
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonRelease:
			flush();
			break;
		default:
			break;
		}
	}
	return QObject::eventFilter(watched, event);
}

bool LongPressEventFilter::handleMousePress(
	QWidget *target, const QMouseEvent *me)
{
	if(me->buttons() == Qt::LeftButton && me->modifiers() == Qt::NoModifier &&
	   isContextMenuTarget(target)) {
		const QStyleHints *sh = qApp->styleHints();
		long long distance = qMax(MINIMUM_DISTANCE, sh->startDragDistance());
		m_distanceSquared = distance * distance;
		m_pressLocalPos = compat::mousePos(*me);
		m_pressGlobalPos = compat::globalPos(*me);
		// Kinetic scrolling may have already delayed the input, so subtract
		// that from the intended delay to avoid waiting for it twice. This may
		// end up with a delay of zero, but that's okay. Also, the user may have
		// already dragged far enough for the cursor to be outside of the
		// long-press distance (it only checks the axes that are scrollable), in
		// which case we bail out here.
		int kineticScrollDelay = getKineticScrollDelay(target);
		if(kineticScrollDelay == 0 || isWithinDistance(QCursor::pos())) {
			m_target = target;
#ifdef Q_OS_ANDROID
			int longPressInterval = m_longPressTimeout;
#else
			int longPressInterval = sh->mousePressAndHoldInterval();
#endif
			if(longPressInterval < MINIMUM_DELAY) {
				longPressInterval = MINIMUM_DELAY;
			}
			// Kinetic scrolling may have already delayed the input, so subtract
			// that from the intended delay to avoid waiting for it twice. This
			// may end up with a delay of zero, but that's okay.
			m_timer->start(qMax(0, longPressInterval - kineticScrollDelay));
			return true;
		}
	}
	cancel();
	return false;
}

bool LongPressEventFilter::handleMouseMove(const QMouseEvent *me)
{
	if(m_timer->isActive()) {
		if(isWithinDistance(compat::globalPos(*me))) {
			return true;
		} else {
			flush();
		}
	}
	return false;
}

void LongPressEventFilter::flush()
{
	QWidget *target = m_target.data();
	cancel();
	if(target && target->isVisible()) {
		QMouseEvent event(
			QEvent::MouseButtonPress, m_pressLocalPos, m_pressGlobalPos,
			Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
		qApp->sendEvent(target, &event);
	}
}

void LongPressEventFilter::cancel()
{
	m_timer->stop();
	m_target.clear();
}

bool LongPressEventFilter::isWithinDistance(const QPoint &globalPos) const
{
	long long x = globalPos.x() - m_pressGlobalPos.x();
	long long y = globalPos.y() - m_pressGlobalPos.y();
	return (x * x) + (y * y) <= m_distanceSquared;
}

void LongPressEventFilter::triggerLongPress()
{
	QWidget *target = m_target.data();
	cancel();
	if(!m_handlingEvent && isContextMenuTarget(target)) {
		QScopedValueRollback rollback(m_handlingEvent, true);
		// First we synchronously send a right click press and release so that
		// the target widget can update its state correctly. Afterwards we post
		// the context menu event to it so that'll open. As far as I can tell,
		// that's what Qt does when you right-click on a widget as well.
		{
			QMouseEvent pressEvent(
				QEvent::MouseButtonPress, m_pressLocalPos, m_pressGlobalPos,
				Qt::RightButton, Qt::RightButton, Qt::NoModifier);
			qApp->sendEvent(target, &pressEvent);
		}
		{
			QMouseEvent releaseEvent(
				QEvent::MouseButtonRelease, m_pressLocalPos, m_pressGlobalPos,
				Qt::RightButton, Qt::NoButton, Qt::NoModifier);
			qApp->sendEvent(target, &releaseEvent);
		}
		qApp->postEvent(
			target,
			new QContextMenuEvent(
				QContextMenuEvent::Mouse, m_pressLocalPos, m_pressGlobalPos));
	}
}

void LongPressEventFilter::setKineticScrollGesture(int kineticScrollGesture)
{
	m_kineticScrollGesture = kineticScrollGesture;
}

int LongPressEventFilter::getKineticScrollDelay(QWidget *target) const
{
	// QScroller has no sane way of retrieving the gesture type, it can only
	// tell you which Qt::Gesture was captured, which is completely useless.
	// So we instead grab it from the settings, that should match anyway.
	switch(m_kineticScrollGesture) {
	case int(desktop::settings::KineticScrollGesture::Touch):
	case int(desktop::settings::KineticScrollGesture::LeftClick):
		break;
	default:
		return 0;
	}

	if(!QScroller::hasScroller(target)) {
		return 0;
	}

	const QScroller *scroller = QScroller::scroller(target);
	// Shouldn't happen, since we asked with hasScroller, but let's be sure.
	if(!scroller) {
		return 0;
	}

#ifdef DRAWPILE_QSCROLLER_PATCH
	QVariant filterVariant = target->property("drawpile_scroller_filter");
	if(filterVariant.isValid()) {
		DrawpileQScrollerFilter *filter =
			filterVariant.value<DrawpileQScrollerFilter *>();
		if(filter && filter->wasScrollFiltered()) {
			return 0;
		}
	}
#endif

	qreal pressDelay =
		scroller->scrollerProperties()
			.scrollMetric(QScrollerProperties::MousePressEventDelay)
			.toReal();
	if(pressDelay < 0.0) {
		return 0;
	}

	return int(pressDelay * 1000.0);
}

bool LongPressEventFilter::isContextMenuTarget(QWidget *target)
{
	while(target && target->isVisible() && isLongPressableWidget(target)) {
		switch(target->contextMenuPolicy()) {
		case Qt::NoContextMenu:
			target = target->parentWidget();
			break;
		case Qt::PreventContextMenu:
			return false;
		default:
			return true;
		}
	}
	return false;
}

bool LongPressEventFilter::isLongPressableWidget(QWidget *target)
{
	QVariant prop = target->property(ENABLED_PROPERTY);
	if(prop.isValid()) {
		return prop.toBool();
	}

	// Several widget types don't have desirable long-press behavior. If they're
	// our own widgets, we should just fix that in the widget, but we can"t
	// really sensibly do that to Qt's widgets. Luckily however, those widgets
	// really shouldn't have context menus in the first place, so we can just
	// disregard them as long-press targets and be done with it.

	// Buttons get depressed when you click and hold on them and won't pop back
	// up if a context menu is opened during that. They may also have a menu
	// attached to them that the user operates like a context menu.
	if(qobject_cast<QAbstractButton *>(target)) {
		return false;
	}

	// Slightly non-obvious, but this thing has many child classes, like text
	// areas or the canvas, none of which have sensible context menus.
	if(qobject_cast<QAbstractScrollArea *>(target)) {
		return false;
	}

	// Sliders (including scrollbars) may get dragged slowly.
	if(qobject_cast<QAbstractSlider *>(target)) {
		return false;
	}

	// Spinners may get held down to keep a number spinning.
	if(qobject_cast<QAbstractSpinBox *>(target)) {
		return false;
	}

	// Combo boxes get held down to pick an item.
	if(qobject_cast<QComboBox *>(target)) {
		return false;
	}

	// Text editing will have its own long-press handling.
	if(qobject_cast<QLineEdit *>(target)) {
		return false;
	}

	// Menus don't have context menus.
	if(qobject_cast<QMenu *>(target)) {
		return false;
	}

	// Menu bars don't have context menus either.
	if(qobject_cast<QMenuBar *>(target)) {
		return false;
	}

	return true;
}
