// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef InputSettings_H
#define InputSettings_H

#include "libclient/canvas/pressure.h"

#include <QDialog>
#include <QMenu>

class Ui_InputSettings;

namespace input {
	class PresetModel;
	struct Preset;
}

namespace dialogs {

class InputSettings final : public QDialog
{
	Q_OBJECT
public:
	explicit InputSettings(QWidget *parent=nullptr);
	~InputSettings() override;

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
