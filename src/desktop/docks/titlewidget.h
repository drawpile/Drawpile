// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include <QDockWidget>

class QBoxLayout;
class QMenu;

namespace widgets{
class GroupedToolButton;
}

namespace docks {

class TitleWidget final : public QWidget
{
	Q_OBJECT
public:
	explicit TitleWidget(QDockWidget *parent = nullptr);

	void addCustomWidget(QWidget *widget, bool stretch=false);
	void addSpace(int space);
	void addStretch(int stretch=0);

	/// Add a spacer to the left side to center the custom widgets
	void addCenteringSpacer();

	void addGlobalDockActions(const QList<QAction *> &actions);

	// Make the title bar button on the right always take up space. Useful for
	// widgets that want to put something in the center and don't want it messed
	// up by the button disappearing.
	void setKeepButtonSpace(bool keepButtonSpace);

private slots:
	void toggleFloating();
	void toggleDockable();
	void updateContextMenuActions();
	void onFeaturesChanged(QDockWidget::DockWidgetFeatures features);
	void onDockLocationChanged(Qt::DockWidgetArea area);

private:
	QBoxLayout *m_layout;
	widgets::GroupedToolButton *m_button;
	QMenu *m_menu;
	QAction *m_dockAction;
	QAction *m_dockableAction;
	QAction *m_closeAction;
};

}

#endif // TITLEWIDGET_H
