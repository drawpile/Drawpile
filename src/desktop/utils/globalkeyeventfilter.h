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
	void setDockTitleBarsHidden(bool hidden);
	void focusCanvas();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void updateDockTitleBarsHidden(bool hidden);
	static bool hasTextFocus();
	void checkCanvasFocus(QKeyEvent *event);

	bool m_wasHidden;
	long long m_lastAltPress;
	unsigned long long m_lastAltInternalTimestamp;
};

#endif
