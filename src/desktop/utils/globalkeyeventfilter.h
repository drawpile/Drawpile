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
	void focusCanvas();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void checkCanvasFocus(QKeyEvent *event);

	long long m_lastAltPress;
	unsigned long long m_lastAltInternalTimestamp;
};

#endif
