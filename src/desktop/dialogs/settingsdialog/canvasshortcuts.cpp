// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/canvasshortcuts.h"
#include "desktop/dialogs/canvasshortcutsdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
#include "desktop/settings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/canvasshortcutsmodel.h"
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

typedef struct CanvasShortcuts::Shortcut Shortcut;

namespace dialogs {
namespace settingsdialog {

CanvasShortcuts::CanvasShortcuts(
	desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	auto *filter = new QLineEdit;
	filter->setClearButtonEnabled(true);
	filter->setPlaceholderText(tr("Search actions…"));
	filter->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	initCanvasShortcuts(settings, layout, filter);
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
	QTableView *view, const Shortcut *shortcut, CanvasShortcutsModel *model,
	QWidget *parent, const QString &title,
	std::function<QModelIndex(Shortcut)> &&callback)
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

void CanvasShortcuts::initCanvasShortcuts(
	desktop::settings::Settings &settings, QVBoxLayout *layout,
	QLineEdit *filter)
{
	layout->addWidget(filter);

	auto *shortcutsModel = new CanvasShortcutsModel(this);
	shortcutsModel->loadShortcuts(settings.canvasShortcuts());
	onAnyModelChange(shortcutsModel, &settings, [=, &settings] {
		settings.setCanvasShortcuts(shortcutsModel->saveShortcuts());
	});

	auto *shortcuts = ProportionalTableView::make(filter, shortcutsModel);
	shortcuts->setColumnStretches({6, 3, 3, 0});
	connect(
		shortcuts, &QAbstractItemView::activated, this,
		[=](const QModelIndex &index) {
			if(const auto *shortcut = shortcutsModel->shortcutAt(
				   mapFromView(shortcuts, index).row())) {
				execCanvasShortcutDialog(
					shortcuts, shortcut, shortcutsModel, this,
					tr("Edit Canvas Shortcut"), [=](auto newShortcut) {
						return shortcutsModel->editShortcut(
							*shortcut, newShortcut);
					});
			}
		});
	layout->addWidget(shortcuts, 1);

	auto *actions = listActions(
		shortcuts, tr("Add canvas shortcut…"),
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
			QT_TR_N_NOOP("Really remove %n canvas shortcut(s)?")));
	actions->addStretch();

	auto *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=] {
		const auto confirm = execConfirm(
			tr("Restore Canvas Shortcut Defaults"),
			tr("Really restore all canvas shortcuts to their default values?"),
			this);
		if(confirm) {
			shortcutsModel->restoreDefaults();
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);
}

} // namespace settingsdialog
} // namespace dialogs
