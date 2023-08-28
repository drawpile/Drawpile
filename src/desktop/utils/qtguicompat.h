// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Drawpile contributors

#ifndef DP_DESKTOP_UTILS_QTGUICOMPAT_H
#define DP_DESKTOP_UTILS_QTGUICOMPAT_H

#include <QApplication>
#include <QtGlobal>
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEventPoint>
#include <QInputDevice>
#endif

namespace compat {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
inline auto fontFamily(const QTextCharFormat &format) {
	return format.fontFamilies().toStringList().value(0, QString{});
}

inline void setFontFamily(QTextCharFormat &format, const QString &family) {
	format.setFontFamilies({ family });
}
#else
inline auto fontFamily(const QTextCharFormat &format) {
	return format.fontFamily();
}

inline void setFontFamily(QTextCharFormat &format, const QString &family) {
	format.setFontFamily(family);
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
inline auto widgetScreen(const QWidget &widget) {
	return widget.screen();
}

inline auto wheelPosition(const QWheelEvent &event) {
	return event.position();
}
#else
inline auto widgetScreen(const QWidget &) {
	return qApp->primaryScreen();
}

inline auto wheelPosition(const QWheelEvent &event) {
	return event.posF();
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
inline auto tabDevice(const QTabletEvent &event) {
	return event.deviceType();
}
#else
inline auto tabDevice(const QTabletEvent &event) {
	return event.device();
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
#define HAVE_QT_COMPAT_DEFAULT_DISABLE_WINDOW_CONTEXT_HELP_BUTTON

using DeviceType = QInputDevice::DeviceType;
using EnterEvent = QEnterEvent;
using PointerType = QPointingDevice::PointerType;
const auto UnknownPointer = PointerType::Unknown;
const auto NoDevice = DeviceType::Unknown;
const auto FourDMouseDevice = DeviceType::Mouse;
const auto RotationStylusDevice = DeviceType::Stylus;

inline auto makeTabletEvent(
	QEvent::Type type,
	const QPointF &pos,
	const QPointF &globalPos,
	compat::DeviceType device,
	compat::PointerType pointerType,
	qreal pressure,
	int xTilt,
	int yTilt,
	qreal tangentialPressure,
	qreal rotation,
	int z,
	Qt::KeyboardModifiers keyState,
	qint64 uniqueID,
	Qt::MouseButton button,
	Qt::MouseButtons buttons
) {
	const QPointingDevice *sysDevice = nullptr;
	for(const auto *candidate : QInputDevice::devices()) {
		if(candidate && candidate->type() == device && candidate->systemId() == uniqueID) {
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

	return ::QTabletEvent(type, sysDevice, pos, globalPos, pressure, xTilt, yTilt, tangentialPressure, rotation, z, keyState, button, buttons);
}

inline auto keyPressed(const QKeyEvent &event) {
	return event.keyCombination();
}

inline auto globalPos(const QMouseEvent &event) {
	return event.globalPosition().toPoint();
}

inline auto mousePos(const QMouseEvent &event) {
	return event.position().toPoint();
}

inline auto mousePosition(const QMouseEvent &event) {
	return event.position();
}

inline auto styleName(const QStyle &style) {
	return style.name();
}

inline auto tabPosF(const QTabletEvent &event) {
	return event.position();
}

inline auto touchPoints(const QTouchEvent &event) {
	return event.points();
}

inline auto touchStartPos(const QEventPoint &event) {
	return event.pressPosition();
}

inline auto touchLastPos(const QEventPoint &event) {
	return event.lastPosition();
}

inline auto touchPos(const QEventPoint &event) {
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
#else
using DeviceType = QTabletEvent::TabletDevice;
using EnterEvent = QEvent;
using PointerType = QTabletEvent::PointerType;
const auto UnknownPointer = PointerType::UnknownPointer;
const auto NoDevice = DeviceType::NoDevice;
const auto FourDMouseDevice = DeviceType::FourDMouse;
const auto RotationStylusDevice = DeviceType::RotationStylus;

inline auto makeTabletEvent(
	QEvent::Type type,
	const QPointF &pos,
	const QPointF &globalPos,
	compat::DeviceType device,
	compat::PointerType pointerType,
	qreal pressure,
	int xTilt,
	int yTilt,
	qreal tangentialPressure,
	qreal rotation,
	int z,
	Qt::KeyboardModifiers keyState,
	qint64 uniqueID,
	Qt::MouseButton button,
	Qt::MouseButtons buttons
) {
	return ::QTabletEvent(type, pos, globalPos, device, pointerType, pressure, xTilt, yTilt, tangentialPressure, rotation, z, keyState, uniqueID, button, buttons);
}

inline auto keyPressed(const QKeyEvent &event) {
	return event.key();
}

inline auto globalPos(const QMouseEvent &event) {
	return event.globalPos();
}

inline auto mousePos(const QMouseEvent &event) {
	return event.pos();
}

inline auto mousePosition(const QMouseEvent &event) {
	return ::QPointF(event.pos());
}

inline auto tabPosF(const QTabletEvent &event) {
	return event.posF();
}

inline auto styleName(const QStyle &style) {
	return style.objectName();
}

inline auto touchPoints(const QTouchEvent &event) {
	return event.touchPoints();
}

inline auto touchStartPos(const QTouchEvent::TouchPoint &event) {
	return event.startPos();
}

inline auto touchLastPos(const QTouchEvent::TouchPoint &event) {
	return event.lastPos();
}

inline auto touchPos(const QTouchEvent::TouchPoint &event) {
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
#endif
}

#endif
