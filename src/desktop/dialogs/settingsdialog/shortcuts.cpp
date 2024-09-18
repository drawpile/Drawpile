// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/canvasshortcutsdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
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
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

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
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	m_filterEdit = new QLineEdit;
	m_filterEdit->setClearButtonEnabled(true);
	m_filterEdit->setPlaceholderText(tr("Search…"));
	m_filterEdit->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	layout->addWidget(m_filterEdit);

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

	connect(
		m_filterEdit, &QLineEdit::textChanged, this, &Shortcuts::updateTabTexts,
		Qt::QueuedConnection);
	updateTabTexts();
}

void Shortcuts::finishEditing()
{
	m_actionsTable->setCurrentIndex(QModelIndex{});
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
	m_globalShortcutsModel = shortcutsModel;
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
		m_filterEdit, int(CustomShortcutModel::FilterRole), shortcutsModel);
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
		bool confirm = execConfirm(
			tr("Restore Shortcut Defaults"),
			tr("Really restore all shortcuts to their default values?"), this);
		if(confirm) {
			settings.setShortcuts({});
			m_globalShortcutsModel->loadShortcuts({});
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

QWidget *Shortcuts::initBrushShortcuts(QStyledItemDelegate *keySequenceDelegate)
{
	int thumbnailSize = brushes::BrushPresetModel::THUMBNAIL_SIZE;
	m_brushShortcutsModel = new BrushShortcutModel(
		dpApp().brushPresets()->presetModel(),
		QSize(thumbnailSize, thumbnailSize), this);

	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	m_brushesTable = ProportionalTableView::make(
		m_filterEdit, int(BrushShortcutModel::FilterRole),
		m_brushShortcutsModel);
	m_brushesTable->setColumnStretches({6, 2});
	m_brushesTable->verticalHeader()->setSectionResizeMode(
		QHeaderView::ResizeToContents);
	utils::bindKineticScrolling(m_brushesTable);

	m_brushesTable->setItemDelegateForColumn(1, keySequenceDelegate);

	layout->addWidget(m_brushesTable, 1);

	return widget;
}

template <typename Fn>
static void
onAnyModelChange(QAbstractItemModel *model, QObject *receiver, Fn fn)
{
	QObject::connect(model, &QAbstractItemModel::dataChanged, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::modelReset, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsInserted, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsRemoved, receiver, fn);
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
	dialogs::CanvasShortcutsDialog editor(shortcut, *model, parent);
	editor.setWindowTitle(title);
	editor.setWindowModality(Qt::WindowModal);
	if(editor.exec() == QDialog::Accepted) {
		auto row = std::invoke(callback, editor.shortcut());
		if(row.isValid()) {
			view->selectRow(mapToView(view, row).row());
		}
	}
}

QWidget *Shortcuts::initCanvasShortcuts(desktop::settings::Settings &settings)
{
	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	CanvasShortcutsModel *shortcutsModel = new CanvasShortcutsModel(this);
	shortcutsModel->loadShortcuts(settings.canvasShortcuts());
	onAnyModelChange(shortcutsModel, &settings, [=, &settings] {
		settings.setCanvasShortcuts(shortcutsModel->saveShortcuts());
	});

	m_canvasTable = ProportionalTableView::make(
		m_filterEdit, int(CanvasShortcutsModel::FilterRole), shortcutsModel);
	m_canvasTable->setColumnStretches({6, 3, 3, 0});
	utils::bindKineticScrolling(m_canvasTable);
	connect(
		m_canvasTable, &QAbstractItemView::activated, this,
		[=](const QModelIndex &index) {
			if(const auto *shortcut = shortcutsModel->shortcutAt(
				   mapFromView(m_canvasTable, index).row())) {
				execCanvasShortcutDialog(
					m_canvasTable, shortcut, shortcutsModel, this,
					tr("Edit Canvas Shortcut"), [=](auto newShortcut) {
						return shortcutsModel->editShortcut(
							*shortcut, newShortcut);
					});
			}
		});
	layout->addWidget(m_canvasTable, 1);

	utils::EncapsulatedLayout *actions = listActions(
		m_canvasTable, tr("Add canvas shortcut…"),
		[=] {
			execCanvasShortcutDialog(
				m_canvasTable, nullptr, shortcutsModel, this,
				tr("New Canvas Shortcut"), [=](auto newShortcut) {
					return shortcutsModel->addShortcut(newShortcut);
				});
		},
		tr("Remove selected canvas shortcut…"),
		makeDefaultDeleter(
			this, m_canvasTable, tr("Remove canvas shortcut"),
			QT_TR_N_NOOP("Really remove %n canvas shortcut(s)?")));
	actions->addStretch();

	QPushButton *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=] {
		bool confirm = execConfirm(
			tr("Restore Canvas Shortcut Defaults"),
			tr("Really restore all canvas shortcuts to their default values?"),
			this);
		if(confirm) {
			shortcutsModel->restoreDefaults();
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

void Shortcuts::updateTabTexts()
{
	if(m_filterEdit->text().isEmpty()) {
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
