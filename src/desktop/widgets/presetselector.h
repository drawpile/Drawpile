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

class QComboBox;
class QPushButton;
class QStringListModel;
class QDir;

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtUiPlugin/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

class PLUGIN_EXPORT PresetSelector : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QString presetFolder READ presetFolder WRITE setPresetFolder)
	Q_PROPERTY(bool writeOnly READ writeOnly WRITE setWriteOnly)
public:
	PresetSelector(QWidget *parent=nullptr, Qt::WindowFlags f=0);

	QString presetFolder() const { return m_folder; }
	bool writeOnly() const { return m_writeOnly; }

public slots:
	void setPresetFolder(const QString &folder);
	void deleteSelected();
	void setWriteOnly(bool writeOnly);

signals:
	void saveRequested(const QString &path);
	void presetSelected(const QString &path);

private slots:
	void onSaveClicked();
	void onTextChanged(const QString &text);
	void onCurrentIndexChanged(const QString &text);

private:
	QDir dir() const;

	QComboBox *m_presetBox;
	QPushButton *m_saveButton;
	QPushButton *m_deleteButton;

	QString m_folder;
	QStringListModel *m_presets;

	bool m_writeOnly;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

