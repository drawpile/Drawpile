// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABLETTEST_WIDGET_H
#define TABLETTEST_WIDGET_H

#include "libclient/canvas/point.h"
#include <QVector>
#include <QWidget>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

namespace widgets {

class QDESIGNER_WIDGET_EXPORT TabletTester final : public QWidget {
	Q_OBJECT
public:
	TabletTester(QWidget *parent=nullptr);

public slots:
	void clear();

signals:
	void eventReport(const QString &msg);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void tabletEvent(QTabletEvent *e) override;

private:
	QVector<QPointF> m_mousePath;
	QVector<canvas::Point> m_tabletPath;

	bool m_mouseDown;
	bool m_tabletDown;
};

}

#endif

