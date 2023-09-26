#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_HELPERS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_HELPERS_H
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QAbstractItemView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QObject>
#include <QSizePolicy>
#include <QString>
#include <QStyle>
#include <QWidget>
#include <Qt>
#include <algorithm>

namespace dialogs {
namespace settingsdialog {

template <
	typename AddCallback, typename RemoveCallback, typename MoveUpCallback,
	typename MoveDownCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addLabel, AddCallback addCallback,
	const QString &removeLabel, RemoveCallback removeCallback,
	const QString &moveUpLabel, MoveUpCallback moveUpCallback,
	const QString &moveDownLabel, MoveDownCallback moveDownCallback,
	bool includeMove = true)
{
	auto *buttons = new utils::EncapsulatedLayout;
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->setSpacing(0);

	auto *add =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	add->setText(addLabel);
	add->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(
		add, &widgets::GroupedToolButton::clicked, view, addCallback);
	buttons->addWidget(add);

	auto *remove = new widgets::GroupedToolButton(
		includeMove ? widgets::GroupedToolButton::GroupCenter
					: widgets::GroupedToolButton::GroupRight);
	remove->setText(removeLabel);
	remove->setIcon(QIcon::fromTheme("list-remove"));
	remove->setEnabled(view->selectionModel()->hasSelection());
	QObject::connect(
		view->selectionModel(), &QItemSelectionModel::selectionChanged, view,
		[=] {
			remove->setEnabled(view->selectionModel()->hasSelection());
		});
	QObject::connect(
		remove, &widgets::GroupedToolButton::clicked, view, removeCallback);
	buttons->addWidget(remove);

	if(includeMove) {
		auto *moveUp = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupCenter);
		moveUp->setText(moveUpLabel);
		moveUp->setIcon(QIcon::fromTheme("arrow-up"));
		moveUp->setEnabled(view->selectionModel()->hasSelection());
		auto updateMoveUp = [=] {
			auto selectionModel = view->selectionModel();
			moveUp->setEnabled(
				selectionModel->hasSelection() &&
				selectionModel->selectedIndexes().size() == 1 &&
				!selectionModel->isRowSelected(0));
		};
		QObject::connect(
			view->selectionModel(), &QItemSelectionModel::selectionChanged,
			view, updateMoveUp);
		QObject::connect(
			view->model(), &QAbstractItemModel::rowsMoved, view, updateMoveUp);
		QObject::connect(
			moveUp, &widgets::GroupedToolButton::clicked, view, moveUpCallback);
		buttons->addWidget(moveUp);

		auto *moveDown = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupRight);
		moveDown->setText(moveDownLabel);
		moveDown->setIcon(QIcon::fromTheme("arrow-down"));
		moveDown->setEnabled(view->selectionModel()->hasSelection());
		auto updateMoveDown = [=] {
			auto selectionModel = view->selectionModel();
			moveDown->setEnabled(
				selectionModel->hasSelection() &&
				selectionModel->selectedIndexes().size() == 1 &&
				!selectionModel->isRowSelected(view->model()->rowCount() - 1));
		};
		QObject::connect(
			view->selectionModel(), &QItemSelectionModel::selectionChanged,
			view, updateMoveDown);
		QObject::connect(
			view->model(), &QAbstractItemModel::rowsMoved, view,
			updateMoveDown);
		QObject::connect(
			moveDown, &widgets::GroupedToolButton::clicked, view,
			moveDownCallback);
		buttons->addWidget(moveDown);
	}

	buttons->addStretch();
	return buttons;
}

template <typename AddCallback, typename RemoveCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addLabel, AddCallback addCallback,
	const QString &removeLabel, RemoveCallback removeCallback)
{
	void (*noop)() = nullptr;
	return listActions(
		view, addLabel, addCallback, removeLabel, removeCallback, QString{},
		noop, QString{}, noop, false);
}

inline bool
execConfirm(const QString &title, const QString &message, QWidget *owner)
{
	QMessageBox box(
		QMessageBox::Question, title, message,
		QMessageBox::Yes | QMessageBox::No, owner);
	box.setDefaultButton(QMessageBox::No);
	box.setWindowModality(Qt::WindowModal);
	return box.exec() == QMessageBox::Yes;
}

inline int execWarning(
	const QString &title, const QString &message, QWidget *owner,
	QMessageBox::StandardButtons buttons = QMessageBox::Ok,
	QMessageBox::StandardButton defaultButton = QMessageBox::Ok)
{
	QMessageBox box(QMessageBox::Warning, title, message, buttons, owner);
	box.setDefaultButton(defaultButton);
	box.setWindowModality(Qt::WindowModal);
	return box.exec();
}

template <typename Widget, typename Fn>
auto makeDefaultDeleter(
	Widget *owner, QAbstractItemView *view, const QString &title,
	const char *message, Fn fn)
{
	return [=] {
		auto selection = view->selectionModel()->selectedRows();

		const auto confirm = execConfirm(
			title, Widget::tr(message, nullptr, selection.size()), owner);

		if(!confirm) {
			return;
		}

		// Any list-based model is going to have a bad time if we do not sort
		// and do not invalidate
		std::sort(
			selection.begin(), selection.end(),
			[](QModelIndex a, QModelIndex b) {
				return b < a;
			});

		auto *model = view->model();
		for(const auto &index : selection) {
			if(fn(index)) {
				model->removeRow(index.row(), index.parent());
			} else {
				model->revert();
				return;
			}
		}

		model->submit();
	};
}

template <typename Widget>
auto makeDefaultDeleter(
	Widget *owner, QAbstractItemView *view, const QString &title,
	const char *message)
{
	return makeDefaultDeleter(
		owner, view, title, message, [](const QModelIndex &) {
			return true;
		});
}

} // namespace settingsdialog
} // namespace dialogs

#endif
