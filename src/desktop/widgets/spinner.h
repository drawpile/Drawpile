// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIDGET_SPINNER_H
#define WIDGET_SPINNER_H

#include <QWidget>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

namespace widgets {

class Spinner final : public QWidget {
	Q_OBJECT
	Q_PROPERTY(int dots READ dots WRITE setDots)
public:
	Spinner(QWidget *parent=nullptr);

	int dots() const { return m_dots; }
	void setDots(int dots) { m_dots = qBound(2, dots, 32); }

protected:
	void paintEvent(QPaintEvent *) override;
	void timerEvent(QTimerEvent *) override;

private:
	int m_dots;
	int m_currentDot;
};

}

#endif

