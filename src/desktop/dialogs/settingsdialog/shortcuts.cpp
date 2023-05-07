// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/canvasshortcutsdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/settings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/keysequenceedit.h"
#include "libclient/utils/canvasshortcutsmodel.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libshared/util/qtcompat.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <functional>

namespace dialogs {
namespace settingsdialog {

Shortcuts::Shortcuts(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	auto *filter = new QLineEdit;
	filter->setClearButtonEnabled(true);
	filter->setPlaceholderText(tr("Search actions…"));
	filter->addAction(QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	initGlobalShortcuts(settings, layout, filter);
	layout->addSpacing(6);
	initCanvasShortcuts(settings, layout, filter);
}

class ProportionalTableView final : public QTableView {
public:
	using Stretches = std::array<int, 4>;

	ProportionalTableView(QWidget *parent = nullptr)
		: QTableView(parent)
	{
		horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

	const Stretches &columnStretches() const { return m_stretches; }
	void setColumnStretches(const Stretches &stretches)
	{
		m_stretches = stretches;
		m_stretchSum = std::accumulate(m_stretches.begin(), m_stretches.end(), 0);
		update();
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QTableView::resizeEvent(event);
		const auto width = viewport()->width();
		const auto size = m_stretches.size();
		auto *header = horizontalHeader();
		auto remainder = width;
		for (auto i = 1UL; i < size; ++i) {
			const auto columnSize = width * m_stretches[i] / m_stretchSum;
			remainder -= columnSize;
			header->resizeSection(i, columnSize);
		}
		header->resizeSection(0, remainder);
	}
private:
	Stretches m_stretches = {1, 1, 1, 1};
	int m_stretchSum = 4;
};

static ProportionalTableView *makeTableView(QLineEdit *filter, QAbstractItemModel *model)
{
	auto *view = new ProportionalTableView;
	view->setCornerButtonEnabled(false);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setFocusPolicy(Qt::StrongFocus);
	view->setAlternatingRowColors(true);
	view->setSortingEnabled(true);
	view->setWordWrap(false);
	view->setEditTriggers(QAbstractItemView::AllEditTriggers);

	auto *filterModel = new QSortFilterProxyModel(view);
	filterModel->setSourceModel(model);
	filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	QObject::connect(filter, &QLineEdit::textChanged, filterModel, &QSortFilterProxyModel::setFilterFixedString);
	view->setModel(filterModel);
	view->sortByColumn(0, Qt::AscendingOrder);

	auto *header = view->horizontalHeader();
	header->setSectionsMovable(false);
	header->setHighlightSections(false);
	view->verticalHeader()->hide();

	return view;
}

template <typename Fn>
static void onAnyModelChange(QAbstractItemModel *model, QObject *receiver, Fn fn)
{
	QObject::connect(model, &QAbstractItemModel::dataChanged, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::modelReset, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsInserted, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsRemoved, receiver, fn);
}

static QModelIndex mapFromView(const QAbstractItemView *view, const QModelIndex &index)
{
	if (const auto *model = qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapToSource(index);
	}
	return index;
}

static QModelIndex mapToView(const QAbstractItemView *view, const QModelIndex &index)
{
	if (const auto *model = qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapFromSource(index);
	}
	return index;
}

static void execCanvasShortcutDialog(QTableView *view, const CanvasShortcuts::Shortcut *shortcut, CanvasShortcutsModel *model, QWidget *parent, const QString &title, std::function<QModelIndex(CanvasShortcuts::Shortcut)> &&callback)
{
	dialogs::CanvasShortcutsDialog editor(shortcut, *model, parent);
	editor.setWindowTitle(title);
	editor.setWindowModality(Qt::WindowModal);
	if(editor.exec() == QDialog::Accepted) {
		auto row = std::invoke(callback, editor.shortcut());
		if (row.isValid()) {
			view->selectRow(mapToView(view, row).row());
		}
	}
}

void Shortcuts::initCanvasShortcuts(desktop::settings::Settings &settings, QVBoxLayout *layout, QLineEdit *filter)
{
	auto *shortcutsLabel = new QLabel(tr("Canvas shortcuts:"));
	layout->addWidget(shortcutsLabel);

	auto *shortcutsModel = new CanvasShortcutsModel(this);
	shortcutsModel->loadShortcuts(settings.canvasShortcuts());
	onAnyModelChange(shortcutsModel, &settings, [=, &settings] {
		settings.setCanvasShortcuts(shortcutsModel->saveShortcuts());
	});

	auto *shortcuts = makeTableView(filter, shortcutsModel);
	shortcutsLabel->setBuddy(shortcuts);
	shortcuts->setColumnStretches({6, 3, 3, 0});
	connect(
		shortcuts, &QAbstractItemView::activated,
		this, [=](const QModelIndex &index) {
		if (const auto *shortcut = shortcutsModel->shortcutAt(mapFromView(shortcuts, index).row())) {
			execCanvasShortcutDialog(
				shortcuts, shortcut, shortcutsModel, this,
				tr("Edit Canvas Shortcut"),
				[=](auto newShortcut) {
					return shortcutsModel->editShortcut(*shortcut, newShortcut);
				});
		}
	});
	layout->addWidget(shortcuts, 1);

	auto *actions = listActions(
		shortcuts,
		tr("Add canvas shortcut…"),
		[=] {
			execCanvasShortcutDialog(
				shortcuts, nullptr, shortcutsModel, this,
				tr("New Canvas Shortcut"), [=](auto newShortcut) {
				return shortcutsModel->addShortcut(newShortcut);
			});
		},
		tr("Remove selected canvas shortcut…"),
		makeDefaultDeleter(
			this, shortcuts, tr("Remove canvas shortcut"),
			QT_TR_NOOP("Really remove %n canvas shortcuts?")
		)
	);
	actions->addStretch();

	auto *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(
		restoreDefaults, &QPushButton::clicked,
		this, [=, &settings] {
			const auto confirm = execConfirm(
				tr("Restore Canvas Shortcut Defaults"),
				tr("Really restore all shortcuts to their default values?"),
				this
			);
			if (confirm) {
				shortcutsModel->restoreDefaults();
				settings.setShortcuts({});
				m_globalShortcutsModel->loadShortcuts({});
			}
		}
	);
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);
}

class KeySequenceEditFactory final : public QItemEditorCreatorBase
{
public:
	QWidget *createWidget(QWidget *parent) const override
	{
		return new widgets::KeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const override
	{
		return "keySequence";
	}
};

void Shortcuts::initGlobalShortcuts(desktop::settings::Settings &settings, QVBoxLayout *layout, QLineEdit *filter)
{
	// Shoving the filter in here kind of sucks but it is the least ugly option
	// that I can think of at the moment for the UI
	auto *header = new QHBoxLayout;
	header->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(header);

	auto *shortcutsLabel = new QLabel(tr("Global shortcuts:"));
	header->addWidget(shortcutsLabel);
	header->addStretch();
	header->addWidget(filter);

	auto *shortcutsModel = new CustomShortcutModel(this);
	// This is also a gross way to access this model, but since these models are
	// not first-class models and I am not rewriting them right now, this is how
	// it will be in order to get the “Restore defaults” hooked up to everything
	m_globalShortcutsModel = shortcutsModel;
	shortcutsModel->loadShortcuts(settings.shortcuts());
	connect(shortcutsModel, &QAbstractItemModel::dataChanged, &settings, [=, &settings] {
		settings.setShortcuts(shortcutsModel->saveShortcuts());
	});
	auto *shortcuts = makeTableView(filter, shortcutsModel);
	shortcuts->setColumnStretches({6, 2, 2, 2});

	QStyledItemDelegate *keySequenceDelegate = new QStyledItemDelegate(this);
	m_itemEditorFactory.registerEditor(compat::metaTypeFromName("QKeySequence"), new KeySequenceEditFactory);
	keySequenceDelegate->setItemEditorFactory(&m_itemEditorFactory);
	shortcuts->setItemDelegateForColumn(1, keySequenceDelegate);
	shortcuts->setItemDelegateForColumn(2, keySequenceDelegate);

	layout->addWidget(shortcuts, 1);
}

} // namespace settingsdialog
} // namespace dialogs
