// SPDX-License-Identifier: GPL-3.0-or-later

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

