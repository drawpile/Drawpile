// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DOCKBASE_H
#define DESKTOP_DOCKBASE_H

#include <QDockWidget>

namespace docks {

// Base class for all of our docks because on macOS you can't resize floating
// docks unless you turn them into regular windows when they start floating.
class DockBase : public QDockWidget {
	Q_OBJECT
public:
	explicit DockBase(QWidget *parent = nullptr);
	explicit DockBase(const QString title, QWidget *parent = nullptr);

#ifdef Q_OS_MACOS
private slots:
	void addWindowDecorations(bool topLevel);

private:
	void initConnection();
#endif
};

}

#endif
