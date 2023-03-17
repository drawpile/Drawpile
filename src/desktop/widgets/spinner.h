/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

