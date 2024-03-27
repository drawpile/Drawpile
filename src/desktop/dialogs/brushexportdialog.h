// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_BRUSHEXPORTDIALOG_H
#define DESKTOP_DIALOGS_BRUSHEXPORTDIALOG_H
#include <QDialog>

class QAbstractButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace brushes {
class BrushPresetModel;
class BrushPresetTagModel;
struct BrushExportResult;
}

namespace dialogs {

class BrushExportDialog final : public QDialog {
	Q_OBJECT
public:
	BrushExportDialog(
		brushes::BrushPresetTagModel *tagModel,
		brushes::BrushPresetModel *presetModel, QWidget *parent = nullptr);

	void checkTag(int tagId);
	void checkPreset(int presetId);

private slots:
	void updateExportButton();
	void exportSelected();

private:
	class ExportTreeWidget;

	void buildTreeTags();
	void buildTreePresets(QTreeWidgetItem *tagItem, int tagId);

	bool exportTo(const QString &path);
	void showExportError(const brushes::BrushExportResult &result);

	brushes::BrushPresetTagModel *m_tagModel;
	brushes::BrushPresetModel *m_presetModel;
	ExportTreeWidget *m_tree;
	QAbstractButton *m_exportButton;
	QTimer *m_debounceTimer;
};

}

#endif
