#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_HELPERS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_HELPERS_H

#include "desktop/utils/sanerformlayout.h"
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

inline QLabel *makeIconLabel(QStyle::StandardPixmap icon, QStyle::PixelMetric size, QWidget *parent = nullptr)
{
	auto *label = new QLabel;
	auto *widget = parent ? parent : label;
	auto *style = widget->style();
	auto labelIcon = style->standardIcon(icon, nullptr, widget);
	auto labelSize = style->pixelMetric(size, nullptr, widget);
	label->setPixmap(labelIcon.pixmap(labelSize));
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return label;
}

template <typename AddCallback, typename RemoveCallback>
utils::EncapsulatedLayout *listActions(
	QAbstractItemView *view,
	const QString &addLabel,
	AddCallback addCallback,
	const QString &removeLabel,
	RemoveCallback removeCallback
)
{
	auto *buttons = new utils::EncapsulatedLayout;
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->setSpacing(0);

	auto *add = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	add->setText(addLabel);
	add->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(add, &widgets::GroupedToolButton::clicked, view, addCallback);
	buttons->addWidget(add);

	auto *remove = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	remove->setText(removeLabel);
	remove->setIcon(QIcon::fromTheme("list-remove"));
	remove->setEnabled(view->selectionModel()->hasSelection());
	QObject::connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, view, [=] {
		remove->setEnabled(view->selectionModel()->hasSelection());
	});
	QObject::connect(remove, &widgets::GroupedToolButton::clicked, view, removeCallback);
	buttons->addWidget(remove);
	buttons->addStretch();

	return buttons;
}

inline bool execConfirm(const QString &title, const QString &message, QWidget *owner)
{
	QMessageBox box(
		QMessageBox::Question,
		title,
		message,
		QMessageBox::Yes | QMessageBox::No,
		owner
	);
	box.setDefaultButton(QMessageBox::No);
	box.setWindowModality(Qt::WindowModal);
	return box.exec() == QMessageBox::Yes;
}

inline int execWarning(
	const QString &title, const QString &message, QWidget *owner,
	QMessageBox::StandardButtons buttons = QMessageBox::Ok,
	QMessageBox::StandardButton defaultButton = QMessageBox::Ok
)
{
	QMessageBox box(
		QMessageBox::Warning,
		title,
		message,
		buttons,
		owner
	);
	box.setDefaultButton(defaultButton);
	box.setWindowModality(Qt::WindowModal);
	return box.exec();
}

template <typename Widget, typename Fn>
auto makeDefaultDeleter(Widget *owner, QAbstractItemView *view, const QString &title, const char *message, Fn fn)
{
	return [=] {
		auto selection = view->selectionModel()->selectedRows();

		const auto confirm = execConfirm(
			title,
			Widget::tr(message, nullptr, selection.size()),
			owner
		);

		if (!confirm) {
			return;
		}

		// Any list-based model is going to have a bad time if we do not sort
		// and do not invalidate
		std::sort(
			selection.begin(), selection.end(),
			[](QModelIndex a, QModelIndex b) { return b < a; }
		);

		auto *model = view->model();
		for (const auto &index : selection) {
			if (fn(index)) {
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
auto makeDefaultDeleter(Widget *owner, QAbstractItemView *view, const QString &title, const char *message)
{
	return makeDefaultDeleter(owner, view, title, message, [](const QModelIndex &) {
		return true;
	});
}

} // namespace settingsdialog
} // namespace dialogs

#endif
