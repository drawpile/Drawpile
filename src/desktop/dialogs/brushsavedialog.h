// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_BRUSHSAVEDIALOG_H
#define DESKTOP_DIALOGS_BRUSHSAVEDIALOG_H
#include <QDialog>
#include <QPixmap>
#include <QSet>

class QListWidget;

namespace brushes {
struct Preset;
}

namespace dialogs {

class BrushPresetForm;

class BrushSaveDialog : public QDialog {
	Q_OBJECT
public:
	BrushSaveDialog(QWidget *parent = nullptr);

	void setPreset(const brushes::Preset &preset);
	void addTag(int id, const QString &name);

	QString presetName() const;
	QString presetDescription() const;
	QPixmap presetThumbnail() const;
	QSet<int> presetTagIds() const;

private:
	BrushPresetForm *m_presetForm;
	QListWidget *m_tagList;
};

}

#endif
