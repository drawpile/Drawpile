// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/canvasshortcutsdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
#include "desktop/dialogs/settingsdialog/shortcutfilterinput.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/keysequenceedit.h"
#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/utils/brushshortcutmodel.h"
#include "libclient/utils/canvasshortcutsmodel.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libshared/util/qtcompat.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#define ON_ANY_MODEL_CHANGE(MODEL, RECEIVER, FN)                               \
	do {                                                                       \
		connect(MODEL, &QAbstractItemModel::dataChanged, RECEIVER, FN);        \
		connect(MODEL, &QAbstractItemModel::modelReset, RECEIVER, FN);         \
		connect(MODEL, &QAbstractItemModel::rowsInserted, RECEIVER, FN);       \
		connect(MODEL, &QAbstractItemModel::rowsRemoved, RECEIVER, FN);        \
	} while(0)

namespace dialogs {
namespace settingsdialog {

namespace {

class KeySequenceEditFactory final : public QItemEditorCreatorBase {
public:
	KeySequenceEditFactory(Shortcuts *shortcuts)
		: m_shortcuts{shortcuts}
	{
	}

	QWidget *createWidget(QWidget *parent) const override
	{
		widgets::KeySequenceEdit *widget = new widgets::KeySequenceEdit(parent);
		QObject::connect(
			widget, &widgets::KeySequenceEdit::editingFinished, m_shortcuts,
			&Shortcuts::finishEditing);
		return widget;
	}

	QByteArray valuePropertyName() const override
	{
		return QByteArrayLiteral("keySequence");
	}

private:
	Shortcuts *m_shortcuts;
};

}

Shortcuts::Shortcuts(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
	, m_shortcutConflictDebounce(100)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	m_filter = new ShortcutFilterInput;
	layout->addWidget(m_filter);

	QStyledItemDelegate *keySequenceDelegate = new QStyledItemDelegate(this);
	m_itemEditorFactory.registerEditor(
		compat::metaTypeFromName("QKeySequence"),
		new KeySequenceEditFactory(this));
	keySequenceDelegate->setItemEditorFactory(&m_itemEditorFactory);

	m_tabs = new QTabWidget;
	Q_ASSERT(m_tabs->count() == ACTION_TAB);
	m_tabs->addTab(
		initActionShortcuts(settings, keySequenceDelegate),
		QIcon::fromTheme("input-keyboard"), QString());
	Q_ASSERT(m_tabs->count() == BRUSH_TAB);
	m_tabs->addTab(
		initBrushShortcuts(keySequenceDelegate), QIcon::fromTheme("draw-brush"),
		QString());
	Q_ASSERT(m_tabs->count() == CANVAS_TAB);
	m_tabs->addTab(
		initCanvasShortcuts(settings), QIcon::fromTheme("edit-image"),
		QString());
	layout->addWidget(m_tabs);

	updateConflicts();
	ON_ANY_MODEL_CHANGE(
		m_actionShortcutsModel, &m_shortcutConflictDebounce,
		&DebounceTimer::setNone);
	ON_ANY_MODEL_CHANGE(
		m_brushShortcutsModel, &m_shortcutConflictDebounce,
		&DebounceTimer::setNone);
	ON_ANY_MODEL_CHANGE(
		m_canvasShortcutsModel, &m_shortcutConflictDebounce,
		&DebounceTimer::setNone);
	connect(
		&m_shortcutConflictDebounce, &DebounceTimer::noneChanged, this,
		&Shortcuts::updateConflicts);

	connect(
		m_filter, &ShortcutFilterInput::filtered, this,
		&Shortcuts::updateTabTexts, Qt::QueuedConnection);
	updateTabTexts();
}

void Shortcuts::initiateFixShortcutConflicts()
{
	m_filter->checkConflictBox();
	if(m_actionShortcutsModel->hasConflicts()) {
		m_tabs->setCurrentIndex(ACTION_TAB);
	} else if(m_brushShortcutsModel->hasConflicts()) {
		m_tabs->setCurrentIndex(BRUSH_TAB);
	} else if(m_canvasShortcutsModel->hasConflicts()) {
		m_tabs->setCurrentIndex(CANVAS_TAB);
	}
}

void Shortcuts::initiateBrushShortcutChange(int presetId)
{
	m_tabs->setCurrentIndex(BRUSH_TAB);
	QSortFilterProxyModel *model =
		qobject_cast<QSortFilterProxyModel *>(m_brushesTable->model());
	QModelIndex idx = m_brushShortcutsModel->indexById(
		presetId, int(BrushShortcutModel::Shortcut));
	if(model && idx.isValid()) {
		QModelIndex mappedIdx = model->mapFromSource(idx);
		m_brushesTable->scrollTo(mappedIdx);
		m_brushesTable->setCurrentIndex(mappedIdx);
		emit m_brushesTable->clicked(mappedIdx);
	}
}

void Shortcuts::finishEditing()
{
	m_actionsTable->setCurrentIndex(QModelIndex());
	m_brushesTable->setCurrentIndex(QModelIndex());
}

QWidget *Shortcuts::initActionShortcuts(
	desktop::settings::Settings &settings,
	QStyledItemDelegate *keySequenceDelegate)
{
	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	CustomShortcutModel *shortcutsModel = new CustomShortcutModel(this);
	// This is also a gross way to access this model, but since these models are
	// not first-class models and I am not rewriting them right now, this is how
	// it will be in order to get the “Restore defaults” hooked up to everything
	m_actionShortcutsModel = shortcutsModel;
	shortcutsModel->loadShortcuts(settings.shortcuts());
	connect(
		shortcutsModel, &QAbstractItemModel::dataChanged, &settings,
		[=, &settings] {
			settings.setShortcuts(shortcutsModel->saveShortcuts());
		});
	connect(
		&dpApp(), &DrawpileApp::shortcutsChanged, shortcutsModel,
		&CustomShortcutModel::updateShortcuts);
	m_actionsTable = ProportionalTableView::make(
		m_filter, int(CustomShortcutModel::FilterRole), shortcutsModel);
	m_actionsTable->setColumnStretches({6, 2, 2, 2});
	utils::bindKineticScrolling(m_actionsTable);

	m_actionsTable->setItemDelegateForColumn(1, keySequenceDelegate);
	m_actionsTable->setItemDelegateForColumn(2, keySequenceDelegate);

	layout->addWidget(m_actionsTable, 1);

	utils::EncapsulatedLayout *actions = new utils::EncapsulatedLayout;
	actions->setContentsMargins(0, 0, 0, 0);
	actions->setSpacing(0);
	actions->addStretch();

	QPushButton *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=, &settings] {
		QMessageBox *box = utils::showQuestion(
			this, tr("Restore Shortcut Defaults"),
			tr("Really restore all shortcuts to their default values?"));
		connect(box, &QMessageBox::accepted, this, [this, &settings] {
			settings.setShortcuts({});
			m_actionShortcutsModel->loadShortcuts({});
		});
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

QWidget *Shortcuts::initBrushShortcuts(QStyledItemDelegate *keySequenceDelegate)
{
	int thumbnailSize = brushes::BrushPresetModel::THUMBNAIL_SIZE;
	brushes::BrushPresetTagModel *tagModel = dpApp().brushPresets();
	m_brushShortcutsModel = new BrushShortcutModel(
		tagModel->presetModel(), QSize(thumbnailSize, thumbnailSize), this);
	m_brushShortcutsFilterModel = new BrushShortcutFilterProxyModel(tagModel);
	connect(
		m_filter, &ShortcutFilterInput::conflictBoxChecked,
		m_brushShortcutsFilterModel,
		&BrushShortcutFilterProxyModel::setSearchAllTags);

	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	QComboBox *tagCombo = new QComboBox;
	tagCombo->setEditable(false);
	tagCombo->setModel(tagModel);
	layout->addWidget(tagCombo);
	connect(
		tagCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		m_brushShortcutsFilterModel,
		&BrushShortcutFilterProxyModel::setCurrentTagRow);
	connect(
		m_filter, &ShortcutFilterInput::conflictBoxChecked, tagCombo,
		&QComboBox::setDisabled);

	m_brushesTable = ProportionalTableView::make(
		m_filter, int(BrushShortcutModel::FilterRole), m_brushShortcutsModel,
		m_brushShortcutsFilterModel, false);
	connect(
		m_filter, &ShortcutFilterInput::filtered, m_brushShortcutsFilterModel,
		&BrushShortcutFilterProxyModel::setSearchString);
	m_brushesTable->setColumnStretches({6, 2});
	m_brushesTable->verticalHeader()->setSectionResizeMode(
		QHeaderView::ResizeToContents);
	utils::bindKineticScrolling(m_brushesTable);

	m_brushesTable->setItemDelegateForColumn(1, keySequenceDelegate);

	layout->addWidget(m_brushesTable, 1);

	return widget;
}

static QModelIndex
mapFromView(const QAbstractItemView *view, const QModelIndex &index)
{
	if(const auto *model =
		   qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapToSource(index);
	}
	return index;
}

static QModelIndex
mapToView(const QAbstractItemView *view, const QModelIndex &index)
{
	if(const auto *model =
		   qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapFromSource(index);
	}
	return index;
}

static void execCanvasShortcutDialog(
	QTableView *view, const CanvasShortcuts::Shortcut *shortcut,
	CanvasShortcutsModel *model, QWidget *parent, const QString &title,
	std::function<QModelIndex(CanvasShortcuts::Shortcut)> &&callback)
{
	dialogs::CanvasShortcutsDialog *editor =
		new dialogs::CanvasShortcutsDialog(shortcut, *model, parent);
	editor->setAttribute(Qt::WA_DeleteOnClose);
	editor->setWindowTitle(title);
	editor->setWindowModality(Qt::WindowModal);
	QObject::connect(
		editor, &dialogs::CanvasShortcutsDialog::accepted, view,
		[editor, view, callback] {
			auto row = std::invoke(callback, editor->shortcut());
			if(row.isValid()) {
				view->selectRow(mapToView(view, row).row());
			}
		});
	editor->show();
}

QWidget *Shortcuts::initCanvasShortcuts(desktop::settings::Settings &settings)
{
	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	m_canvasShortcutsModel = new CanvasShortcutsModel(this);
	m_canvasShortcutsModel->loadShortcuts(settings.canvasShortcuts());
	std::function<void()> updateModelFn = [=, &settings] {
		settings.setCanvasShortcuts(m_canvasShortcutsModel->saveShortcuts());
	};
	ON_ANY_MODEL_CHANGE(m_canvasShortcutsModel, &settings, updateModelFn);

	m_canvasTable = ProportionalTableView::make(
		m_filter, int(CanvasShortcutsModel::FilterRole),
		m_canvasShortcutsModel);
	m_canvasTable->setColumnStretches({6, 3, 3, 0});
	utils::bindKineticScrolling(m_canvasTable);
	connect(
		m_canvasTable, &QAbstractItemView::activated, this,
		[=](const QModelIndex &index) {
			if(const CanvasShortcuts::Shortcut *shortcut =
				   m_canvasShortcutsModel->shortcutAt(
					   mapFromView(m_canvasTable, index).row())) {
				execCanvasShortcutDialog(
					m_canvasTable, shortcut, m_canvasShortcutsModel, this,
					tr("Edit Canvas Shortcut"),
					[=](const CanvasShortcuts::Shortcut &newShortcut) {
						return m_canvasShortcutsModel->editShortcut(
							*shortcut, newShortcut);
					});
			}
		});
	layout->addWidget(m_canvasTable, 1);

	utils::EncapsulatedLayout *actions = listActions(
		m_canvasTable, tr("Add"), tr("Add canvas shortcut…"),
		[this] {
			execCanvasShortcutDialog(
				m_canvasTable, nullptr, m_canvasShortcutsModel, this,
				tr("New Canvas Shortcut"),
				[this](const CanvasShortcuts::Shortcut &newShortcut) {
					return m_canvasShortcutsModel->addShortcut(newShortcut);
				});
		},
		tr("Edit"), tr("Edit selected canvas shortcut…"),
		[this] {
			if(const CanvasShortcuts::Shortcut *shortcut =
				   m_canvasShortcutsModel->shortcutAt(
					   mapFromView(m_canvasTable, m_canvasTable->currentIndex())
						   .row())) {
				execCanvasShortcutDialog(
					m_canvasTable, shortcut, m_canvasShortcutsModel, this,
					tr("Edit Canvas Shortcut"),
					[=](const CanvasShortcuts::Shortcut &newShortcut) {
						return m_canvasShortcutsModel->editShortcut(
							*shortcut, newShortcut);
					});
			}
		},
		tr("Remove"), tr("Remove selected canvas shortcut…"),
		makeDefaultDeleter(
			this, m_canvasTable, tr("Remove canvas shortcut"),
			QT_TR_N_NOOP("Really remove %n canvas shortcut(s)?")));
	actions->addStretch();

	QPushButton *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [this] {
		QMessageBox *box = utils::showQuestion(
			this, tr("Restore Canvas Shortcut Defaults"),
			tr("Really restore all canvas shortcuts to their default values?"));
		connect(box, &QMessageBox::accepted, this, [this] {
			m_canvasShortcutsModel->restoreDefaults();
			m_actionShortcutsModel->loadShortcuts({});
		});
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

void Shortcuts::updateConflicts()
{
	if(!m_updatingConflicts) {
		QScopedValueRollback<bool> rollback(m_updatingConflicts, true);

		QSet<QKeySequence> actionKeySequences;
		int actionShortcutCount = m_actionShortcutsModel->rowCount();
		for(int i = 0; i < actionShortcutCount; ++i) {
			const CustomShortcut &cs = m_actionShortcutsModel->shortcutAt(i);
			for(const QKeySequence &shortcut :
				{cs.currentShortcut, cs.alternateShortcut}) {
				if(!shortcut.isEmpty()) {
					actionKeySequences.insert(shortcut);
				}
			}
		}

		QSet<QKeySequence> brushKeySequences;
		int brushShortcutCount = m_brushShortcutsModel->rowCount();
		for(int i = 0; i < brushShortcutCount; ++i) {
			brushKeySequences.insert(m_brushShortcutsModel->shortcutAt(i));
		}

		QSet<QKeySequence> canvasKeySequences;
		int canvasShortcutCount = m_canvasShortcutsModel->rowCount();
		for(int i = 0; i < canvasShortcutCount; ++i) {
			const CanvasShortcuts::Shortcut *s =
				m_canvasShortcutsModel->shortcutAt(i);
			if(s) {
				canvasKeySequences += s->keySequences();
			}
		}

		m_actionShortcutsModel->setExternalKeySequences(
			brushKeySequences + canvasKeySequences);
		m_brushShortcutsModel->setExternalKeySequences(
			actionKeySequences + canvasKeySequences);
		m_canvasShortcutsModel->setExternalKeySequences(
			actionKeySequences + brushKeySequences);

		QTabBar *bar = m_tabs->tabBar();
		QColor conflictColor = m_tabs->palette().buttonText().color();
		conflictColor.setRedF(conflictColor.redF() * 0.5 + 0.5);
		conflictColor.setGreenF(conflictColor.greenF() * 0.5);
		conflictColor.setBlueF(conflictColor.blueF() * 0.5);
		bar->setTabTextColor(
			ACTION_TAB,
			m_actionShortcutsModel->hasConflicts() ? conflictColor : QColor());
		bar->setTabTextColor(
			BRUSH_TAB,
			m_brushShortcutsModel->hasConflicts() ? conflictColor : QColor());
		bar->setTabTextColor(
			CANVAS_TAB,
			m_canvasShortcutsModel->hasConflicts() ? conflictColor : QColor());

		updateTabTexts();
	}
}

void Shortcuts::updateTabTexts()
{
	if(m_filter->isEmpty()) {
		m_tabs->setTabText(ACTION_TAB, actionTabText());
		m_tabs->setTabText(BRUSH_TAB, brushTabText());
		m_tabs->setTabText(CANVAS_TAB, canvasTabText());
	} else {
		m_tabs->setTabText(
			ACTION_TAB,
			searchResultText(
				actionTabText(), m_actionsTable->model()->rowCount()));
		m_tabs->setTabText(
			BRUSH_TAB,
			searchResultText(
				brushTabText(), m_brushesTable->model()->rowCount()));
		m_tabs->setTabText(
			CANVAS_TAB,
			searchResultText(
				canvasTabText(), m_canvasTable->model()->rowCount()));
	}
}

QString Shortcuts::actionTabText()
{
	return tr("Actions");
}

QString Shortcuts::brushTabText()
{
	return tr("Brushes");
}

QString Shortcuts::canvasTabText()
{
	return tr("Canvas");
}

QString Shortcuts::searchResultText(const QString &text, int results)
{
	//: This is for showing search feedback in the tabs of the shortcut
	//: preferences. %1 is the original tab title, like "Actions" or "Brushes",
	//: %2 is the number of search results in that tab.
	return tr("%1 (%2)").arg(text).arg(results);
}

} // namespace settingsdialog
} // namespace dialogs
