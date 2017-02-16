/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#include "ui_serverlog.h"

#include <QSortFilterProxyModel>

namespace dialogs {

ServerLogDialog::ServerLogDialog(QWidget *parent)
	: QDialog(parent)
{
	m_ui = new Ui_ServerLogDialog;
	m_ui->setupUi(this);

	m_proxy = new QSortFilterProxyModel(this);
	m_ui->view->setModel(m_proxy);

	m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	connect(m_ui->filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
}

ServerLogDialog::~ServerLogDialog()
{
	delete m_ui;
}

void ServerLogDialog::setModel(QAbstractItemModel *model)
{
	m_proxy->setSourceModel(model);
}

}

