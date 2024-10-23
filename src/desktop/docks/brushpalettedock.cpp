// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/brushpalettedock.h"
#include "desktop/dialogs/brushexportdialog.h"
#include "desktop/docks/brushpalettedelegate.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/brushes/brush.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <functional>
using std::placeholders::_1;
using std::placeholders::_2;

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
	{
	}

	// Used by next/previous preset actions, for those we *do* want to scroll.
	void forceScrollTo(const QModelIndex &index)
	{
		QListView::scrollTo(index, EnsureVisible);
	}

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

	QComboBox *tagComboBox;
	QLineEdit *searchLineEdit;
	widgets::GroupedToolButton *menuButton;
	QMenu *assignmentMenu;
	QAction *newBrushAction;
	QAction *overwriteBrushAction;
	QAction *editBrushAction;
	QAction *resetBrushAction;
	QAction *resetAllAction;
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
	IgnoreEnsureVisibleListView *presetListView;

	int selectedPresetId = 0;
	int lastSelectedPresetId = 0;
};

BrushPalette::BrushPalette(QWidget *parent)
	: DockBase(parent)
	, d(new Private)
{
	d->tagModel = dpApp().brushPresets();
	d->presetModel = d->tagModel->presetModel();

	d->presetProxyModel = new QSortFilterProxyModel(this);
	d->presetProxyModel->setSourceModel(d->presetModel);
	d->presetProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	d->presetProxyModel->setFilterRole(brushes::BrushPresetModel::FilterRole);

	setWindowTitle(tr("Brushes"));

	TitleWidget *titleWidget = new TitleWidget(this);
	setTitleBarWidget(titleWidget);

	d->menuButton = new widgets::GroupedToolButton(this);
	d->menuButton->setIcon(QIcon::fromTheme("application-menu"));
	d->menuButton->setPopupMode(QToolButton::InstantPopup);
	titleWidget->addCustomWidget(d->menuButton);
	titleWidget->addSpace(4);

	d->tagComboBox = new QComboBox(this);
	d->tagComboBox->setInsertPolicy(QComboBox::NoInsert);
	d->tagComboBox->setMinimumWidth(24);
	d->menuButton->setMaximumHeight(d->tagComboBox->height());
	titleWidget->addCustomWidget(d->tagComboBox, true);
	titleWidget->addSpace(4);

	d->searchLineEdit = new QLineEdit(this);
	d->searchLineEdit->setPlaceholderText(tr("Search"));
	d->searchLineEdit->setMinimumWidth(24);
	titleWidget->addCustomWidget(d->searchLineEdit, true);
	titleWidget->addSpace(4);

	d->tagMenu = new QMenu(this);
	d->editBrushAction =
		d->tagMenu->addAction(QIcon::fromTheme("configure"), tr("&Edit Brush"));
	d->resetBrushAction = d->tagMenu->addAction(
		QIcon::fromTheme("view-refresh"), tr("&Reset Brush"));
	d->resetAllAction = d->tagMenu->addAction(tr("Reset All &Brushes"));
	d->tagMenu->addSeparator();
	d->newBrushAction =
		d->tagMenu->addAction(QIcon::fromTheme("list-add"), tr("&New Brush"));
	d->overwriteBrushAction = d->tagMenu->addAction(
		QIcon::fromTheme("document-save"), tr("&Overwrite Brush"));
	d->deleteBrushAction = d->tagMenu->addAction(
		QIcon::fromTheme("trash-empty"), tr("&Delete Brush"));
	d->assignmentMenu = d->tagMenu->addMenu(tr("Brush &Tags"));
	d->iconSizeMenu = d->tagMenu->addMenu(tr("&Icon Size"));
	d->tagMenu->addSeparator();
	d->newTagAction =
		d->tagMenu->addAction(QIcon::fromTheme("folder-new"), tr("Ne&w Tag"));
	d->editTagAction = d->tagMenu->addAction(
		QIcon::fromTheme("edit-rename"), tr("Rena&me Tag"));
	d->deleteTagAction = d->tagMenu->addAction(
		QIcon::fromTheme("list-remove"), tr("De&lete Tag"));
	d->tagMenu->addSeparator();
	d->importBrushesAction = d->tagMenu->addAction(tr("Im&port Brushes…"));
	d->exportTagAction = d->tagMenu->addAction(tr("E&xport Tag…"));
	d->menuButton->setMenu(d->tagMenu);

	d->brushMenu = new QMenu(this);
	d->brushMenu->addAction(d->editBrushAction);
	d->brushMenu->addAction(d->resetBrushAction);
	d->brushMenu->addAction(d->resetAllAction);
	d->brushMenu->addSeparator();
	d->brushMenu->addAction(d->newBrushAction);
	d->brushMenu->addAction(d->overwriteBrushAction);
	d->brushMenu->addAction(d->deleteBrushAction);
	d->brushMenu->addMenu(d->assignmentMenu);
	d->brushMenu->addMenu(d->iconSizeMenu);
	d->brushMenu->addSeparator();
	d->brushMenu->addAction(d->importBrushesAction);
	d->exportPresetAction = d->brushMenu->addAction(tr("Export Brush…"));

	for(int dimension = 16; dimension <= 128; dimension += 16) {
		QAction *sizeAction =
			d->iconSizeMenu->addAction(tr("%1x%1").arg(dimension));
		sizeAction->setCheckable(true);
		sizeAction->setData(dimension);
		connect(sizeAction, &QAction::triggered, [=]() {
			d->presetModel->setIconDimension(dimension);
		});
	}

	d->presetListView = new IgnoreEnsureVisibleListView(this);
	d->presetListView->setUniformItemSizes(true);
	d->presetListView->setFlow(QListView::LeftToRight);
	d->presetListView->setWrapping(true);
	d->presetListView->setResizeMode(QListView::Adjust);
	d->presetListView->setContextMenuPolicy(Qt::CustomContextMenu);
	d->presetListView->setSelectionMode(QAbstractItemView::SingleSelection);
	BrushPaletteDelegate *delegate = new BrushPaletteDelegate(this);
	d->presetListView->setItemDelegate(delegate);
	utils::bindKineticScrolling(d->presetListView);
	setWidget(d->presetListView);

	d->tagComboBox->setModel(d->tagModel);
	d->presetListView->setModel(d->presetProxyModel);

	connect(
		d->presetModel, &QAbstractItemModel::modelReset, this,
		&BrushPalette::presetsReset);
	connect(
		d->presetModel, &QAbstractItemModel::modelReset, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->presetModel, &QAbstractItemModel::rowsInserted, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->presetModel, &QAbstractItemModel::rowsRemoved, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->presetModel, &QAbstractItemModel::columnsInserted, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->presetModel, &QAbstractItemModel::columnsRemoved, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->presetModel, &QAbstractItemModel::dataChanged, delegate,
		&BrushPaletteDelegate::clearCache);
	connect(
		d->tagComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &BrushPalette::tagIndexChanged);
	connect(
		d->searchLineEdit, &QLineEdit::textChanged, d->presetProxyModel,
		&QSortFilterProxyModel::setFilterFixedString);
	connect(
		d->presetListView->selectionModel(),
		&QItemSelectionModel::currentChanged, this,
		&BrushPalette::presetCurrentIndexChanged);
	connect(
		d->assignmentMenu, &QMenu::aboutToShow, this,
		&BrushPalette::prepareTagAssignmentMenu);
	connect(d->newTagAction, &QAction::triggered, this, &BrushPalette::newTag);
	connect(
		d->editTagAction, &QAction::triggered, this,
		&BrushPalette::editCurrentTag);
	connect(
		d->deleteTagAction, &QAction::triggered, this,
		&BrushPalette::deleteCurrentTag);
	connect(
		d->newBrushAction, &QAction::triggered, this, &BrushPalette::newPreset);
	connect(
		d->overwriteBrushAction, &QAction::triggered, this,
		std::bind(&BrushPalette::overwriteCurrentPreset, this, this));
	connect(
		d->editBrushAction, &QAction::triggered, this,
		&BrushPalette::editBrushRequested);
	connect(
		d->resetBrushAction, &QAction::triggered, this,
		&BrushPalette::resetCurrentPreset);
	connect(
		d->resetAllAction, &QAction::triggered, this,
		&BrushPalette::resetAllPresets);
	connect(
		d->deleteBrushAction, &QAction::triggered, this,
		&BrushPalette::deleteCurrentPreset);
	connect(
		d->importBrushesAction, &QAction::triggered, this,
		&BrushPalette::importBrushes);
	connect(
		d->exportTagAction, &QAction::triggered, this,
		&BrushPalette::exportCurrentTag);
	connect(
		d->exportPresetAction, &QAction::triggered, this,
		&BrushPalette::exportCurrentPreset);
	connect(
		d->presetListView, &QAbstractItemView::clicked, this,
		&BrushPalette::applyToDetachedBrushSettings);
	connect(
		d->presetListView, &QAbstractItemView::doubleClicked, this,
		&BrushPalette::editBrushRequested);
	connect(
		d->presetListView, &QWidget::customContextMenuRequested, this,
		&BrushPalette::showPresetContextMenu);

	bool selectedTagIdOk;
	int selectedTagId =
		d->tagModel->getState(SELECTED_TAG_ID_KEY).toInt(&selectedTagIdOk);
	int selectedTagRow =
		selectedTagIdOk ? d->tagModel->getTagRowById(selectedTagId) : -1;
	int initialTagRow = selectedTagRow > 0 ? selectedTagRow : 0;
	d->tagComboBox->setCurrentIndex(initialTagRow);
	tagIndexChanged(d->tagComboBox->currentIndex());
	presetsReset();
}

BrushPalette::~BrushPalette()
{
	d->presetModel->writePresetChanges();
	delete d;
}

void BrushPalette::connectBrushSettings(tools::BrushSettings *brushSettings)
{
	d->brushSettings = brushSettings;
	connect(
		brushSettings, &tools::BrushSettings::presetIdChanged, this,
		&BrushPalette::setSelectedPresetId);
	connect(
		brushSettings, &tools::BrushSettings::newBrushRequested, this,
		&BrushPalette::newPreset);
	connect(
		brushSettings, &tools::BrushSettings::overwriteBrushRequested, this,
		std::bind(&BrushPalette::overwriteCurrentPreset, this, this));
	connect(
		brushSettings, &tools::BrushSettings::deleteBrushRequested, this,
		&BrushPalette::deleteCurrentPreset);
	brushSettings->connectBrushPresets(d->presetModel);
}

void BrushPalette::setActions(
	QAction *nextPreset, QAction *previousPreset, QAction *nextTag,
	QAction *previousTag)
{
	connect(
		nextPreset, &QAction::triggered, this, &BrushPalette::selectNextPreset);
	connect(
		previousPreset, &QAction::triggered, this,
		&BrushPalette::selectPreviousPreset);
	connect(nextTag, &QAction::triggered, this, &BrushPalette::selectNextTag);
	connect(
		previousTag, &QAction::triggered, this,
		&BrushPalette::selectPreviousTag);
}

void BrushPalette::newPreset()
{
	if(!d->brushSettings) {
		qWarning("Cannot overwrite preset: BrushSettings not connected to "
				 "BrushPalette");
		return;
	}

	const brushes::ActiveBrush brush = d->brushSettings->currentBrush();

	QString name, description;
	QPixmap thumbnail;
	if(d->brushSettings->currentPresetId() > 0) {
		name = d->brushSettings->currentPresetName();
		description = d->brushSettings->currentPresetDescription();
		thumbnail = d->brushSettings->currentPresetThumbnail();
	} else {
		name = tr("New Brush");
		description = QStringLiteral("");
		thumbnail = brush.presetThumbnail();
	}

	int tagId = d->currentTag.isAssignable() ? d->currentTag.id : 0;

	QRegularExpression trailingParensRe(
		QStringLiteral("\\A(.+?)\\s*\\(([0-9]+)\\)\\s*\\z"),
		QRegularExpression::DotMatchesEverythingOption);
	QRegularExpressionMatch match = trailingParensRe.match(name);
	int suffixNumber;
	if(match.hasMatch()) {
		name = match.captured(1);
		suffixNumber = match.captured(2).toInt();
	} else {
		suffixNumber = 0;
	}

	while(suffixNumber < 50) {
		QString suffixedName =
			suffixNumber == 0
				? name
				: QStringLiteral("%1 (%2)").arg(name).arg(suffixNumber + 1);
		int existingCount = d->presetModel->countNames(suffixedName);
		if(existingCount == 0) {
			name = suffixedName;
			break;
		} else {
			++suffixNumber;
		}
	}

	std::optional<brushes::Preset> opt =
		d->presetModel->newPreset(name, description, thumbnail, brush, tagId);
	if(opt.has_value()) {
		int presetId = opt->id;
		setSelectedPresetId(presetId);
		if(presetId != d->brushSettings->currentPresetId()) {
			d->brushSettings->setCurrentBrushPreset(opt.value());
		}
		emit editBrushRequested();
	}
}

void BrushPalette::overwriteCurrentPreset(QWidget *parent)
{
	if(!d->brushSettings) {
		qWarning("Cannot overwrite preset: BrushSettings not connected to "
				 "BrushPalette");
		return;
	}

	int presetId = d->selectedPresetId <= 0 ? d->lastSelectedPresetId
											: d->selectedPresetId;
	if(presetId <= 0) {
		return;
	}

	std::optional<brushes::Preset> opt =
		d->presetModel->searchPresetBrushData(presetId);
	if(!opt.has_value()) {
		return;
	}

	QMessageBox *box = utils::makeQuestion(
		parent, tr("Overwrite Brush"),
		tr("Really overwrite brush '%1' with the current one?")
			.arg(opt->originalName));
	box->setIconPixmap(opt->originalThumbnail);
	box->button(QMessageBox::Yes)->setText(tr("Overwrite"));
	box->button(QMessageBox::No)->setText(tr("Keep"));
	box->setDefaultButton(QMessageBox::No);
	connect(box, &QMessageBox::finished, this, [this, presetId](int result) {
		if(result == QMessageBox::Yes) {
			if(!d->brushSettings->isCurrentPresetAttached()) {
				d->presetModel->updatePresetBrush(
					presetId, d->brushSettings->currentBrush());
			} else if(d->brushSettings->currentPresetId() == presetId) {
				d->presetModel->updatePreset(
					presetId, d->brushSettings->currentPresetName(),
					d->brushSettings->currentPresetDescription(),
					d->brushSettings->currentPresetThumbnail(),
					d->brushSettings->currentBrush());
			}
		}
	});
	box->show();
}

void BrushPalette::setSelectedPresetIdsFromShortcut(
	const QKeySequence &shortcut)
{
	QVector<int> ids = d->presetModel->getPresetIdsForShortcut(shortcut);
	int count = ids.size();
	if(count != 0) {
		int currentIndex = ids.indexOf(d->selectedPresetId);
		int i = currentIndex < 0 ? 0 : ((currentIndex + 1) % count);
		setSelectedPresetId(ids[i]);
		if(d->brushSettings &&
		   d->brushSettings->currentPresetId() != d->selectedPresetId) {
			std::optional<brushes::Preset> opt =
				d->presetModel->searchPresetBrushData(d->selectedPresetId);
			if(opt.has_value()) {
				d->brushSettings->setCurrentBrushPreset(opt.value());
			}
		}
	}
}

void BrushPalette::resetAllPresets()
{
	d->presetModel->resetAllPresetChanges();
	if(d->brushSettings) {
		d->brushSettings->resetPresetsInAllSlots();
	}
}

void BrushPalette::importBrushes()
{
	FileWrangler(this).openBrushPack(
		std::bind(&BrushPalette::onOpen, this, _1, _2));
}

void BrushPalette::onOpen(const QString &path, QTemporaryFile *tempFile)
{
	brushes::BrushImportResult result =
		d->tagModel->importBrushPack(tempFile ? tempFile->fileName() : path);
	delete tempFile;
	if(!result.importedTags.isEmpty()) {
		d->tagComboBox->setCurrentIndex(
			tagIdToProxyRow(result.importedTags[0].id));
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
		tagsLabel->setText(tr("%n tag(s) imported: %1", "", tagCount)
							   .arg(tagNames.join(", ")));
	} else {
		tagsLabel->setText(tr("0 tags imported."));
	}

	QLabel *errorsLabel =
		new QLabel{tr("%n error(s) encountered.", "", errorCount), dlg};
	layout->addWidget(errorsLabel);
	errorsLabel->setWordWrap(true);

	if(errorCount != 0) {
		dlg->resize(400, 400);
		QTextBrowser *browser = new QTextBrowser{dlg};
		browser->setPlainText(result.errors.join("\n"));
		utils::bindKineticScrolling(browser);
		layout->addWidget(browser);
	}

	QDialogButtonBox *buttons = new QDialogButtonBox{dlg};
	buttons->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
	layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
	dlg->show();
}

void BrushPalette::exportBrushes()
{
	showExportDialog();
}

void BrushPalette::selectNextPreset()
{
	int rowCount = d->presetProxyModel->rowCount();
	if(rowCount > 0) {
		QModelIndex currentIdx = d->presetListView->currentIndex();
		int lastRow = rowCount - 1;
		int currentRow = currentIdx.isValid() ? currentIdx.row() : rowCount;
		int targetRow = currentRow < lastRow ? currentRow + 1 : 0;
		QModelIndex targetIdx =
			d->presetProxyModel->index(qBound(0, targetRow, lastRow), 0);
		d->presetListView->setCurrentIndex(targetIdx);
		d->presetListView->forceScrollTo(targetIdx);
	}
}

void BrushPalette::selectPreviousPreset()
{
	int rowCount = d->presetProxyModel->rowCount();
	if(rowCount > 0) {
		QModelIndex currentIdx = d->presetListView->currentIndex();
		int lastRow = rowCount - 1;
		int currentRow = currentIdx.isValid() ? currentIdx.row() : -1;
		int targetRow = currentRow > 0 ? currentRow - 1 : lastRow;
		QModelIndex targetIdx =
			d->presetProxyModel->index(qBound(0, targetRow, lastRow), 0);
		d->presetListView->setCurrentIndex(targetIdx);
		d->presetListView->forceScrollTo(targetIdx);
	}
}

void BrushPalette::selectNextTag()
{
	int index = d->tagComboBox->currentIndex() + 1;
	if(index < d->tagComboBox->count()) {
		d->tagComboBox->setCurrentIndex(index);
	} else {
		d->tagComboBox->setCurrentIndex(0);
	}
}

void BrushPalette::selectPreviousTag()
{
	int index = d->tagComboBox->currentIndex() - 1;
	if(index >= 0) {
		d->tagComboBox->setCurrentIndex(index);
	} else {
		d->tagComboBox->setCurrentIndex(d->tagComboBox->count() - 1);
	}
}

void BrushPalette::tagIndexChanged(int row)
{
	d->currentTag = d->tagModel->getTagAt(row);
	d->editTagAction->setEnabled(d->currentTag.isEditable());
	d->deleteTagAction->setEnabled(d->currentTag.isEditable());
	d->presetModel->setTagIdToFilter(d->currentTag.id);
	d->tagModel->setState(SELECTED_TAG_ID_KEY, d->currentTag.id);
}

void BrushPalette::setSelectedPresetId(int presetId)
{
	if(presetId != d->selectedPresetId) {
		d->selectedPresetId = presetId;
		if(presetId != 0) {
			d->lastSelectedPresetId = presetId;
		}
		updateSelectedPreset();
	}
}

void BrushPalette::prepareTagAssignmentMenu()
{
	if(d->assignmentMenu->isEmpty()) {
		if(d->selectedPresetId > 0) {
			QList<brushes::TagAssignment> tagAssignments =
				d->presetModel->getTagAssignments(d->selectedPresetId);
			if(tagAssignments.isEmpty()) {
				//: This message is shown when trying to assign a brush to tags,
				//: but there's no tags to assign it to.
				QAction *a = d->assignmentMenu->addAction(tr("No tags"));
				a->setEnabled(false);
			} else {
				std::sort(
					tagAssignments.begin(), tagAssignments.end(),
					[](const brushes::TagAssignment &a,
					   const brushes::TagAssignment &b) {
						return a.name < b.name;
					});
				d->assignmentMenu->clear();
				for(const brushes::TagAssignment &ta : tagAssignments) {
					QAction *action = d->assignmentMenu->addAction(ta.name);
					action->setCheckable(true);
					action->setChecked(ta.assigned);
					int tagId = ta.id;
					connect(action, &QAction::triggered, [=](bool checked) {
						changeTagAssignment(tagId, checked);
					});
				}
			}
		} else {
			//: This message is shown when trying to assign a brush to tags, but
			//: no brush is actually selected.
			QAction *a = d->assignmentMenu->addAction(tr("No brush selected"));
			a->setEnabled(false);
		}
	}
}

void BrushPalette::presetsReset()
{
	updateSelectedPreset();
	int iconDimension = d->presetModel->iconDimension();
	for(QAction *action : d->iconSizeMenu->actions()) {
		action->setChecked(action->data().toInt() == iconDimension);
	}
}

void BrushPalette::presetCurrentIndexChanged(
	const QModelIndex &current, const QModelIndex &previous)
{
	Q_UNUSED(previous);
	int presetId = presetProxyIndexToId(current);
	bool selected = presetId > 0;
	if(selected) {
		d->selectedPresetId = presetId;
		d->lastSelectedPresetId = presetId;
		if(d->brushSettings && d->brushSettings->brushPresetsAttach()) {
			applyToBrushSettings(current);
		}
	}
	d->assignmentMenu->clear();
	d->assignmentMenu->setEnabled(selected);
	d->overwriteBrushAction->setEnabled(selected);
	d->editBrushAction->setEnabled(selected);
	d->deleteBrushAction->setEnabled(selected);
}

void BrushPalette::newTag()
{
	bool ok;
	QString name = QInputDialog::getText(
		this, tr("New Tag"), tr("Tag name:"), QLineEdit::Normal, QString(),
		&ok);
	if(ok && !(name = name.trimmed()).isEmpty()) {
		int tagId = d->tagModel->newTag(name);
		if(tagId > 0) {
			d->tagComboBox->setCurrentIndex(tagIdToProxyRow(tagId));
		}
	}
}

void BrushPalette::editCurrentTag()
{
	if(d->currentTag.isEditable()) {
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
	if(d->currentTag.isEditable()) {
		QMessageBox *box = utils::makeQuestion(
			this, tr("Delete Tag"),
			tr("Really delete tag '%1'? This will not delete the brushes "
			   "within.")
				.arg(d->currentTag.name));
		box->button(QMessageBox::Yes)->setText(tr("Delete"));
		box->button(QMessageBox::No)->setText(tr("Keep"));
		box->setDefaultButton(QMessageBox::No);
		connect(
			box, &QMessageBox::finished, this,
			[this, tagId = d->currentTag.id](int result) {
				if(result == QMessageBox::Yes && tagId == d->currentTag.id &&
				   d->currentTag.isEditable()) {
					d->tagModel->deleteTag(tagId);
				}
			});
		box->show();
	}
}

void BrushPalette::resetCurrentPreset()
{
	int presetId = d->selectedPresetId;
	if(presetId > 0) {
		if(d->brushSettings &&
		   d->brushSettings->currentPresetId() == presetId) {
			d->brushSettings->resetPreset();
		} else {
			d->presetModel->changePreset(presetId);
		}
	}
}

void BrushPalette::deleteCurrentPreset()
{
	int presetId = d->selectedPresetId;
	if(presetId <= 0) {
		return;
	}

	std::optional<brushes::Preset> opt =
		d->presetModel->searchPresetBrushData(presetId);
	if(!opt.has_value()) {
		return;
	}

	QMessageBox *box = utils::makeQuestion(
		this, tr("Delete Brush"),
		tr("Really delete brush '%1'?").arg(opt->effectiveName()));
	box->setIconPixmap(opt->effectiveThumbnail());
	box->button(QMessageBox::Yes)->setText(tr("Delete"));
	box->button(QMessageBox::No)->setText(tr("Keep"));
	box->setDefaultButton(QMessageBox::No);
	connect(box, &QMessageBox::finished, this, [this, presetId](int result) {
		if(result == QMessageBox::Yes && d->selectedPresetId == presetId) {
			d->presetModel->deletePreset(presetId);
		}
	});
	box->show();
}

void BrushPalette::exportCurrentTag()
{
	dialogs::BrushExportDialog *dlg = showExportDialog();
	dlg->checkTag(d->currentTag.id);
}

void BrushPalette::exportCurrentPreset()
{
	dialogs::BrushExportDialog *dlg = showExportDialog();
	dlg->checkPreset(d->selectedPresetId);
}

void BrushPalette::showPresetContextMenu(const QPoint &pos)
{
	d->brushMenu->popup(d->presetListView->mapToGlobal(pos));
}

void BrushPalette::applyToDetachedBrushSettings(const QModelIndex &proxyIndex)
{
	if(d->brushSettings && !d->brushSettings->brushPresetsAttach()) {
		applyToBrushSettings(proxyIndex);
	}
}

void BrushPalette::applyToBrushSettings(const QModelIndex &proxyIndex)
{
	if(!d->brushSettings) {
		qWarning("BrushSettings not connected to BrushPalette");
		return;
	}

	QModelIndex sourceIndex = presetIndexToSource(proxyIndex);
	brushes::Preset preset =
		sourceIndex.data(brushes::BrushPresetModel::PresetRole)
			.value<brushes::Preset>();
	if(preset.id > 0 && preset.id != d->brushSettings->currentPresetId()) {
		d->brushSettings->setCurrentBrushPreset(preset);
	}
}

void BrushPalette::changeTagAssignment(int tagId, bool assigned)
{
	int presetId = d->selectedPresetId;
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

void BrushPalette::updateSelectedPreset()
{
	QModelIndex idx;
	if(d->selectedPresetId > 0) {
		idx = d->presetModel->cachedIndexForId(d->selectedPresetId);
	}

	if(idx.isValid()) {
		d->presetListView->selectionModel()->setCurrentIndex(
			presetIndexToProxy(idx), QItemSelectionModel::ClearAndSelect);
	} else {
		d->presetListView->selectionModel()->clear();
	}
}

dialogs::BrushExportDialog *BrushPalette::showExportDialog()
{
	dialogs::BrushExportDialog *dlg =
		new dialogs::BrushExportDialog{d->tagModel, d->presetModel, this};
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
	return dlg;
}

}
