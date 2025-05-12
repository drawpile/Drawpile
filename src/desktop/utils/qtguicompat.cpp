// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QTextCharFormat>
#include <QWheelEvent>
#include <QWidget>

namespace compat {

QString fontFamily(const QTextCharFormat &format)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	return format.fontFamilies().toStringList().value(0, QString());
#else
	return format.fontFamily();
#endif
}

void setFontFamily(QTextCharFormat &format, const QString &family)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	format.setFontFamilies({family});
#else
	format.setFontFamily(family);
#endif
}

QScreen *widgetScreen(const QWidget &widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	return widget.screen();
#else
	Q_UNUSED(widget);
	return qApp->primaryScreen();
#endif
}

QPointF wheelPosition(const QWheelEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	return event.position();
#else
	return event.posF();
#endif
}

QTabletEvent makeTabletEvent(
	QEvent::Type type, const QPointF &pos, const QPointF &globalPos,
	compat::DeviceType device, compat::PointerType pointerType, qreal pressure,
	int xTilt, int yTilt, qreal tangentialPressure, qreal rotation, int z,
	Qt::KeyboardModifiers keyState, qint64 uniqueID, Qt::MouseButton button,
	Qt::MouseButtons buttons)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QPointingDevice *sysDevice = nullptr;
	for(const QInputDevice *candidate : QInputDevice::devices()) {
		if(candidate && candidate->type() == device &&
		   candidate->systemId() == uniqueID) {
			const QPointingDevice *pd =
				static_cast<const QPointingDevice *>(candidate);
			if(pd->pointerType() == pointerType) {
				sysDevice = pd;
				break;
			}
		}
	}

	// Happens with KisTablet. Falling back to the primary device seems to work
	// fine without any negative repercussions, leaving it null causes a crash.
	if(!sysDevice) {
		qDebug("Could not find device matching event ID %lld", uniqueID);
		sysDevice = QPointingDevice::primaryPointingDevice();
	}

	return QTabletEvent(
		type, sysDevice, pos, globalPos, pressure, xTilt, yTilt,
		tangentialPressure, rotation, z, keyState, button, buttons);
#else
	return QTabletEvent(
		type, pos, globalPos, device, pointerType, pressure, xTilt, yTilt,
		tangentialPressure, rotation, z, keyState, uniqueID, button, buttons);
#endif
}

KeyCombination keyPressed(const QKeyEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.keyCombination();
#else
	return event.key();
#endif
}

QPoint globalPos(const QMouseEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.globalPosition().toPoint();
#else
	return event.globalPos();
#endif
}

QPoint mousePos(const QMouseEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position().toPoint();
#else
	return event.pos();
#endif
}

QPointF mousePosition(const QMouseEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position();
#else
	return QPointF(event.pos());
#endif
}

QString styleName(const QStyle &style)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return style.name();
#else
	return style.objectName();
#endif
}

QPointF tabPosF(const QTabletEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position();
#else
	return event.posF();
#endif
}

const QList<TouchPoint> &touchPoints(const QTouchEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.points();
#else
	return event.touchPoints();
#endif
}

int touchId(const TouchPoint &event)
{
	return event.id();
}

QPointF touchStartPos(const TouchPoint &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.pressPosition();
#else
	return event.startPos();
#endif
}

QPointF touchPos(const TouchPoint &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position();
#else
	return event.pos();
#endif
}

QPoint dragMovePos(const QDragMoveEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position().toPoint();
#else
	return event.pos();
#endif
}

QPoint dropPos(const QDropEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event.position().toPoint();
#else
	return event.pos();
#endif
}

KeyCombination keyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return KeyCombination(modifiers, key);
#else
	return KeyCombination(modifiers) | KeyCombination(key);
#endif
}

QString touchDeviceName(const QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QInputDevice *device = event->device();
#else
	const QTouchDevice *device = event->device();
#endif
	return device ? device->name() : QString();
}

int touchDeviceType(const QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return int(event->deviceType());
#else
	QTouchDevice *device = event->device();
	return device ? int(device->type()) : -1;
#endif
}

bool isTouchPad(const QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event->deviceType() == QInputDevice::DeviceType::TouchPad;
#else
	return touchDeviceType(event) == int(QTouchDevice::TouchPad);
#endif
}

DeviceType tabDevice(const QTabletEvent &event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	return event.deviceType();
#else
	return event.device();
#endif
}

}
