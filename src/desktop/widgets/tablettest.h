/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TABLETTEST_WIDGET_H
#define TABLETTEST_WIDGET_H

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
	QPolygon m_mousePath;
	QPolygon m_tabletPath;

	bool m_mouseDown;
	bool m_tabletDown;
};

}

#endif

