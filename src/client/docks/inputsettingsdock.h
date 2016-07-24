/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "canvas/pressure.h"

#include <QDockWidget>

class Ui_InputSettings;

namespace docks {

class InputSettings : public QDockWidget
{
	Q_PROPERTY(PressureMapping pressureMapping READ getPressureMapping NOTIFY pressureMappingChanged)
	Q_PROPERTY(int smoothing READ getSmoothing NOTIFY smoothingChanged)

	Q_OBJECT
public:
	explicit InputSettings(QWidget *parent = 0);
	~InputSettings();

	int getSmoothing() const;
	PressureMapping getPressureMapping() const;

signals:
	void smoothingChanged(int value);
	void pressureMappingChanged(const PressureMapping &mapping);

private slots:
	void updatePressureMapping();

private:
	Ui_InputSettings *m_ui;
};

}

#endif
