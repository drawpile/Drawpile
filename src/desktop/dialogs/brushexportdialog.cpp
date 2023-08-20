// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/brushexportdialog.h"
#include "desktop/filewrangler.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace dialogs {

// The itemFromIndex member function is protected in Qt5 and there doesn't seem
// to be another way to set the check state of an item given an index otherwise.
class BrushExportDialog::ExportTreeWidget final : public QTreeWidget {
public:
	ExportTreeWidget(QWidget *parent = nullptr)
		: QTreeWidget{parent}
	{
	}

	void setCheckState(const QModelIndex &index, Qt::CheckState state)
	{
		QTreeWidgetItem *item = itemFromIndex(index);
		if(item) {
			item->setCheckState(0, state);
		}
	}
};

BrushExportDialog::BrushExportDialog(
	brushes::BrushPresetTagModel *tagModel,
	brushes::BrushPresetModel *presetModel, QWidget *parent)
	: QDialog{parent}
	, m_tagModel{tagModel}
	, m_presetModel{presetModel}
{
	setWindowModality(Qt::ApplicationModal);
	setWindowTitle(tr("Export Brushes"));

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	m_tree = new ExportTreeWidget;
	m_tree->setHeaderHidden(true);
	m_tree->setStyleSheet(
		QStringLiteral("QAbstractScrollArea { background-color: %1; }")
			.arg(palette().alternateBase().color().name(QColor::HexRgb)));
	buildTreeTags();
	layout->addWidget(m_tree);

	QDialogButtonBox *buttons =
		new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
	m_exportButton = buttons->button(QDialogButtonBox::Ok);
	m_exportButton->setText(tr("Export"));
	layout->addWidget(buttons);

	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&BrushExportDialog::exportSelected);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_debounceTimer = new QTimer{this};
	m_debounceTimer->setTimerType(Qt::CoarseTimer);
	m_debounceTimer->setSingleShot(true);
	m_debounceTimer->setInterval(20);

	updateExportButton();
	connect(
		m_tree->model(), &QAbstractItemModel::dataChanged, m_debounceTimer,
		QOverload<>::of(&QTimer::start));
	connect(
		m_debounceTimer, &QTimer::timeout, this,
		&BrushExportDialog::updateExportButton);

	resize(600, 500);
}

void BrushExportDialog::checkTag(int tagId)
{
	QAbstractItemModel *model = m_tree->model();
	int tagCount = model->rowCount();
	for(int i = 0; i < tagCount; ++i) {
		QModelIndex tagIndex = model->index(i, 0);
		if(tagIndex.data(Qt::UserRole).toInt() == tagId) {
			m_tree->expand(tagIndex);
			m_tree->setCheckState(tagIndex, Qt::Checked);
			return;
		}
	}
}

void BrushExportDialog::checkPreset(int presetId)
{
	QAbstractItemModel *model = m_tree->model();
	int tagCount = model->rowCount();
	for(int i = 0; i < tagCount; ++i) {
		QModelIndex tagIndex = model->index(i, 0);
		int presetCount = model->rowCount(tagIndex);
		for(int j = 0; j < presetCount; ++j) {
			QModelIndex presetIndex = model->index(j, 0, tagIndex);
			if(presetIndex.data(Qt::UserRole).toInt() == presetId) {
				m_tree->expand(tagIndex);
				m_tree->setCheckState(presetIndex, Qt::Checked);
				return;
			}
		}
	}
}

void BrushExportDialog::updateExportButton()
{
	QAbstractItemModel *model = m_tree->model();
	int tagCount = model->rowCount();
	for(int i = 0; i < tagCount; ++i) {
		QModelIndex tagIndex = model->index(i, 0);
		int presetCount = model->rowCount(tagIndex);
		for(int j = 0; j < presetCount; ++j) {
			QModelIndex presetIndex = model->index(j, 0, tagIndex);
			Qt::CheckState checkState =
				presetIndex.data(Qt::CheckStateRole).value<Qt::CheckState>();
			if(checkState == Qt::Checked) {
				m_exportButton->setEnabled(true);
				return;
			}
		}
	}
	m_exportButton->setEnabled(false);
}

void BrushExportDialog::exportSelected()
{
	QString path = FileWrangler{this}.getSaveBrushPackPath();
	if(path.isEmpty()) {
		return;
	}

	QVector<brushes::BrushExportTag> exportTags;
	QAbstractItemModel *model = m_tree->model();
	int tagCount = model->rowCount();
	for(int i = 0; i < tagCount; ++i) {
		QModelIndex tagIndex = model->index(i, 0);

		QVector<int> presetIds;
		int presetCount = model->rowCount(tagIndex);
		for(int j = 0; j < presetCount; ++j) {
			QModelIndex presetIndex = model->index(j, 0, tagIndex);
			Qt::CheckState checkState =
				presetIndex.data(Qt::CheckStateRole).value<Qt::CheckState>();
			if(checkState == Qt::Checked) {
				presetIds.append(presetIndex.data(Qt::UserRole).toInt());
			}
		}

		if(!presetIds.isEmpty()) {
			exportTags.append(
				{tagIndex.data(Qt::DisplayRole).toString(), presetIds});
		}
	}

	brushes::BrushExportResult result =
		m_tagModel->exportBrushPack(path, exportTags);
	if(result.ok) {
		accept();
	} else {
		showExportError(result);
	}
}

void BrushExportDialog::buildTreeTags()
{
	int tagCount = m_tagModel->rowCount();
	for(int i = 0; i < tagCount; ++i) {
		if(brushes::BrushPresetTagModel::isExportableRow(i)) {
			brushes::Tag tag = m_tagModel->getTagAt(i);
			QTreeWidgetItem *tagItem = new QTreeWidgetItem;
			tagItem->setCheckState(0, Qt::Unchecked);
			tagItem->setText(0, tag.name);
			tagItem->setData(0, Qt::UserRole, tag.id);
			buildTreePresets(tagItem, tag.id);
			Qt::ItemFlags flags =
				Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
			if(tagItem->childCount() == 0) {
				flags.setFlag(Qt::ItemNeverHasChildren);
			} else {
				flags.setFlag(Qt::ItemIsEnabled);
				flags.setFlag(Qt::ItemIsAutoTristate);
			}
			tagItem->setFlags(flags);
			m_tree->addTopLevelItem(tagItem);
		}
	}
}

void BrushExportDialog::buildTreePresets(QTreeWidgetItem *tagItem, int tagId)
{
	int presetCount = m_presetModel->rowCountForTagId(tagId);
	for(int i = 0; i < presetCount; ++i) {
		QModelIndex index = m_presetModel->indexForTagId(tagId, i);
		QTreeWidgetItem *presetItem = new QTreeWidgetItem;
		presetItem->setFlags(
			Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
			Qt::ItemNeverHasChildren);
		presetItem->setCheckState(0, Qt::Unchecked);
		presetItem->setText(
			0, index.data(brushes::BrushPresetModel::TitleRole).toString());
		presetItem->setData(
			0, Qt::UserRole, m_presetModel->getIdFromIndex(index));
		tagItem->addChild(presetItem);
	}
}

void BrushExportDialog::showExportError(
	const brushes::BrushExportResult &result)
{
	QDialog *dlg = new QDialog{this};
	dlg->setWindowTitle(tr("Brush Export"));
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setModal(true);

	QVBoxLayout *layout = new QVBoxLayout;
	dlg->setLayout(layout);

	QLabel *summaryLabel = new QLabel;
	if(result.ok) {
		summaryLabel->setText(
			tr("%n brush(es) exported.", "", result.exportedBrushCount));
	} else {
		summaryLabel->setText(tr("Brush export failed."));
	}
	summaryLabel->setTextFormat(Qt::PlainText);
	summaryLabel->setWordWrap(true);
	layout->addWidget(summaryLabel);

	QLabel *errorLabel =
		new QLabel{tr("%n error(s) encountered.", "", result.errors.size())};
	errorLabel->setTextFormat(Qt::PlainText);
	errorLabel->setWordWrap(true);
	layout->addWidget(errorLabel);

	QTextBrowser *browser = new QTextBrowser;
	browser->setPlainText(result.errors.join("\n"));
	layout->addWidget(browser);

	QDialogButtonBox *buttons = new QDialogButtonBox;
	buttons->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

	dlg->resize(400, 400);
	dlg->show();
}

}
