// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HIDEDOCKTITLEBARSEVENTFILTER_H
#define HIDEDOCKTITLEBARSEVENTFILTER_H

#include <QObject>

class QKeyEvent;

class GlobalKeyEventFilter : public QObject {
	Q_OBJECT
public:
	explicit GlobalKeyEventFilter(QObject *parent = nullptr);

signals:
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	void tabletProximityChanged(bool enter, bool eraser);
	void eraserNear(bool near);
#endif
	void focusCanvas();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void checkCanvasFocus(QKeyEvent *event);
	void updateEraserNear(bool eraserNear);

	long long m_lastAltPress;
	unsigned long long m_lastAltInternalTimestamp;
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	bool m_eraserNear = false;
	bool m_tabletDown = false;
	bool m_anyTabletProximityEventsReceived = false;
	bool m_sawPenTip = false;
	bool m_sawEraserTip = false;
#endif
};

#endif
