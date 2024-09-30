#include "desktop/utils/wineventfilter.h"
#include "desktop/scene/canvasview.h"
#include "desktop/view/canvasview.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <windows.h>

bool WinEventFilter::nativeEventFilter(
	const QByteArray &eventType, void *message,
	compat::NativeEventResult result)
{
	Q_UNUSED(result);
	if(eventType == QByteArrayLiteral("windows_generic_MSG")) {
		MSG *msg = static_cast<MSG *>(message);
		switch(msg->message) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			return handleKeyDown(message);
		case WM_KEYUP:
		case WM_SYSKEYUP:
			return handleKeyUp(message);
		default:
			break;
		}
	}
	return false;
}

bool WinEventFilter::handleKeyDown(void *message)
{
	MSG *msg = static_cast<MSG *>(message);
	if(LOWORD(msg->wParam) == VK_SPACE) {
		WORD flags = HIWORD(msg->lParam);
		if((flags & KF_ALTDOWN) == KF_ALTDOWN) {
			QObject *target = QGuiApplication::focusObject();
			if(qobject_cast<widgets::CanvasView *>(target) ||
			   qobject_cast<view::CanvasView *>(target)) {
				bool autorepeat = (flags & KF_REPEAT) == KF_REPEAT;
				QKeyEvent event(
					QKeyEvent::KeyPress, Qt::Key_Space, Qt::AltModifier,
					QStringLiteral(" "), autorepeat, LOWORD(msg->lParam));
				QGuiApplication::sendEvent(target, &event);
				m_keyDownSent = true;
				return true;
			}
		}
	}
	return false;
}

bool WinEventFilter::handleKeyUp(void *message)
{
	MSG *msg = static_cast<MSG *>(message);
	if(m_keyDownSent) {
		WORD vk = LOWORD(msg->wParam);
		bool isSpace = vk == VK_SPACE;
		bool isAlt = vk == VK_MENU;
		if(isSpace || isAlt) {
			QObject *target = QGuiApplication::focusObject();
			if(target) {
				QKeyEvent event(
					QKeyEvent::KeyRelease, Qt::Key_Space,
					isAlt ? Qt::AltModifier : Qt::NoModifier);
				QGuiApplication::sendEvent(target, &event);
				m_keyDownSent = false;
			}
		}
	}
	return false;
}
