// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/brushpalettedock.h"
#include "desktop/dialogs/brushexportdialog.h"
#include "desktop/dialogs/brushpresetproperties.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/brushes/brush.h"
#include "desktop/filewrangler.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/utils/widgetutils.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QMessageBox>

static constexpr char SELECTED_TAG_ID_KEY[] = "brushpalette:selected_tag_id";

namespace docks {

namespace {

// By default, the list view will try to scroll a chosen preset into view. This
// is already disorienting when it works as intended, but sometimes Qt ends up
// overscrolling, causing an infinite feedback loop of the scroll bar bouncing
// up and down in a desperate struggle against itself. So we just disable it.
class IgnoreEnsureVisibleListView final : public QListView {
public:
	IgnoreEnsureVisibleListView(QWidget *parent = nullptr)
		: QListView{parent}
	{}

protected:
	void scrollTo(
		const QModelIndex &index,
		QAbstractItemView::ScrollHint hint = EnsureVisible) override final
	{
		if(hint != EnsureVisible) {
			QListView::scrollTo(index, hint);
		}
	}
};

}

struct BrushPalette::Private {
	brushes::BrushPresetTagModel *tagModel;
	brushes::BrushPresetModel *presetModel;
	QSortFilterProxyModel *presetProxyModel;
	tools::BrushSettings *brushSettings;
	brushes::Tag currentTag;
	brushes::ActiveBrush newBrush;

	QComboBox *tagComboBox;
	QLineEdit *searchLineEdit;
	widgets::GroupedToolButton *menuButton;
	QMenu *assignmentMenu;
	QAction *newBrushAction;
	QAction *duplicateBrushAction;
	QAction *overwriteBrushAction;
	QAction *editBrushAction;
	QAction *deleteBrushAction;
	QMenu *iconSizeMenu;
	QMenu *tagMenu;
	QMenu *brushMenu;
	QAction *newTagAction;
	QAction *editTagAction;
	QAction *deleteTagAction;
	QAction *importBrushesAction;
	QAction *exportTagAction;
	QAction *exportPresetAction;
	QListView *presetListView;
};

BrushPalette::BrushPalette(QWidget *parent)
	: DockBase(parent)
	, d(new Private)
{
	static brushes::BrushPresetTagModel *tagModelInstance;
	if(!tagModelInstance) {
		tagModelInstance = new brushes::BrushPresetTagModel;
	}

	d->tagModel = tagModelInstance;
	d->presetModel = d->tagModel->presetModel();

	d->presetProxyModel = new QSortFilterProxyModel(this);
	d->presetProxyModel->setSourceModel(d->presetModel);
	d->presetProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	d->presetProxyModel->setFilterRole(brushes::BrushPresetModel::FilterRole);

	setWindowTitle(tr("Brushes"));

	TitleWidget *titleWidget = new TitleWidget(this);
	setTitleBarWidget(titleWidget);
	titleWidget->addSpace(4);

	d->tagComboBox = new QComboBox(this);
	d->tagComboBox->setInsertPolicy(QComboBox::NoInsert);
	d->tagComboBox->setMinimumWidth(24);
	titleWidget->addCustomWidget(d->tagComboBox, true);
	titleWidget->addSpace(4);

	d->searchLineEdit = new QLineEdit(this);
	d->searchLineEdit->setPlaceholderText(tr("Search"));
	d->searchLineEdit->setMinimumWidth(24);
	titleWidget->addCustomWidget(d->searchLineEdit, true);
	titleWidget->addSpace(4);

	d->menuButton = new widgets::GroupedToolButton(this);
	d->menuButton->setIcon(QIcon::fromTheme("application-menu"));
	d->menuButton->setPopupMode(QToolButton::InstantPopup);
	d->menuButton->setMaximumHeight(d->tagComboBox->height());
	titleWidget->addCustomWidget(d->menuButton);
	titleWidget->addSpace(4);

	d->tagMenu = new QMenu(this);
	d->newBrushAction = d->tagMenu->addAction(QIcon::fromTheme("list-add"), tr("New Brush Preset"));
	d->duplicateBrushAction = d->tagMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Duplicate Brush Preset"));
	d->overwriteBrushAction = d->tagMenu->addAction(QIcon::fromTheme("document-save"), tr("Overwrite Brush Preset"));
	d->editBrushAction = d->tagMenu->addAction(QIcon::fromTheme("configure"), tr("Edit Brush Preset"));
	d->deleteBrushAction = d->tagMenu->addAction(QIcon::fromTheme("trash-empty"), tr("Delete Brush Preset"));
	d->assignmentMenu = d->tagMenu->addMenu(tr("Brush Tags"));
	d->iconSizeMenu = d->tagMenu->addMenu(tr("Icon Size"));
	d->tagMenu->addSeparator();
	d->newTagAction = d->tagMenu->addAction(QIcon::fromTheme("folder-new"), tr("New Tag"));
	d->editTagAction = d->tagMenu->addAction(QIcon::fromTheme("edit-rename"), tr("Rename Tag"));
	d->deleteTagAction = d->tagMenu->addAction(QIcon::fromTheme("list-remove"), tr("Delete Tag"));
	d->tagMenu->addSeparator();
	d->importBrushesAction = d->tagMenu->addAction(tr("Import Brushes..."));
	d->exportTagAction = d->tagMenu->addAction(tr("Export Tag…"));
	d->menuButton->setMenu(d->tagMenu);

	d->brushMenu = new QMenu(this);
	d->brushMenu->addAction(d->newBrushAction);
	d->brushMenu->addAction(d->duplicateBrushAction);
	d->brushMenu->addAction(d->overwriteBrushAction);
	d->brushMenu->addAction(d->editBrushAction);
	d->brushMenu->addAction(d->deleteBrushAction);
	d->brushMenu->addMenu(d->assignmentMenu);
	d->brushMenu->addMenu(d->iconSizeMenu);
	d->tagMenu->addSeparator();
	d->brushMenu->addAction(d->importBrushesAction);
	d->exportPresetAction = d->brushMenu->addAction(tr("Export Brush…"));

	for(int dimension = 16; dimension <= 128; dimension += 16) {
		QAction *sizeAction = d->iconSizeMenu->addAction(tr("%1x%1").arg(dimension));
		sizeAction->setCheckable(true);
		sizeAction->setData(dimension);
		connect(sizeAction, &QAction::triggered, [=](){
			d->presetModel->setIconDimension(dimension);
		});
	}

	d->presetListView = new IgnoreEnsureVisibleListView(this);
	d->presetListView->setUniformItemSizes(true);
	d->presetListView->setFlow(QListView::LeftToRight);
	d->presetListView->setWrapping(true);
	d->presetListView->setResizeMode(QListView::Adjust);
	d->presetListView->setContextMenuPolicy(Qt::CustomContextMenu);
	setWidget(d->presetListView);

	d->tagComboBox->setModel(d->tagModel);
	d->presetListView->setModel(d->presetProxyModel);

	connect(d->presetModel, &QAbstractItemModel::modelReset, this, &BrushPalette::presetsReset);
	connect(d->tagComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &BrushPalette::tagIndexChanged);
	connect(d->searchLineEdit, &QLineEdit::textChanged,
		d->presetProxyModel, &QSortFilterProxyModel::setFilterFixedString);
	connect(d->presetListView->selectionModel(), &QItemSelectionModel::currentChanged,
		this, &BrushPalette::presetSelectionChanged);
	connect(d->newTagAction, &QAction::triggered, this, &BrushPalette::newTag);
	connect(d->editTagAction, &QAction::triggered, this, &BrushPalette::editCurrentTag);
	connect(d->deleteTagAction, &QAction::triggered, this, &BrushPalette::deleteCurrentTag);
	connect(d->newBrushAction, &QAction::triggered, this, &BrushPalette::newPreset);
	connect(d->duplicateBrushAction, &QAction::triggered, this, &BrushPalette::duplicateCurrentPreset);
	connect(d->overwriteBrushAction, &QAction::triggered, this, &BrushPalette::overwriteCurrentPreset);
	connect(d->editBrushAction, &QAction::triggered, this, &BrushPalette::editCurrentPreset);
	connect(d->deleteBrushAction, &QAction::triggered, this, &BrushPalette::deleteCurrentPreset);
	connect(d->importBrushesAction, &QAction::triggered, this, &BrushPalette::importBrushes);
	connect(d->exportTagAction, &QAction::triggered, this, &BrushPalette::exportCurrentTag);
	connect(d->exportPresetAction, &QAction::triggered, this, &BrushPalette::exportCurrentPreset);
	connect(d->presetListView, &QAbstractItemView::clicked, this, &BrushPalette::applyToBrushSettings);
	connect(d->presetListView, &QAbstractItemView::doubleClicked, this, &BrushPalette::editCurrentPreset);
	connect(d->presetListView, &QWidget::customContextMenuRequested, this, &BrushPalette::showPresetContextMenu);

	bool selectedTagIdOk;
	int selectedTagId = d->tagModel->getState(SELECTED_TAG_ID_KEY).toInt(&selectedTagIdOk);
	int selectedTagRow = selectedTagIdOk ? d->tagModel->getTagRowById(selectedTagId) : -1;
	int initialTagRow = selectedTagRow > 0 ? selectedTagRow : 0;
	d->tagComboBox->setCurrentIndex(initialTagRow);
	tagIndexChanged(d->tagComboBox->currentIndex());
	presetsReset();
}

BrushPalette::~BrushPalette()
{
	delete d;
}

void BrushPalette::connectBrushSettings(tools::ToolSettings *toolSettings)
{
	d->brushSettings = qobject_cast<tools::BrushSettings*>(toolSettings);
}

void BrushPalette::importBrushes()
{
	QString file = FileWrangler{this}.getOpenBrushPackPath();

	if(!file.isEmpty()) {
		brushes::BrushImportResult result =
			d->tagModel->importBrushPack(file);
		if(!result.importedTags.isEmpty()) {
			d->tagComboBox->setCurrentIndex(tagIdToProxyRow(result.importedTags[0].id));
		}

		QDialog *dlg = new QDialog{this};
		dlg->setWindowTitle(tr("Brush Import"));
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setModal(true);

		QVBoxLayout *layout = new QVBoxLayout{dlg};
		dlg->setLayout(layout);

		QString text;
		int errorCount = result.errors.size();

		QLabel *brushLabel = new QLabel{
			tr("%n brush(es) imported.", "", result.importedBrushCount), dlg};
		layout->addWidget(brushLabel);
		brushLabel->setWordWrap(true);

		QLabel *tagsLabel = new QLabel{dlg};
		layout->addWidget(tagsLabel);
		tagsLabel->setWordWrap(true);
		int tagCount = result.importedTags.count();
		if(tagCount > 0) {
			QStringList tagNames;
			for(const brushes::Tag &tag : result.importedTags) {
				tagNames.append(tag.name);
			}
			tagsLabel->setText(
				tr("%n tag(s) imported: %1", "", tagCount).arg(tagNames.join(", ")));
		} else {
			tagsLabel->setText(tr("0 tags imported."));
		}

		QLabel *errorsLabel = new QLabel{
			tr("%n error(s) encountered.", "", errorCount), dlg};
		layout->addWidget(errorsLabel);
		errorsLabel->setWordWrap(true);

		if(errorCount != 0) {
			dlg->resize(400, 400);
			QTextBrowser *browser = new QTextBrowser{dlg};
			browser->setPlainText(result.errors.join("\n"));
			layout->addWidget(browser);
		}

		QDialogButtonBox *buttons = new QDialogButtonBox{dlg};
		buttons->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
		layout->addWidget(buttons);

		connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
		dlg->show();
	}
}

void BrushPalette::exportBrushes()
{
	showExportDialog();
}

void BrushPalette::tagIndexChanged(int row)
{
	d->currentTag = d->tagModel->getTagAt(row);
	d->editTagAction->setEnabled(d->currentTag.editable);
	d->deleteTagAction->setEnabled(d->currentTag.editable);
	d->presetModel->setTagIdToFilter(d->currentTag.id);
	d->tagModel->setState(SELECTED_TAG_ID_KEY, d->currentTag.id);
}

void BrushPalette::presetsReset()
{
	presetSelectionChanged(d->presetListView->currentIndex(), QModelIndex());
	int iconDimension = d->presetModel->iconDimension();
	for(QAction *action : d->iconSizeMenu->actions()) {
		action->setChecked(action->data().toInt() == iconDimension);
	}
}

void BrushPalette::presetSelectionChanged(const QModelIndex &current, const QModelIndex &)
{
	int presetId = presetProxyIndexToId(current);
	bool selected = presetId > 0;
	if(selected) {
		QList<brushes::TagAssignment> tagAssignments = d->presetModel->getTagAssignments(presetId);
		if(!tagAssignments.isEmpty()) {
			std::sort(tagAssignments.begin(), tagAssignments.end(),
				[](const brushes::TagAssignment &a, const brushes::TagAssignment &b) {
					return a.name < b.name;
				});
			d->assignmentMenu->clear();
			for (const brushes::TagAssignment &ta : tagAssignments) {
				QAction *action = d->assignmentMenu->addAction(ta.name);
				action->setCheckable(true);
				action->setChecked(ta.assigned);
				int tagId = ta.id;
				connect(action, &QAction::triggered, [=](bool checked) {
					changeTagAssignment(tagId, checked);
				});
			}
		}
	}
	d->assignmentMenu->setEnabled(selected && !d->assignmentMenu->actions().isEmpty());
	d->duplicateBrushAction->setEnabled(selected);
	d->overwriteBrushAction->setEnabled(selected);
	d->editBrushAction->setEnabled(selected);
	d->deleteBrushAction->setEnabled(selected);
}

void BrushPalette::newTag()
{
	bool ok;
	QString name = QInputDialog::getText(
		this, tr("New Tag"), tr("Tag name:"), QLineEdit::Normal, QString(), &ok);
	if(ok && !(name = name.trimmed()).isEmpty()) {
		int tagId = d->tagModel->newTag(name);
		if(tagId > 0) {
			d->tagComboBox->setCurrentIndex(tagIdToProxyRow(tagId));
		}
	}
}

void BrushPalette::editCurrentTag()
{
	if(d->currentTag.editable) {
		bool ok;
		QString name = QInputDialog::getText(
			this, tr("Edit Tag"), tr("Tag name:"), QLineEdit::Normal,
			d->currentTag.name, &ok);
		if(ok && !(name = name.trimmed()).isEmpty()) {
			int sourceRow = d->tagModel->editTag(d->currentTag.id, name);
			if(sourceRow >= 0) {
				d->tagComboBox->setCurrentIndex(sourceRow);
			}
		}
	}
}

void BrushPalette::deleteCurrentTag()
{
	if(d->currentTag.editable) {
		bool confirmed = question(
			tr("Delete Tag"), tr("Really delete tag '%1'? This will not delete "
								 "the brushes within.")
								  .arg(d->currentTag.name));
		if(confirmed) {
			d->tagModel->deleteTag(d->currentTag.id);
		}
	}
}

void BrushPalette::newPreset()
{
	if(!d->brushSettings) {
		qWarning("Cannot overwrite preset: BrushSettings not connected to BrushPalette");
		return;
	}
	d->newBrush = d->brushSettings->currentBrush();

	dialogs::BrushPresetProperties *dialog = new dialogs::BrushPresetProperties(
		0, tr("New Brush"), QString(), d->newBrush.presetThumbnail(), this);
	connect(dialog, &dialogs::BrushPresetProperties::presetPropertiesApplied,
		this, &BrushPalette::applyPresetProperties);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowTitle(tr("New Brush Preset"));
	dialog->show();
}

void BrushPalette::duplicateCurrentPreset()
{
	int presetId = currentPresetId();
	if(presetId > 0) {
		d->presetModel->duplicatePreset(presetId);
	}
}

void BrushPalette::overwriteCurrentPreset()
{
	if(!d->brushSettings) {
		qWarning("Cannot overwrite preset: BrushSettings not connected to BrushPalette");
		return;
	}
	int presetId = currentPresetId();
	if(presetId > 0) {
		QString name = d->presetModel->getPresetMetadata(presetId).name;
		bool confirmed = question(tr("Overwrite Brush Preset"),
			tr("Really overwrite brush preset '%1' with the current brush?").arg(name));
		if(confirmed) {
			brushes::ActiveBrush brush = d->brushSettings->currentBrush();
			d->presetModel->updatePresetData(presetId, brush.presetType(), brush.presetData());
		}
	}
}

void BrushPalette::editCurrentPreset()
{
	int presetId = currentPresetId();
	if(presetId > 0) {
		brushes::PresetMetadata pm = d->presetModel->getPresetMetadata(presetId);
		if(pm.id > 0) {
			QPixmap thumbnail;
			if(!thumbnail.loadFromData(pm.thumbnail)) {
				qWarning("Loading thumbnail for preset %d failed", pm.id);
			}
			dialogs::BrushPresetProperties *dialog = new dialogs::BrushPresetProperties(
				pm.id, pm.name, pm.description, thumbnail, this);
			connect(dialog, &dialogs::BrushPresetProperties::presetPropertiesApplied,
				this, &BrushPalette::applyPresetProperties);
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->setWindowTitle(tr("Edit Brush Preset"));
			dialog->show();
		}
	}
}

void BrushPalette::deleteCurrentPreset()
{
	int presetId = currentPresetId();
	if(presetId > 0) {
		QString name = d->presetModel->getPresetMetadata(presetId).name;
		bool confirmed = question(tr("Delete Brush Preset"),
			tr("Really delete brush preset '%1'?").arg(name));
		if(confirmed) {
			d->presetModel->deletePreset(presetId);
		}
	}
}

void BrushPalette::exportCurrentTag()
{
	dialogs::BrushExportDialog *dlg = showExportDialog();
	dlg->checkTag(d->currentTag.id);
}

void BrushPalette::exportCurrentPreset()
{
	dialogs::BrushExportDialog *dlg = showExportDialog();
	dlg->checkPreset(currentPresetId());
}

void BrushPalette::applyPresetProperties(int id, const QString &name,
	const QString &description, const QPixmap &thumbnail)
{
	if(id == 0) {
		int presetId = d->presetModel->newPreset(d->newBrush.presetType(), name,
			description, thumbnail, d->newBrush.presetData());
		int tagId = d->currentTag.id;
		if(presetId > 0 && tagId > 0) {
			d->presetModel->changeTagAssignment(presetId, tagId, true);
		}
	} else {
		d->presetModel->updatePresetMetadata(id, name, description, thumbnail);
	}
}

void BrushPalette::applyToBrushSettings(const QModelIndex &proxyIndex)
{
	if(!d->brushSettings) {
		qWarning("BrushSettings not connected to BrushPalette");
		return;
	}

	QModelIndex sourceIndex = presetIndexToSource(proxyIndex);
	QVariant v = sourceIndex.data(brushes::BrushPresetModel::BrushRole);
	if(v.isNull()) {
		qWarning("Brush was null");
		return;
	}

	d->brushSettings->setCurrentBrush(v.value<brushes::ActiveBrush>());
}

void BrushPalette::showPresetContextMenu(const QPoint &pos)
{
	d->brushMenu->popup(d->presetListView->mapToGlobal(pos));
}

void BrushPalette::changeTagAssignment(int tagId, bool assigned)
{
	int presetId = currentPresetId();
	if(presetId > 0) {
		d->presetModel->changeTagAssignment(presetId, tagId, assigned);
	}
}

int BrushPalette::tagIdToProxyRow(int tagId)
{
	return d->tagModel->getTagRowById(tagId);
}

int BrushPalette::presetRowToSource(int proxyRow)
{
	return presetIndexToSource(d->presetProxyModel->index(proxyRow, 0)).row();
}

int BrushPalette::presetRowToProxy(int sourceRow)
{
	return presetIndexToProxy(d->presetModel->index(sourceRow)).row();
}

QModelIndex BrushPalette::presetIndexToSource(const QModelIndex &proxyIndex)
{
	return d->presetProxyModel->mapToSource(proxyIndex);
}

QModelIndex BrushPalette::presetIndexToProxy(const QModelIndex &sourceIndex)
{
	return d->presetProxyModel->mapFromSource(sourceIndex);
}

int BrushPalette::presetProxyIndexToId(const QModelIndex &proxyIndex)
{
	return d->presetModel->getIdFromIndex(presetIndexToSource(proxyIndex));
}

int BrushPalette::currentPresetId()
{
	return presetProxyIndexToId(d->presetListView->currentIndex());
}

dialogs::BrushExportDialog *BrushPalette::showExportDialog()
{
	dialogs::BrushExportDialog *dlg =
		new dialogs::BrushExportDialog{d->tagModel, d->presetModel, this};
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
	return dlg;
}

bool BrushPalette::question(const QString &title, const QString &text) const
{
	return QMessageBox::question(widget(), title, text) == QMessageBox::Yes;
}

}
