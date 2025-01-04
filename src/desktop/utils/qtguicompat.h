// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_DESKTOP_UTILS_QTGUICOMPAT_H
#define DP_DESKTOP_UTILS_QTGUICOMPAT_H
#include <QApplication>
#include <QCheckBox>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QTabletEvent>
#include <QTextCharFormat>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#	include <QEventPoint>
#	include <QImageReader>
#	include <QInputDevice>
#else
#	include <QTouchDevice>
#endif

namespace compat {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
inline auto fontFamily(const QTextCharFormat &format)
{
	return format.fontFamilies().toStringList().value(0, QString{});
}

inline void setFontFamily(QTextCharFormat &format, const QString &family)
{
	format.setFontFamilies({family});
}
#else
inline auto fontFamily(const QTextCharFormat &format)
{
	return format.fontFamily();
}

inline void setFontFamily(QTextCharFormat &format, const QString &family)
{
	format.setFontFamily(family);
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
inline auto widgetScreen(const QWidget &widget)
{
	return widget.screen();
}

inline auto wheelPosition(const QWheelEvent &event)
{
	return event.position();
}
#else
inline auto widgetScreen(const QWidget &)
{
	return qApp->primaryScreen();
}

inline auto wheelPosition(const QWheelEvent &event)
{
	return event.posF();
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
inline auto tabDevice(const QTabletEvent &event)
{
	return event.deviceType();
}
#else
inline auto tabDevice(const QTabletEvent &event)
{
	return event.device();
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#	define HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
#	define HAVE_QT_COMPAT_DEFAULT_HIGHDPI_SCALING
#	define HAVE_QT_COMPAT_DEFAULT_DISABLE_WINDOW_CONTEXT_HELP_BUTTON

using DeviceType = QInputDevice::DeviceType;
using EnterEvent = QEnterEvent;
using PointerType = QPointingDevice::PointerType;
using TouchPoint = QEventPoint;
constexpr auto UnknownPointer = PointerType::Unknown;
constexpr auto NoDevice = DeviceType::Unknown;
constexpr auto FourDMouseDevice = DeviceType::Mouse;
constexpr auto RotationStylusDevice = DeviceType::Stylus;

inline auto makeTabletEvent(
	QEvent::Type type, const QPointF &pos, const QPointF &globalPos,
	compat::DeviceType device, compat::PointerType pointerType, qreal pressure,
	int xTilt, int yTilt, qreal tangentialPressure, qreal rotation, int z,
	Qt::KeyboardModifiers keyState, qint64 uniqueID, Qt::MouseButton button,
	Qt::MouseButtons buttons)
{
	const QPointingDevice *sysDevice = nullptr;
	for(const auto *candidate : QInputDevice::devices()) {
		if(candidate && candidate->type() == device &&
		   candidate->systemId() == uniqueID) {
			const auto *pd = static_cast<const QPointingDevice *>(candidate);
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

	return ::QTabletEvent(
		type, sysDevice, pos, globalPos, pressure, xTilt, yTilt,
		tangentialPressure, rotation, z, keyState, button, buttons);
}

inline auto keyPressed(const QKeyEvent &event)
{
	return event.keyCombination();
}

inline auto globalPos(const QMouseEvent &event)
{
	return event.globalPosition().toPoint();
}

inline auto mousePos(const QMouseEvent &event)
{
	return event.position().toPoint();
}

inline auto mousePosition(const QMouseEvent &event)
{
	return event.position();
}

inline auto styleName(const QStyle &style)
{
	return style.name();
}

inline auto tabPosF(const QTabletEvent &event)
{
	return event.position();
}

inline const QList<TouchPoint> &touchPoints(const QTouchEvent &event)
{
	return event.points();
}

inline auto touchStartPos(const QEventPoint &event)
{
	return event.pressPosition();
}

inline auto touchPos(const QEventPoint &event)
{
	return event.position();
}

inline auto dragMovePos(const QDragMoveEvent &event)
{
	return event.position().toPoint();
}

inline auto dropPos(const QDropEvent &event)
{
	return event.position().toPoint();
}

inline auto keyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key)
{
	return QKeyCombination{modifiers, key};
}

inline QString touchDeviceName(const QTouchEvent *event)
{
	const QInputDevice *device = event->device();
	return device ? device->name() : QString();
}

inline int touchDeviceType(const QTouchEvent *event)
{
	return int(event->deviceType());
}

inline bool isTouchPad(const QTouchEvent *event)
{
	return event->deviceType() == QInputDevice::DeviceType::TouchPad;
}

inline void disableImageReaderAllocationLimit()
{
	QImageReader::setAllocationLimit(0);
}

#else
using DeviceType = QTabletEvent::TabletDevice;
using EnterEvent = QEvent;
using PointerType = QTabletEvent::PointerType;
using TouchPoint = QTouchEvent::TouchPoint;
constexpr auto UnknownPointer = PointerType::UnknownPointer;
constexpr auto NoDevice = DeviceType::NoDevice;
constexpr auto FourDMouseDevice = DeviceType::FourDMouse;
constexpr auto RotationStylusDevice = DeviceType::RotationStylus;

inline auto makeTabletEvent(
	QEvent::Type type, const QPointF &pos, const QPointF &globalPos,
	compat::DeviceType device, compat::PointerType pointerType, qreal pressure,
	int xTilt, int yTilt, qreal tangentialPressure, qreal rotation, int z,
	Qt::KeyboardModifiers keyState, qint64 uniqueID, Qt::MouseButton button,
	Qt::MouseButtons buttons)
{
	return ::QTabletEvent(
		type, pos, globalPos, device, pointerType, pressure, xTilt, yTilt,
		tangentialPressure, rotation, z, keyState, uniqueID, button, buttons);
}

inline auto keyPressed(const QKeyEvent &event)
{
	return event.key();
}

inline auto globalPos(const QMouseEvent &event)
{
	return event.globalPos();
}

inline auto mousePos(const QMouseEvent &event)
{
	return event.pos();
}

inline auto mousePosition(const QMouseEvent &event)
{
	return ::QPointF(event.pos());
}

inline auto tabPosF(const QTabletEvent &event)
{
	return event.posF();
}

inline auto styleName(const QStyle &style)
{
	return style.objectName();
}

inline const QList<TouchPoint> &touchPoints(const QTouchEvent &event)
{
	return event.touchPoints();
}

inline auto touchStartPos(const QTouchEvent::TouchPoint &event)
{
	return event.startPos();
}

inline auto touchPos(const QTouchEvent::TouchPoint &event)
{
	return event.pos();
}

inline auto dragMovePos(const QDragMoveEvent &event)
{
	return event.pos();
}

inline auto dropPos(const QDropEvent &event)
{
	return event.pos();
}

inline auto keyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key)
{
	return int(modifiers) | int(key);
}

inline QString touchDeviceName(const QTouchEvent *event)
{
	QTouchDevice *device = event->device();
	return device ? device->name() : QString();
}

inline int touchDeviceType(const QTouchEvent *event)
{
	QTouchDevice *device = event->device();
	return device ? int(device->type()) : -1;
}

inline bool isTouchPad(const QTouchEvent *event)
{
	return touchDeviceType(event) == int(QTouchDevice::TouchPad);
}

inline void disableImageReaderAllocationLimit()
{
	// Nothing to do, Qt 5 doesn't have this limit.
}
#endif

// Do not attempt to replace these #defines with something more C++ish.
// That gets miscompiled on MSVC and your signals will not connect.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
using CheckBoxState = Qt::CheckState;
#	define COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(CLS) &CLS::checkStateChanged
#else
using CheckBoxState = int;
#	define COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(CLS) &CLS::stateChanged
#endif
}

#endif
