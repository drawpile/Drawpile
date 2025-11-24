// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_QTGUICOMPAT_H
#define LIBCLIENT_UTILS_QTGUICOMPAT_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#	include <QEventPoint>
#	include <QInputDevice>
#	include <QEnterEvent>
#else
#	include <QEvent>
#	include <QTouchDevice>
#endif

class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QTextCharFormat;
class QWheelEvent;

namespace compat {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

#	define HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
#	define HAVE_QT_COMPAT_DEFAULT_HIGHDPI_SCALING
#	define HAVE_QT_COMPAT_DEFAULT_DISABLE_WINDOW_CONTEXT_HELP_BUTTON

using DeviceType = QInputDevice::DeviceType;
using EnterEvent = QEnterEvent;
using KeyCombination = QKeyCombination;
using PointerType = QPointingDevice::PointerType;
using TouchPoint = QEventPoint;

constexpr PointerType UnknownPointer = PointerType::Unknown;
constexpr DeviceType NoDevice = DeviceType::Unknown;
constexpr DeviceType FourDMouseDevice = DeviceType::Mouse;
constexpr DeviceType RotationStylusDevice = DeviceType::Stylus;

#else

using DeviceType = QTabletEvent::TabletDevice;
using EnterEvent = QEvent;
using KeyCombination = int;
using PointerType = QTabletEvent::PointerType;
using TouchPoint = QTouchEvent::TouchPoint;

constexpr PointerType UnknownPointer = PointerType::UnknownPointer;
constexpr DeviceType NoDevice = DeviceType::NoDevice;
constexpr DeviceType FourDMouseDevice = DeviceType::FourDMouse;
constexpr DeviceType RotationStylusDevice = DeviceType::RotationStylus;

#endif

QString fontFamily(const QTextCharFormat &format);
void setFontFamily(QTextCharFormat &format, const QString &family);

QPointF wheelPosition(const QWheelEvent &event);

QTabletEvent makeTabletEvent(
	QEvent::Type type, const QPointF &pos, const QPointF &globalPos,
	compat::DeviceType device, compat::PointerType pointerType, qreal pressure,
	int xTilt, int yTilt, qreal tangentialPressure, qreal rotation, int z,
	Qt::KeyboardModifiers keyState, qint64 uniqueID, Qt::MouseButton button,
	Qt::MouseButtons buttons);

KeyCombination keyPressed(const QKeyEvent &event);

QPoint globalPos(const QMouseEvent &event);
QPoint mousePos(const QMouseEvent &event);
QPointF mousePosition(const QMouseEvent &event);

QPointF tabPosF(const QTabletEvent &event);
QPoint tabGlobalPos(const QTabletEvent &event);
int pointerType(const QTabletEvent &event);
bool isEraser(const QTabletEvent &event);

const QList<TouchPoint> &touchPoints(const QTouchEvent &event);

int touchId(const TouchPoint &event);
QPointF touchStartPos(const TouchPoint &event);
QPointF touchPos(const TouchPoint &event);
QPoint touchGlobalPos(const TouchPoint &event);

QPoint dragMovePos(const QDragMoveEvent &event);
QPoint dropPos(const QDropEvent &event);

KeyCombination keyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key);

QString touchDeviceName(const QTouchEvent *event);
int touchDeviceType(const QTouchEvent *event);
bool isTouchPad(const QTouchEvent *event);

DeviceType tabDevice(const QTabletEvent &event);

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#	define COMPAT_FLAGS_ARG(F) (F).toInt()
#else
#	define COMPAT_FLAGS_ARG(F) (F)
#endif

}

#endif
