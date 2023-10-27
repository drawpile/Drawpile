#ifndef DESKTOP_UTILS_WINEVENTFILTER_H
#define DESKTOP_UTILS_WINEVENTFILTER_H
#include "libshared/util/qtcompat.h"
#include <QAbstractNativeEventFilter>

class WinEventFilter : public QAbstractNativeEventFilter {
public:
	bool nativeEventFilter(
		const QByteArray &eventType, void *message,
		compat::NativeEventResult result) override;

private:
	bool handleKeyDown(void *message);
	bool handleKeyUp(void *message);

	bool m_keyDownSent = false;
};

#endif
