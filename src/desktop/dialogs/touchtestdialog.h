// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_TOUCHTESTDIALOG_H
#define DESKTOP_DIALOGS_TOUCHTESTDIALOG_H
#include <QDialog>
#include <QGraphicsView>

namespace dialogs {

class TouchTestView final : public QGraphicsView {
	Q_OBJECT
public:
	static constexpr int MODE_TOUCH = 0;
	static constexpr int MODE_GESTURE = 1;

	explicit TouchTestView(QWidget *parent = nullptr);

	bool touchEventsEnabled() const { return m_touchEventsEnabled; }
	void setTouchEventsEnabled(bool enabled);

	bool gestureEventsEnabled() const { return m_gestureEventsEnabled; }
	void setGestureEventsEnabled(bool enabled);

protected:
	bool viewportEvent(QEvent *event) override;

signals:
	void logEvent(const QString &message);

private:
	bool m_touchEventsEnabled = false;
	bool m_gestureEventsEnabled = false;
	QGraphicsScene *m_scene;
};

class TouchTestDialog final : public QDialog {
	Q_OBJECT
public:
	explicit TouchTestDialog(QWidget *parent = nullptr);
};

}

#endif
