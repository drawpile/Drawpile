/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QPushButton>
#include <QDebug>

#include "dialogs/selectsessiondialog.h"
#include "net/loginsessions.h"
#include "utils/html.h"

#include "ui_selectsession.h"

namespace dialogs {

SelectSessionDialog::SelectSessionDialog(net::LoginSessionModel *model, const QString &serverTitle, QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_SelectSession;
	_ui->setupUi(this);

	QPushButton *ok = _ui->buttonBox->button(QDialogButtonBox::Ok);
	ok->setText(tr("Join"));
	ok->setEnabled(false);

	_ui->sessionView->setModel(model);
	_ui->serverTitle->setText(htmlutils::newlineToBr(htmlutils::linkify(serverTitle.toHtmlEscaped())));

	QHeaderView *header = _ui->sessionView->horizontalHeader();
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(0, QHeaderView::Fixed);
	header->resizeSection(0, 24);

	connect(this, &QDialog::accepted, [this, model]() {
		// Emit selection when OK is clicked
		int row = _ui->sessionView->selectionModel()->selectedIndexes().at(0).row();
		const net::LoginSession &s = model->sessionAt(row);
		emit selected(s.id, s.needPassword);
	});

	connect(_ui->sessionView->selectionModel(), &QItemSelectionModel::selectionChanged, [model, ok](const QItemSelection &sel) {
		// Enable/disable OK button depending on the selection
		if(sel.indexes().isEmpty()) {
			ok->setEnabled(false);
		} else {
			const net::LoginSession &s = model->sessionAt(sel.indexes().at(0).row());
			ok->setEnabled(!s.incompatible && !s.closed);
		}
	});

	connect(_ui->sessionView, &QTableView::doubleClicked, [this, ok](const QModelIndex &idx) {
		// Shortcut: double click to OK
		if(ok->isEnabled())
			accept();
	});
}

}
