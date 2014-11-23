/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#ifndef VIEWSTATUS_H
#define VIEWSTATUS_H

#include <QWidget>

class QLabel;
class QSlider;

namespace widgets {

class ViewStatus : public QWidget
{
Q_OBJECT
public:
	ViewStatus(QWidget *parent=0);

public slots:
	void setTransformation(qreal zoom, qreal angle);

signals:
	void zoomChanged(qreal newZoom);
	void angleChanged(qreal newAngle);

private:
	void addZoomShortcut(int zoomLevel);
	void addAngleShortcut(int angle);

	QSlider *_zoomSlider, *_angleSlider;
	QLabel *_zoom, *_angle;
};

}

#endif
