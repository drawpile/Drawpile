/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef InputSettings_H
#define InputSettings_H

#include <QDockWidget>

class Ui_InputSettings;

namespace widgets {
	class CanvasView;
}

namespace docks {

class CanvasView;

class InputSettings : public QDockWidget
{
	Q_OBJECT
public:
	explicit InputSettings(QWidget *parent = 0);
	~InputSettings();

	void connectCanvasView(widgets::CanvasView *view);

private slots:
	void updateFakePressureMode();

private:
	Ui_InputSettings *_ui;
	widgets::CanvasView *_canvasview;
};

}

#endif
