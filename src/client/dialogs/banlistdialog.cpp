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

#include "banlistdialog.h"
#include "net/banlistmodel.h"

#include <QVBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>

namespace dialogs {

BanlistDialog::BanlistDialog(BanlistModel *model, bool isAdmin, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Banned users"));
	setMinimumWidth(400);

	auto *layout = new QVBoxLayout(this);

	auto *table = new QTableView(this);
	table->setModel(model);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->horizontalHeader()->setStretchLastSection(true);
	table->verticalHeader()->setVisible(false);
	layout->addWidget(table);

	auto *buttonbox = new QDialogButtonBox(QDialogButtonBox::Close, this);

	if(isAdmin) {
		buttonbox->addButton(tr("Remove"), QDialogButtonBox::ActionRole);
	}

	layout->addWidget(buttonbox);

	connect(buttonbox, &QDialogButtonBox::clicked, [this, buttonbox, table](QAbstractButton *btn) {
		if(buttonbox->buttonRole(btn) == QDialogButtonBox::RejectRole)
			this->reject();
		else if(buttonbox->buttonRole(btn) == QDialogButtonBox::ActionRole) {
			int id = table->selectionModel()->currentIndex().data(Qt::UserRole).toInt();
			if(id>0)
				emit requestBanRemoval(id);
		}
	});

	setLayout(layout);
}

}
