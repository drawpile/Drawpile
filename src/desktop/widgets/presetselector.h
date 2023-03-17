/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#ifndef PRESET_SELECTOR_WIDGET_H
#define PRESET_SELECTOR_WIDGET_H

#include <QWidget>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

class QComboBox;
class QPushButton;
class QStringListModel;
class QDir;

namespace widgets {

class QDESIGNER_WIDGET_EXPORT PresetSelector final : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QString presetFolder READ presetFolder WRITE setPresetFolder)
	Q_PROPERTY(bool writeOnly READ writeOnly WRITE setWriteOnly)
public:
	PresetSelector(QWidget *parent=nullptr);

	QString presetFolder() const { return m_folder; }
	bool writeOnly() const { return m_writeOnly; }

public slots:
	void setPresetFolder(const QString &folder);
	void deleteSelected();
	void setWriteOnly(bool writeOnly);

signals:
	void saveRequested(const QString &path);
	void loadRequested(const QString &path);

private slots:
	void onLoadClicked();
	void onSaveClicked();
	void onTextChanged(const QString &text);

private:
	QDir dir() const;

	QComboBox *m_presetBox;
	QPushButton *m_loadButton;
	QPushButton *m_saveButton;
	QPushButton *m_deleteButton;

	QString m_folder;
	QStringListModel *m_presets;

	bool m_writeOnly;
};

}

#endif

