// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HIDEDOCKTITLEBARSEVENTFILTER_H
#define HIDEDOCKTITLEBARSEVENTFILTER_H

#include <QObject>

class HideDockTitleBarsEventFilter : public QObject {
	Q_OBJECT
public:
	explicit HideDockTitleBarsEventFilter(QObject *parent = nullptr);

signals:
	void setDockTitleBarsHidden(bool hidden);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void checkDockTitleBarsHidden(Qt::KeyboardModifiers mods);

	bool m_wasHidden;
};

#endif
