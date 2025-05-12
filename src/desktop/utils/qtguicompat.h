// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_QTGUICOMPAT_H
#define DESKTOP_UTILS_QTGUICOMPAT_H
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
class QScreen;
class QStyle;
class QTextCharFormat;
class QWheelEvent;
class QWidget;

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

QScreen *widgetScreen(const QWidget &widget);

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

QString styleName(const QStyle &style);

QPointF tabPosF(const QTabletEvent &event);

const QList<TouchPoint> &touchPoints(const QTouchEvent &event);

int touchId(const TouchPoint &event);
QPointF touchStartPos(const TouchPoint &event);
QPointF touchPos(const TouchPoint &event);

QPoint dragMovePos(const QDragMoveEvent &event);
QPoint dropPos(const QDropEvent &event);

KeyCombination keyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key);

QString touchDeviceName(const QTouchEvent *event);
int touchDeviceType(const QTouchEvent *event);
bool isTouchPad(const QTouchEvent *event);

DeviceType tabDevice(const QTabletEvent &event);

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
