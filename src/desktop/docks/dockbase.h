// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_DOCKBASE_H
#define DESKTOP_DOCKS_DOCKBASE_H
#include <QDockWidget>
#include <QIcon>

class QTabBar;

namespace docks {

class DockBase : public QDockWidget {
	Q_OBJECT
public:
	explicit DockBase(
		const QString &fullTitle, const QString &shortTitle,
		const QIcon &tabIcon, QWidget *parent = nullptr);

	const QString &fullTitle() const { return m_fullTitle; }
	const QIcon &tabIcon() const { return m_tabIcon; }

	void setShowIcons(bool showIcons);

	void makeTabCurrent(bool toggled);

	QWidget *actualTitleBarWidget() const;
	void setArrangeMode(bool arrangeMode);

signals:
	void arrangingFinished();
	void tabUpdateRequested();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
#ifdef Q_OS_MACOS
	void initConnection();
	void addWindowDecorations(bool topLevel);
#endif
	void handleTopLevelChanged(bool floating);
	void adjustTitle(bool floating);
	void fixViewToggleActionTitle(const QString &title);
	QTabBar *searchTab(int &outIndex);

	QString m_fullTitle;
	QString m_shortTitle;
	QIcon m_tabIcon;
	QWidget *m_originalTitleBarWidget = nullptr;
	bool m_showIcons = false;
};

}

#endif
