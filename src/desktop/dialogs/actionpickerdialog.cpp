// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/actionpickerdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include "libclient/utils/customshortcutmodel.h"
#include <QDialogButtonBox>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

namespace dialogs {

ActionPickerDialog::ActionPickerDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Select Action"));
	utils::makeModal(this);
	resize(400, 500);

	QVBoxLayout *dlgLayout = new QVBoxLayout;
	setLayout(dlgLayout);

	m_filterEdit = new QLineEdit;
	m_filterEdit->setClearButtonEnabled(true);
	m_filterEdit->setPlaceholderText(
		QCoreApplication::translate(
			"dialogs::settingsdialog::ShortcutFilterInput", "Search…"));
	m_filterEdit->addAction(
		QIcon::fromTheme(QStringLiteral("edit-find")),
		QLineEdit::LeadingPosition);
	dlgLayout->addWidget(m_filterEdit);

	m_customShortcutModel = new CustomShortcutModel(this);
	m_customShortcutModel->setShowIcons(false);
	m_customShortcutModel->loadShortcuts(dpAppConfig()->getShortcuts());

	m_proxyModel = new QSortFilterProxyModel(this);
	m_proxyModel->setSourceModel(m_customShortcutModel);
	m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_proxyModel->setFilterRole(CustomShortcutModel::FilterRole);
	m_proxyModel->setSortRole(Qt::DisplayRole);
	m_proxyModel->sort(0);
	connect(
		m_filterEdit, &QLineEdit::textChanged, m_proxyModel,
		&QSortFilterProxyModel::setFilterFixedString);

	m_listView = new QListView;
	m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_listView->setModel(m_proxyModel);
	dlgLayout->addWidget(m_listView, 1);
	connect(
		m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &ActionPickerDialog::updateButtons);

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	dlgLayout->addWidget(m_buttons);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this,
		&ActionPickerDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&ActionPickerDialog::reject);

	updateButtons();
}

void ActionPickerDialog::setSelectedAction(const QString &name)
{
	QModelIndex idx;
	if(!name.isEmpty()) {
		idx = m_customShortcutModel->searchByActionName(name);
	}

	if(idx.isValid()) {
		idx = m_proxyModel->mapFromSource(idx);
	}

	if(idx.isValid()) {
		m_listView->selectionModel()->setCurrentIndex(
			idx, QItemSelectionModel::ClearAndSelect);
	} else {
		m_listView->selectionModel()->clearSelection();
	}
}

void ActionPickerDialog::accept()
{
	QModelIndexList selected = m_listView->selectionModel()->selectedRows();
	if(!selected.isEmpty()) {
		Q_EMIT(actionSelected(selected.constFirst()
								  .data(CustomShortcutModel::ActionNameRole)
								  .toString()));
	}
	QDialog::accept();
}

void ActionPickerDialog::updateButtons()
{
	m_buttons->button(QDialogButtonBox::Ok)
		->setEnabled(!m_listView->selectionModel()->selectedRows().isEmpty());
}

}
