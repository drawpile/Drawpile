#ifndef DP_DESKTOP_UTILS_QTGUICOMPAT_H
#define DP_DESKTOP_UTILS_QTGUICOMPAT_H

#include <QtGlobal>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QTextCharFormat>
#include <QTouchEvent>
#include <QWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEventPoint>
#include <QInputDevice>
#endif

namespace compat {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
inline auto fontFamily(const QTextCharFormat &format) {
	return format.fontFamilies().toStringList().first();
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
#else
inline auto widgetScreen(const QWidget &) {
	return qApp->primaryScreen();
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

using DeviceType = QInputDevice::DeviceType;
using EnterEvent = QEnterEvent;
using PointerType = QPointingDevice::PointerType;

inline auto keyPressed(const QKeyEvent &event) {
	return event.keyCombination();
}

inline auto globalPos(const QMouseEvent &event) {
	return event.globalPosition().toPoint();
}

inline auto mousePos(const QMouseEvent &event) {
	return event.position().toPoint();
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
#else
using DeviceType = QTabletEvent;
using EnterEvent = QEvent;
using PointerType = QTabletEvent;

inline auto keyPressed(const QKeyEvent &event) {
	return event.key();
}

inline auto globalPos(const QMouseEvent &event) {
	return event.globalPos();
}

inline auto mousePos(const QMouseEvent &event) {
	return event.pos();
}

inline auto tabPosF(const QTabletEvent &event) {
	return event.posF();
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
#endif
}

#endif
