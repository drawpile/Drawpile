// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_DOCKBASE_H
#define DESKTOP_DOCKS_DOCKBASE_H
#include <QDockWidget>

class QTabBar;

namespace docks {

class DockBase : public QDockWidget {
	Q_OBJECT
public:
	explicit DockBase(
		const QString &fullTitle, const QString &shortTitle,
		QWidget *parent = nullptr);

	const QString &fullTitle() const { return m_fullTitle; }

	void makeTabCurrent(bool toggled);

#ifdef Q_OS_MACOS
private slots:
	void addWindowDecorations(bool topLevel);
#endif

private:
#ifdef Q_OS_MACOS
	void initConnection();
#endif
	QTabBar *searchTab(int &outIndex);

	QString m_fullTitle;
};

}

#endif
