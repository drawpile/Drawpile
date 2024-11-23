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

inline void setUpToolButton(
	QToolButton *button, const QIcon &icon, const QString &text,
	const QString &toolTip)
{
	button->setIcon(icon);
	button->setToolButtonStyle(
		text.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
	button->setText(text);
	button->setToolTip(toolTip);
}

template <
	typename AddCallback, typename EditCallback, typename RemoveCallback,
	typename MoveUpCallback, typename MoveDownCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addText, const QString &addToolTip,
	AddCallback addCallback, const QString &editText,
	const QString &editToolTip, EditCallback editCallback,
	const QString &removeText, const QString &removeToolTip,
	RemoveCallback removeCallback, const QString &moveUpText,
	const QString &moveUpToolTip, MoveUpCallback moveUpCallback,
	const QString &moveDownText, const QString &moveDownToolTip,
	MoveDownCallback moveDownCallback, bool includeEdit, bool includeMove)
{
	utils::EncapsulatedLayout *buttons = new utils::EncapsulatedLayout;
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->setSpacing(0);

	widgets::GroupedToolButton *add =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	setUpToolButton(add, QIcon::fromTheme("list-add"), addText, addToolTip);
	QObject::connect(
		add, &widgets::GroupedToolButton::clicked, view, addCallback);
	buttons->addWidget(add);

	if(includeEdit) {
		widgets::GroupedToolButton *edit = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupCenter);
		setUpToolButton(
			edit, QIcon::fromTheme("edit-rename"), editText, editToolTip);
		edit->setEnabled(view->selectionModel()->hasSelection());
		QObject::connect(
			view->selectionModel(), &QItemSelectionModel::selectionChanged,
			view, [=] {
				edit->setEnabled(view->selectionModel()->hasSelection());
			});
		QObject::connect(
			edit, &widgets::GroupedToolButton::clicked, view, editCallback);
		buttons->addWidget(edit);
	}

	widgets::GroupedToolButton *remove = new widgets::GroupedToolButton(
		includeMove ? widgets::GroupedToolButton::GroupCenter
					: widgets::GroupedToolButton::GroupRight);
	setUpToolButton(
		remove, QIcon::fromTheme("list-remove"), removeText, removeToolTip);
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
		widgets::GroupedToolButton *moveUp = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupCenter);
		setUpToolButton(
			moveUp, QIcon::fromTheme("arrow-up"), moveUpText, moveUpToolTip);
		moveUp->setEnabled(view->selectionModel()->hasSelection());
		std::function<void()> updateMoveUp = [=] {
			QItemSelectionModel *selectionModel = view->selectionModel();
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

		widgets::GroupedToolButton *moveDown = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupRight);
		setUpToolButton(
			moveDown, QIcon::fromTheme("arrow-down"), moveDownText,
			moveDownToolTip);
		moveDown->setEnabled(view->selectionModel()->hasSelection());
		std::function<void()> updateMoveDown = [=] {
			QItemSelectionModel *selectionModel = view->selectionModel();
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

template <
	typename AddCallback, typename RemoveCallback, typename MoveUpCallback,
	typename MoveDownCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addText, const QString &addToolTip,
	AddCallback addCallback, const QString &removeText,
	const QString &removeToolTip, RemoveCallback removeCallback,
	const QString &moveUpText, const QString &moveUpToolTip,
	MoveUpCallback moveUpCallback, const QString &moveDownText,
	const QString &moveDownToolTip, MoveDownCallback moveDownCallback)
{
	void (*noop)() = nullptr;
	return listActions(
		view, addText, addToolTip, addCallback, QString(), QString(), noop,
		removeText, removeToolTip, removeCallback, moveUpText, moveUpToolTip,
		moveUpCallback, moveDownText, moveDownToolTip, moveDownCallback, false,
		true);
}

template <typename AddCallback, typename EditCallback, typename RemoveCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addText, const QString &addToolTip,
	AddCallback addCallback, const QString &editText,
	const QString &editToolTip, EditCallback editCallback,
	const QString &removeText, const QString &removeToolTip,
	RemoveCallback removeCallback)
{
	void (*noop)() = nullptr;
	return listActions(
		view, addText, addToolTip, addCallback, editText, editToolTip,
		editCallback, removeText, removeToolTip, removeCallback, QString(),
		QString(), noop, QString(), QString(), noop, true, false);
}

template <typename AddCallback, typename RemoveCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view, const QString &addText, const QString &addToolTip,
	AddCallback addCallback, const QString &removeText,
	const QString &removeToolTip, RemoveCallback removeCallback)
{
	void (*noop)() = nullptr;
	return listActions(
		view, addText, addToolTip, addCallback, QString(), QString(), noop,
		removeText, removeToolTip, removeCallback, QString(), QString(), noop,
		QString(), QString(), noop, false, false);
}

inline bool
execConfirm(const QString &title, const QString &message, QWidget *owner)
{
	QMessageBox box(
		QMessageBox::Question, title, message,
		QMessageBox::Yes | QMessageBox::No, owner);
	box.setDefaultButton(QMessageBox::No);
#ifndef __EMSCRIPTEN__
	box.setWindowModality(Qt::WindowModal);
#endif
	return box.exec() == QMessageBox::Yes;
}

inline int execWarning(
	const QString &title, const QString &message, QWidget *owner,
	QMessageBox::StandardButtons buttons = QMessageBox::Ok,
	QMessageBox::StandardButton defaultButton = QMessageBox::Ok)
{
	QMessageBox box(QMessageBox::Warning, title, message, buttons, owner);
	box.setDefaultButton(defaultButton);
#ifndef __EMSCRIPTEN__
	box.setWindowModality(Qt::WindowModal);
#endif
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
