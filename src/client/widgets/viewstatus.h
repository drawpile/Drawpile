/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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
class QToolButton;

namespace widgets {

class ViewStatus : public QWidget
{
Q_OBJECT
public:
	ViewStatus(QWidget *parent=0);

	void setZoomActions(QAction *zoomIn, QAction *zoomOut, QAction *zoomOriginal);
	void setRotationActions(QAction *resetRotation);
	void setFlipActions(QAction *flip, QAction *mirror);

public slots:
	void setTransformation(qreal zoom, qreal angle);

signals:
	void zoomChanged(qreal newZoom);
	void angleChanged(qreal newAngle);

private slots:
	void rotateLeft();
	void rotateRight();

private:
	void addZoomShortcut(int zoomLevel);
	void addAngleShortcut(int angle);

	QSlider *_zoomSlider, *_angleSlider;
	QLabel *_zoom, *_angle;
	QToolButton *_zoomIn, *_zoomOut, *_zoomOriginal, *_resetRotation;
	QToolButton *_viewFlip, *_viewMirror;
};

}

#endif
