/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "serverlogdialog.h"
#include "canvas/userlist.h"
#include "net/commands.h"
#include "../shared/net/undo.h"
#include "ui_serverlog.h"

#include <QSortFilterProxyModel>

namespace dialogs {

ServerLogDialog::ServerLogDialog(QWidget *parent)
	: QDialog(parent), m_opMode(false)
{
	m_ui = new Ui_ServerLogDialog;
	m_ui->setupUi(this);

	m_eventlogProxy = new QSortFilterProxyModel(this);
	m_ui->view->setModel(m_eventlogProxy);

	m_eventlogProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	connect(m_ui->filter, &QLineEdit::textChanged, m_eventlogProxy, &QSortFilterProxyModel::setFilterFixedString);

	m_userlistProxy = new QSortFilterProxyModel(this);
	m_ui->userlistView->setModel(m_userlistProxy);

	m_userlistProxy->setFilterKeyColumn(0);
	m_userlistProxy->setFilterRole(Qt::DisplayRole);
	m_userlistProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	connect(m_ui->userlistFilter, &QLineEdit::textChanged, m_userlistProxy, &QSortFilterProxyModel::setFilterFixedString);

	connect(m_ui->inspectMode, &QPushButton::toggled, this, &ServerLogDialog::setInspectMode);
	connect(m_ui->kickUser, &QPushButton::clicked, this, &ServerLogDialog::kickSelected);
	connect(m_ui->banUser, &QPushButton::clicked, this, &ServerLogDialog::banSelected);
	connect(m_ui->undoUser, &QPushButton::clicked, this, &ServerLogDialog::undoSelected);
	connect(m_ui->redoUser, &QPushButton::clicked, this, &ServerLogDialog::redoSelected);

	userSelected(QItemSelection());
}

ServerLogDialog::~ServerLogDialog()
{
	delete m_ui;
}

void ServerLogDialog::hideEvent(QHideEvent *event)
{
	QDialog::hideEvent(event);
	m_ui->inspectMode->setChecked(false);
}

void ServerLogDialog::setModel(QAbstractItemModel *model)
{
	m_eventlogProxy->setSourceModel(model);
}

void ServerLogDialog::setUserList(canvas::UserListModel *userlist)
{
	m_userlist = userlist;
	m_userlistProxy->setSourceModel(userlist);

	m_ui->userlistView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_ui->userlistView->horizontalHeader()->setSectionResizeMode(0,	QHeaderView::Stretch);

	connect(m_ui->userlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServerLogDialog::userSelected);
}

void ServerLogDialog::setOperatorMode(bool op)
{
	m_opMode = op;
	if(m_ui->userlistView->selectionModel())
		userSelected(m_ui->userlistView->selectionModel()->selection());
	else
		userSelected(QItemSelection());
}

void ServerLogDialog::userSelected(const QItemSelection &selected)
{
	const bool s = m_opMode && !selected.isEmpty();
	m_ui->kickUser->setEnabled(s && selected.indexes().first().data(canvas::UserListModel::IsOnlineRole).toBool());
	m_ui->banUser->setEnabled(s);
	m_ui->undoUser->setEnabled(s);
	m_ui->redoUser->setEnabled(s);

	setInspectMode(m_ui->inspectMode->isChecked());
}

uint8_t ServerLogDialog::selectedUserId() const
{
	if(!m_ui->userlistView->selectionModel())
		return 0;

	const auto selection = m_ui->userlistView->selectionModel()->selection().indexes();

	return selection.isEmpty() ? 0 : uint8_t(selection.first().data(canvas::UserListModel::IdRole).toInt());
}

void ServerLogDialog::setInspectMode(bool inspect)
{
	if(!m_ui->userlistView->selectionModel())
		return;

	auto selection = selectedUserId();
	if(inspect && selection)
		emit inspectModeChanged(selection);
	else
		emit inspectModeStopped();
}

void ServerLogDialog::kickSelected()
{
	auto user = selectedUserId();
	if(user)
		emit opCommand(net::command::kick(user, false));
}

void ServerLogDialog::banSelected()
{
	auto user = selectedUserId();
	if(user)
		emit opCommand(net::command::kick(user, true));
}

void ServerLogDialog::undoSelected()
{
	auto user = selectedUserId();
	if(user)
		emit opCommand(protocol::MessagePtr(new protocol::Undo(0, user, false)));
}

void ServerLogDialog::redoSelected()
{
	auto user = selectedUserId();
	if(user)
		emit opCommand(protocol::MessagePtr(new protocol::Undo(0, user, true)));
}

}

