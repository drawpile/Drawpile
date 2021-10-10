/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include <QDialog>
#include <QMenu>

class Ui_InputSettings;

namespace input {
	class PresetModel;
	struct Preset;
}

namespace dialogs {

class InputSettings : public QDialog
{
	Q_OBJECT
public:
	explicit InputSettings(QWidget *parent=nullptr);
	~InputSettings();

	void setCurrentPreset(const QString &id);
	const input::Preset *currentPreset() const;

signals:
	void currentIndexChanged(int index);
	void activePresetModified(const input::Preset *preset);

public slots:
	void setCurrentIndex(int index);

private slots:
	void choosePreset(int index);
	void presetNameChanged(const QString &name);
	void addPreset();
	void copyPreset();
	void removePreset();
	void onPresetCountChanged();

private:
	input::Preset *mutableCurrentPreset();

	void applyPresetToUi(const input::Preset &preset);
	void applyUiToPreset();
	void updateModeUi(PressureMapping::Mode mode);

	Ui_InputSettings *m_ui;
	input::PresetModel *m_presetModel;
	bool m_updateInProgress;
	bool m_indexChangeInProgress;
	QMenu *m_presetmenu;
	QAction *m_removePresetAction;
};

}

#endif
