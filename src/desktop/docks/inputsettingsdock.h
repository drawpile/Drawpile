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
#include "inputpresetmodel.h"

#include <QDockWidget>
#include <QMenu>
#include <QSettings>

class Ui_InputSettings;

namespace docks {

class InputSettings : public QDockWidget
{
	Q_OBJECT
public:
	explicit InputSettings(input::PresetModel *presetModel, QWidget *parent = 0);
	~InputSettings();

	const input::Preset *currentPreset() const;

private slots:
	void updateSmoothing();
	void updatePressureMapping();
	void choosePreset(int index);
	void addPreset();
	void renamePreset();
	void removePreset();
	void presetDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
	input::Preset *mutableCurrentPreset();

	void applyPresetToUi(const input::Preset &preset);
	void applyUiToPreset(input::Preset &preset) const;
	void updatePresetMenu() const;

	Ui_InputSettings *m_ui;
	input::PresetModel *m_presetModel;
	bool m_updateInProgress;
	QMenu *m_presetmenu;
	QAction *m_addPresetAction;
	QAction *m_renamePresetAction;
	QAction *m_removePresetAction;
};

}

#endif
