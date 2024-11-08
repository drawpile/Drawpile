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
	explicit DockBase(
		const QString &fullTitle, const QString &shortTitle,
		QWidget *parent = nullptr);

	const QString &fullTitle() const { return m_fullTitle; }

#ifdef Q_OS_MACOS
private slots:
	void addWindowDecorations(bool topLevel);
#endif

private:
#ifdef Q_OS_MACOS
	void initConnection();
#endif

	QString m_fullTitle;
};

}

#endif
